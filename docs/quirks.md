# Quirks & gotchas — symptom → cause → fix

A flat, greppable reference for the non-obvious things that bite when building
this TivaC / TI-RTOS firmware with **CMake + arm-none-eabi-gcc** (instead of
CCS + XDCtools + armcl). Each entry leads with the literal error so you can
search for it. Deeper write-ups are linked where they exist.

For the end-to-end story see [tivac-cmake-migration.md](tivac-cmake-migration.md);
for C-library locking specifically see [newlib-locking-port.md](newlib-locking-port.md).

---

## Toolchain / generation

### `CreateProcess error=193` (or configuro deletes its outputs)
**Cause:** Windows XDCtools `gmake.exe`/`xs.exe` cannot exec a Linux
`arm-none-eabi-gcc`. **Fix:** use the **Linux** XDCtools for the one-time
generation. (It also deletes `random_pm4fg.c`/`linker.cmd` on failure, so a
failed run looks like it produced nothing.)

### XDCtools `xs` can't find a JVM / Java errors at startup
**Cause:** XDCtools 3.32 (2015) probes the legacy JRE layout
(`jre/lib/amd64/server/libjvm.so`, classic `rt.jar`); a modular JDK (11/17/21)
lacks these, and symlink shims don't fully satisfy it. **Fix:** install the
XDCtools variant that **bundles its own JRE** (ships Java 8). Point `XDCTOOLS=` at
it (here: `/opt/ti/xdctools_3_32_00_06_core_jre/xdctools_3_32_00_06_core`).

### `xCon/sysbios/random.cfg:NNN TypeError: Cannot set property "loadAddress" of undefined`
**Cause:** the gnu target doesn't pre-populate `Program.sectMap[".resetVecs"]`.
**Fix:** guard it in `xCon/sysbios/random.cfg`:
```js
if (Program.sectMap[".resetVecs"] === undefined) {
    Program.sectMap[".resetVecs"] = new Program.SectionSpec();
}
```

### gcc chokes on `BIOS.customCCOpts` (`-mv7M4 --abi=eabi` ...)
**Cause:** those are armcl flags. **Fix:** wrap the assignment in
`if (Program.build.target.$name.match(/ti\.targets/)) { ... }`.

---

## Compiling the kernel / SDK from source

### No prebuilt SYS/BIOS archives (only `.mak` recipes)
**Cause:** this SYS/BIOS distribution ships **source only**. **Fix:** compile the
kernel from source — configuro selects a 29-file subset, mirrored in
`xCon/sysbios/sysbios.cmake`. (Drivers/WiFi/RTS *do* ship prebuilt `.am4fg`.)

### `Timer.c`: `ti/catalog/arm/peripherals/timers/timer.h: No such file`
**Cause:** kernel source needs `ti/catalog` headers. **Fix:** vendor them
(`vendor-sdk.sh` copies `ti/catalog/**.h`).

### `ReentSupport.c`: `request for member 'init_done' in something not a structure`
**Cause:** a non-functional compile-time canary (`volatile _LOCK_T lock;
lock.init_done = 1;`) assumes the old newlib `_LOCK_T` struct; modern newlib makes
it opaque. **Fix:** neutralized (vendored copy + idempotent SDK patch in
`generate-bios-config.sh`). See [newlib-locking-port.md](newlib-locking-port.md).

### `random_pm4fg.c`: `request for member 'sem' in something not a structure`
**Cause:** the generated config emits pre-newlib-3.0 `__libc_lock_*` glue over a
struct `_LOCK_T`; gcc 13.2's newlib (built without `_RETARGETABLE_LOCKING`) makes
`_LOCK_T` an `int` and never calls those hooks. **Fix:** the block is `#if 0`'d
(idempotent in `generate-bios-config.sh`); real locking lives in
`xCon/sysbios/newlib_locks.c`. Full story: [newlib-locking-port.md](newlib-locking-port.md).

### `multiple definition of __sfp_lock_acquire` / `__sfp_lock_release`
**Cause:** unlike `__malloc_lock`/`__env_lock`/`__tz_lock` (each in its own
`libc.a` object, so overridable), newlib keeps `__sfp_lock_*`/`__sinit_lock_*` in
`findfp.o` *with* essential stdio that's always linked — so redefining them is a
collision, not an override. **Fix:** don't override them; `xCon/sysbios/newlib_locks.c`
provides only malloc/env/tz.

---

## Case sensitivity (WSL/Linux vs Windows CCS)

### `fatal error: board.h` / `ti/sysbios/knl/clock.h: No such file`
**Cause:** Windows CCS is case-insensitive; WSL is not. Real files are `Board.h`
and `Clock.h`. **Fix:** correct the includes in the app sources to exact case.
(Run a case-audit if you add more SDK includes.)

---

## Unused stock-board drivers pulling missing deps

### `EMACSnow.h -> ti/ndk/inc/stkmain.h: No such file`
**Cause:** the stock board file (`EK_TM4C129EXL.c`) configures the wired-Ethernet
EMAC driver, which needs the **NDK** (omitted — this board uses CC3200 WiFi).
**Fix:** the EMAC/NIMU block is guarded behind `#if defined(BOARD_USE_NDK)` (off
by default; no app callers). To actually add NDK: [adding-ndk.md](adding-ndk.md).

### `SDSPITiva.h -> ti/mw/fatfs/ff.h: No such file`
**Cause:** the board file also declares SD-card (SDSPI) and USB mass-storage
(USBMSCHFatFs) drivers, which need the **FatFs** library (not vendored, not used).
**Fix:** both blocks are guarded behind `#if defined(BOARD_USE_FATFS)` (off by
default).

