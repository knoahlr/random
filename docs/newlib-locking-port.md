# Porting SYS/BIOS C-library (newlib) locking to arm-none-eabi-gcc 13.2

This documents a non-obvious incompatibility hit during the CMake/GNU migration
and the fix applied. Read it before touching `xCon/sysbios/newlib_locks.c`, the `#if 0`
block in the generated config, or upgrading the toolchain / SYS/BIOS.

## TL;DR

- SYS/BIOS `bios_6_45_01_29` (2015) provides C-library thread-safety glue written
  against a **pre-2018 newlib** ABI: the `__libc_lock_*` hooks over a *struct*
  `_LOCK_T { sem, init_done, owner, count }`.
- The newlib in **arm-none-eabi-gcc 13.2** does not use that API. It is built
  **without `_RETARGETABLE_LOCKING`**, so `_LOCK_T` is `int`, the `__libc_lock_*`
  and modern `__retarget_lock_*` hooks are **never called**, and the C library
  instead calls a small set of older **reent-model** global hooks.
- Result: the generated `__libc_lock_*` code is dead *and* doesn't compile
  (`lock->sem` on an `int`). We **disable** it and provide the hooks this newlib
  actually calls, backed by a BIOS `GateMutex`, in `xCon/sysbios/newlib_locks.c`.

## Why "compiled with the same gcc" doesn't make it compatible

The incompatibility is **not** a compiler or binary/ABI mismatch — same gcc, same
AAPCS. It is a **C-library (newlib) interface-version** mismatch:

- **gcc the compiler** emits Cortex-M4F code; identical for every file.
- **newlib** ships *with* the toolchain and exposes a set of **threading hooks**
  the RTOS is expected to implement.
- **SYS/BIOS RTS glue** (`ti.sysbios.rts.gnu.ReentSupport`) exists only to plug
  into those hooks.

newlib changed its hook contract across versions. The SDK glue `#include`s
`<sys/lock.h>` / `<reent.h>` from whatever newlib the toolchain ships, and defines
functions against the **2015** contract. gcc 13.2's newlib defines `_LOCK_T`
differently, so `lock->sem` becomes "member of an `int`." Compiling with a newer
gcc fixes code generation but cannot reconcile glue code that reaches into
**newlib internals that no longer exist in that form**.

(This is a *different* axis from the prebuilt `.am4fg` driver/WiFi libraries,
where the concern genuinely is old-gcc-4.8.4 binary vintage. The lock issue is a
pure from-source API drift.)

## The three newlib locking generations

| Era | `_LOCK_T` | Hooks newlib calls | Relevant to us |
| --- | --- | --- | --- |
| ≤ 2015 (pre-newlib-3.0) | TI struct `{sem,init_done,owner,count}` | `__libc_lock_init/acquire/release/...` | **what the SDK emits** (now dead) |
| **gcc 13.2 newlib (retargetable OFF)** | `int` | `__malloc_lock`, `__env_lock`, `__tz_lock`, `__sfp_lock_acquire`, `__sinit_lock_acquire` (+ per-task `__getreent`) | **what we must target** |
| newlib 3.0+ retargetable | opaque `struct __lock *` | `__retarget_lock_init/acquire/...` | only if newlib built `--enable-newlib-retargetable-locking` |

### How this was determined

```sh
# 1) _LOCK_T resolved to int, not a struct, in the toolchain header:
arm-none-eabi-gcc -mcpu=cortex-m4 -E -P - <<<'#include <sys/lock.h>
#ifdef _RETARGETABLE_LOCKING
RETARGETABLE_ON
#else
RETARGETABLE_OFF
#endif'
#   -> RETARGETABLE_OFF

# 2) libc.a references the reent-model hooks, not __libc_lock_* / __retarget_lock_*:
LIBC=$(arm-none-eabi-gcc -mcpu=cortex-m4 -mthumb -mfloat-abi=hard \
        -mfpu=fpv4-sp-d16 -print-file-name=libc.a)
arm-none-eabi-nm "$LIBC" | grep -iE 'lock'
#   -> __malloc_lock, __env_lock, __tz_lock, __sfp_lock_acquire, ...
```

ARM's official 13.2 build (`13.2.rel1`) is retargetable-OFF, so implementing
`__retarget_lock_*` would compile but never be called. The reent-model hooks are
the ones to implement.