---

## SimpleLink vs newlib

### `redefinition of 'struct SlTimeval_t'` / `conflicting types for 'select'`
**Cause:** SimpleLink's `socket.h` BSD-naming layer (`SL_INC_STD_BSD_API_NAMING`,
forced on) does `#define timeval SlTimeval_t`, `#define fd_set SlFdSet_t`, and
defines its own `select()`. Pulling in newlib's `<sys/select.h>` in the same TU
collides. The app **uses BSD socket names**, so SimpleLink's layer must stay on.
**Fix:** don't pull newlib's `<sys/select.h>`. In `xCon/app/main.c` that meant removing
an **unused** `#include <time.h>` (which chained `time.h → sys/types.h →
sys/select.h`). Time comes from SYS/BIOS `Seconds`.

---

## Linking the application

### `memory region 'REGION_TEXT' not declared`
**Cause:** `xCon/generated/linker.cmd` is only a **fragment** — no `MEMORY{}`; it
references `REGION_TEXT`/`REGION_DATA` from a base script. **Fix:** added
`xCon/linker/tm4c129encpdt.lds` (FLASH 1 MB @ 0x0, SRAM 256 KB @ 0x20000000, REGION
aliases, standard sections), adapted from TI's `DK_TM4C129X.lds`.

### `linker.cmd: contains output sections; did you forget -T?`
**Cause:** CMake **de-duplicates** identical repeated link flags, so passing
`-T base.lds -T linker.cmd` dropped the second `-T` and `linker.cmd` became an
input file. **Fix:** use the comma form — `-Wl,-T,base.lds` and
`-Wl,-T,linker.cmd` are distinct tokens and survive dedup. Order matters: **base
`.lds` first**, then the configuro fragment (mirrors TI-RTOS `makedefs`).

### Hundreds of `multiple definition of ti_sysbios_* / xdc_runtime_*`
**Cause:** `linker.cmd`'s `INPUT()` block pulls the configuro-built
`random_pm4fg.om4fg` + `sysbios.am4fg`, which duplicate our from-source
`random_pm4fg.c.obj` + `libsysbios.a` (and uses absolute `/mnt/c/ti` paths).
**Fix:** strip the `INPUT()` block (idempotent in `generate-bios-config.sh`);
CMake provides all objects/libs from `xCon/third_party/`.

### `undefined symbol 'xdc_runtime_System_asprintf_va__F'` (and the other `__F`)
**Cause:** the config provides the `__E` printf entry points but their `__F`
bodies live in **`gnu.targets.arm.rtsv7M.am4fg`** (the gnu RTS = TI's build of the
xdc.runtime for ARMv7M) — one of the libs the stripped `INPUT()` used to pull.
**Fix:** vendor and link the gnu RTS set — **`gnu.targets.arm.rtsv7M.am4fg`**
(xdc.runtime bodies), `boot.am4fg` (`_c_int00`), `syscalls.am4fg` — plus the
TivaC catalog `Boot.am4fg` (SysCtl clock init). They're in `SDK_PREBUILT_LIBS`
and `vendor-sdk.sh`.

### `undefined reference to 'CPUcpsie'` / `CPUcpsid` (from `interrupt.c`)
**Cause:** TivaWare `cpu.c` selects its per-compiler inline assembly with the
**legacy `gcc` macro**, not `__GNUC__`:
`#if defined(codered) || defined(gcc) || defined(sourcerygxx)`. arm-none-eabi-gcc
doesn't define `gcc`, so `cpu.c` compiled to an **empty object**. **Fix:**
`target_compile_definitions(tivaware PRIVATE gcc)` (TivaWare's own makefiles pass
`-Dgcc`).

### `undefined reference to 'initialise_monitor_handles'` (from `SemiHostSupport.c`)
**Cause:** the BIOS gnu `SemiHostSupport` module uses **semihosting**;
`initialise_monitor_handles` is in `librdimon`, not `libnosys`.
**Original fix (superseded):** link `-lrdimon`. This *links* but stalls at
runtime — semihosting `_write`/`_sbrk` traps need a debugger attached, so a
standalone board hangs on the first `printf()`/`malloc()`.
**Current fix:** link **`-lnosys`** and retarget C-library I/O to **UART0** (the
ICDI virtual COM port) in `xCon/sysbios/syscalls_uart.c`: override `_write`→UART0,
`_isatty`/`_fstat` (line-buffered stdout), a no-op `initialise_monitor_handles`,
and a guard `_sbrk` (malloc is BIOS-backed). No config regeneration needed. Full
write-up: [uart-console-retarget.md](uart-console-retarget.md).

### `printf()` works under the debugger but a standalone board hangs / prints nothing
**Cause:** C-library I/O still on semihosting (`-lrdimon` + `SemiHostSupport`) —
the `BKPT`/`SVC` traps are serviced only by an attached debugger. **Fix:** the
UART0 retarget above (`-lnosys` + `xCon/sysbios/syscalls_uart.c`). After it, output needs
UART0 up: `printf` before `UARTStdioConfig(0,...)` is dropped by the `UARTEN`
gate (move the config into board init for earliest boot logs).

### `random.bin` is ~512 MB
**Cause:** SYS/BIOS's RAM vector table section `.vtable` has its **LMA in SRAM**
(`0x20000000`); `objcopy -O binary` then spans the entire flash→SRAM gap. The
section is zero-filled and built at runtime by `Hwi_init`. **Fix:**
`objcopy -O binary -R .vtable ...`. The `.out` and `.hex` are unaffected.