## The fix

### 1. Disable the obsolete glue in the generated config

`xCon/generated/package/cfg/random_pm4fg.c` contains the
`ti.sysbios.rts.gnu.ReentSupport TEMPLATE` block. We keep `__getreent()` (it is
correct for this newlib — gives each BIOS Task its own `_reent`: per-task `errno`,
stdio buffers, etc.) and wrap only the `__libc_lock_*` functions in `#if 0`.

Because this file is generated, the same edit is re-applied **idempotently** by
`tools/generate-bios-config.sh` after any regeneration (a `perl -0777` wrap keyed
on the `__libc_lock_init` signature and the following `SemiHostSupport TEMPLATE`
banner). If you regenerate by hand, re-apply it or re-run that script.

### 2. Implement the hooks this newlib actually calls

`xCon/sysbios/newlib_locks.c` overrides newlib's default no-op global lock hooks with a
single recursive BIOS `GateMutex`:

| Hook | Guards |
| --- | --- |
| `__malloc_lock` / `__malloc_unlock` | the heap (`malloc`/`free`/`realloc`) |
| `__env_lock` / `__env_unlock` | the environment (`getenv`/`setenv`) |
| `__tz_lock` / `__tz_unlock` | timezone state |

Design notes (see the source for detail):

- **Override mechanism:** newlib keeps each default hook in its own `libc.a`
  object, pulled in only to satisfy an undefined reference. Defining the symbols
  in our object means the linker never pulls newlib's defaults — no
  multiple-definition conflict.
- **`__sfp_lock_*` / `__sinit_lock_*` are NOT overridden.** Those two are the
  exception to the override mechanism: newlib keeps them in `findfp.o` *alongside*
  essential stdio (`__sfp`, `__sinit`, the std streams), which is always linked —
  so defining our own yields a **multiple-definition link error**, not an
  override. We accept newlib's defaults (no-ops here). The shared stdio `FILE`
  list is therefore not gated by a BIOS lock; per-task stdio state is still
  isolated via `__getreent()`, so only concurrent `fopen`/`fclose` of *different*
  streams from multiple tasks is unguarded. (This was found at link time, after
  the initial version tried to override all five classes.)
- **One coarse gate** for all classes. Simpler than per-class gates; newlib does
  not nest distinct global locks deeply and contention is negligible. `GateMutex`
  is recursive, so same-task re-entry is safe.
- **Lazy construction** guarded by `Hwi_disable()` so a first-use race is safe.
- **Thread-type gating:** only `BIOS_ThreadType_Task` actually takes the gate.
  Before the scheduler runs (Main) the system is single-threaded; the C library
  must not be called from Hwi/Swi context. The thread type is constant across a
  matched lock/unlock pair, so enter/leave agree on whether the gate was taken.
- **Per-class saved keys** are file-scope statics, written only while the gate is
  held, so they need no extra protection.

### 3. Wire into CMake

`xCon/sysbios/newlib_locks.c` is added to `APP_SOURCES` in `CMakeLists.txt`.

## Verification

`xCon/generated/package/cfg/random_pm4fg.c` and `xCon/sysbios/newlib_locks.c` both compile
cleanly under gcc 13.2 (`cmake --build --preset arm-gcc`). The earlier
`request for member 'sem' in something not a structure` errors are gone.

(The full `random.out` link is still gated on unrelated, later work — the TM4C129
MEMORY map and a few application-source porting issues; see
`docs/tivac-cmake-migration.md` §5.)

## To revisit later

- **Want genuine `__retarget_lock_*` instead?** You would need a newlib built with
  `_RETARGETABLE_LOCKING` (a custom newlib, or a toolchain that enables it). Then
  define `struct __lock`, the static `__lock___*_mutex` instances, and the ten
  `__retarget_lock_*` functions over BIOS gates. More surface area; not required
  on the current toolchain.
- **A newer SYS/BIOS** may emit modern retarget glue for you, but it is a large
  migration (new TI-RTOS bundle, module/API changes, re-verify the CC3200
  SimpleLink driver + platform packages, full regeneration) and is *still* gated
  on the toolchain's newlib model. Not worth it versus this ~40-line file.
- **Granularity:** if profiling ever shows the single coarse gate is a
  bottleneck, split into per-class gates.
