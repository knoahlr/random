# Migrating a TivaC / TI-RTOS (SYS/BIOS) project from CCS to CMake

The end-to-end procedure used to move this firmware off Code Composer Studio +
XDCtools (RTSC managed build, TI `armcl`) to a **self-contained CMake build** with
`arm-none-eabi-gcc` in WSL — no CCS and no XDCtools at build time. It doubles as a
playbook for any TivaC + TI-RTOS project of this era.

Related docs:
- [quirks.md](quirks.md) — flat, greppable symptom → cause → fix index for every
  error hit along the way. Reach for it when something breaks.
- [regenerating-sysbios-config.md](regenerating-sysbios-config.md) — the narrow
  "how do I regenerate the kernel config" task.
- [newlib-locking-port.md](newlib-locking-port.md) — the C-library locking port.
- [uart-console-retarget.md](uart-console-retarget.md) — standalone C-library I/O
  (printf/malloc) on UART0 instead of semihosting.
- [flashing-and-console.md](flashing-and-console.md) — flashing with `lm4flash`
  and viewing UART prints, from Windows PowerShell or WSL.
- [wsl_flashing.md](wsl_flashing.md) — sharing the ICDI USB device with
  `usbipd-win` and attaching it to WSL.
- [adding-ndk.md](adding-ndk.md) — adding the wired TCP/IP stack later.

---

## 0. Start point and goal

**Start:** TM4C129ENCPDT firmware. TI-RTOS bundle `tirtos_tivac_2_16_00_08`
(SYS/BIOS `bios_6_45_01_29`, TI-drivers, SimpleLink **CC3200 WiFi** host driver),
TivaWare `2.2.0.295`, configured by `xCon/sysbios/random.cfg` and built by CCS via
XDCtools `3.32.00.06` (RTSC) with the TI `armcl` compiler.

**Goal:** a fresh WSL/Linux machine clones the repo and builds with **only**
`cmake`, `ninja`, and `arm-none-eabi-gcc`. No TI install required at build time.

**Strategy (the key decisions):**
1. **GNU compiler** (`arm-none-eabi-gcc`), target `gnu.targets.arm.M4F`.
2. **Drop XDCtools from the build loop** via *generate-once-then-freeze*: run
   `configuro` exactly once to emit the kernel config C + linker fragment, commit
   them, and never run XDCtools to build again.
3. **Build SYS/BIOS from source** (it ships source-only anyway — see §3.1).
4. **Link the other SDK pieces as prebuilt GNU libraries** (`.am4fg`).
5. **Vendor** the needed SDK subset into `xCon/third_party/` so the repo is
   self-contained.

---

## 1. The dependency map (know this before you start)

| Component | How it's used | In the CMake build |
| --- | --- | --- |
| **TivaWare** driverlib + `uartstdio` | plain C | compiled from source (needs `-Dgcc`) |
| **SYS/BIOS** kernel | RTSC package, **source-only** | 29 files compiled from source (configuro picks them) |
| **xdc.runtime** | headers + runtime bodies | headers vendored; module bodies come from the gnu RTS lib |
| **ti.drivers** (GPIO/PWM/WiFi), **ports** | RTSC packages | **prebuilt** `.am4fg` linked |
| **CC3x00 WiFi host driver** | RTSC, **prebuilt-only** (no source ships) | **prebuilt** `.am4fg` linked |
| **gnu RTS** (`rtsv7M`/`boot`/`syscalls`) + catalog `Boot` | RTSC | **prebuilt** `.am4fg` linked |
| **NDK**, **FatFs** | wired networking, filesystem | **omitted / guarded out** (unused; CC3200 has sockets) |
| **XDCtools** | code generator | **one-time only**, not at build time |

---

## 2. Prerequisites (one-time host setup)

In WSL:
```bash
sudo apt-get install cmake ninja-build gcc-arm-none-eabi
```

For the single generation step you also need the **Linux XDCtools, same version
as the SDK (`3_32_00_06`), the variant WITH a bundled JRE** (the stock one can't
find a modern JVM — see [quirks.md](quirks.md)). The SDK itself can stay on the
Windows side under `/mnt/c/ti`.

---

## 3. Procedure

### 3.1 Why SYS/BIOS must be built from source

This SYS/BIOS distribution ships **source only** — there are no prebuilt kernel
archives, just `.mak` recipes. `configuro` selects a config-specific subset (here
**29 files**: 25 `.c` + 4 `.sv7M` GNU-assembly) and builds a custom `sysbios`
library. We mirror that exact file list + its `-D` feature defines into CMake
(`xCon/sysbios/sysbios.cmake`). The driver/WiFi/RTS pieces, by contrast, **do** ship
prebuilt GNU `.am4fg` archives, so we link those.

### 3.2 Make `xCon/sysbios/random.cfg` target-agnostic (TI → GNU)

Stock TI `.cfg` files assume `armcl`. Two edits are required for the gnu target
(already applied here):

```js
// 1. The gnu target doesn't pre-populate this section entry.
if (Program.sectMap[".resetVecs"] === undefined) {
    Program.sectMap[".resetVecs"] = new Program.SectionSpec();
}
Program.sectMap[".resetVecs"].loadAddress = 0;

// 2. customCCOpts holds armcl flags gcc can't parse — TI targets only.
if (Program.build.target.$name.match(/ti\.targets/)) {
    BIOS.customCCOpts = "--endian=little -mv7M4 --abi=eabi ...";
}
```

### 3.3 Vendor the SDK subset

`tools/vendor-sdk.sh` rsyncs the pruned dependency set into `xCon/third_party/`
(headers + the GNU `.am4fg` libs + TivaWare/SYS-BIOS sources). It's reproducible;
re-run only to refresh against a new SDK. NDK is intentionally excluded.

### 3.4 Patch the newlib reentrancy canary

`ti/sysbios/rts/gnu/ReentSupport.c` contains a non-functional compile-time canary
(`volatile _LOCK_T lock; lock.init_done = 1;`) that assumes the **old newlib lock
layout**. Modern newlib (gcc 13.x) makes `_LOCK_T` opaque, so it no longer
compiles. It is neutralized in two places:
- the vendored copy (`xCon/third_party/.../ReentSupport.c`) — used by the CMake build;
- the SDK copy, applied idempotently by `tools/generate-bios-config.sh` — used by
  configuro's internal library build.

This is one facet of a larger C-library locking mismatch; the full port is in
[newlib-locking-port.md](newlib-locking-port.md).

### 3.5 One-time generation with `configuro`

```bash
XDCTOOLS=/opt/ti/<xdctools-with-jre>/xdctools_3_32_00_06_core \
  ./tools/generate-bios-config.sh
```

This runs configuro (see the script for the full invocation) and, after it,
applies two idempotent post-generation patches: disabling the obsolete
`__libc_lock_*` block and stripping the `linker.cmd` `INPUT()` block (both
explained in [quirks.md](quirks.md)).

Outputs that matter:
- `xCon/generated/package/cfg/random_pm4fg.c` / `.h` — the static kernel config.
- `xCon/generated/linker.cmd` — section + symbol fragment (no `MEMORY{}`; see §3.7).
- `xCon/generated/compiler.opt` — reference include/define set.

### 3.6 Freeze

Force-add those artifacts (the rest of `xCon/generated/` is gitignored intermediate
output) and commit. XDCtools is now out of the build loop.

### 3.7 Build with CMake

```bash
cmake --preset arm-gcc        # toolchain-arm-none-eabi.cmake, Ninja
cmake --build --preset arm-gcc
```

CMake structure:
- `xCon/cmake/toolchain-arm-none-eabi.cmake` — Cortex-M4F flags.
- `xCon/sysbios/sysbios.cmake` — the 29 kernel files + `SYSBIOS_FEATURE_DEFS`.
- `CMakeLists.txt` — `sdk_config` (shared includes/defines) → `sysbios` +
  `tivaware` from source → link prebuilt `.am4fg` → `random.out` (gated on the
  frozen `xCon/generated/` config) → `objcopy` to `.bin`/`.hex`.

**The link model.** `xCon/generated/linker.cmd` is only a *fragment* — it has the
`.resetVecs`/`xdc.meta` sections, symbol aliases, and `ENTRY(_c_int00)`, but **no
`MEMORY{}`**. So the link uses two `-T` scripts, base first:

```
-Wl,-T,xCon/linker/tm4c129encpdt.lds   # MEMORY map + REGION aliases + standard sections
-Wl,-T,xCon/generated/linker.cmd       # the configuro fragment, layered on top
```

`tm4c129encpdt.lds` (FLASH 1 MB @ 0x0, SRAM 256 KB @ 0x20000000) is adapted from
the TI-RTOS GNU example `DK_TM4C129X.lds`; the two-script order mirrors TI-RTOS
`makedefs`. All objects and libraries are provided by CMake from `xCon/third_party/`
(not via the fragment's old `INPUT()` block), keeping the build self-contained.
The link group is: from-source `sysbios` + `tivaware`, the prebuilt driver/WiFi/
ports/RTS/Boot `.am4fg`, then `-lc -lgcc -lm -lnosys`. (`-lnosys`, not the
semihosting `-lrdimon`: C-library I/O is retargeted to UART0 — see
[uart-console-retarget.md](uart-console-retarget.md).)

Result: `random.out` (+ `.bin`/`.hex`), entry `_c_int00`, `.intvecs` at `0x0`,
`.data`/`.bss`/`.stack` in SRAM; ~109 KB flash / ~72 KB SRAM.

The individual link gotchas (`REGION_TEXT`, the `-T` de-dup, the `INPUT()`
duplicates, the `rtsv7M.am4fg` / `CPUcpsie` / `initialise_monitor_handles` /
`random.bin`-512 MB issues) are each catalogued in [quirks.md](quirks.md).

---

## 4. Design caveats (not bugs — keep in mind)

- **Prebuilt-library ABI vintage.** The vendored `.am4fg` archives (drivers,
  WiFi, gnu RTS, Boot) were built with **GCC ARM 4.8.4**; we build with **13.2**.
  Cortex-M4F AAPCS is stable, but newlib/libgcc edges can bite. The CC3x00 WiFi
  driver is **prebuilt-only** (no source), so this can't be fully escaped — a
  watch-item for link/runtime behaviour.
- **C-library I/O is on UART0, not semihosting.** The build links `-lnosys` and
  `xCon/sysbios/syscalls_uart.c` retargets `_write`/`_isatty`/`_sbrk` (+ a no-op
  `initialise_monitor_handles`) to UART0, so `printf`/`malloc` run with no
  debugger. Output appears once `UARTStdioConfig(0,...)` has run. See
  [uart-console-retarget.md](uart-console-retarget.md).

---

## 5. Status & still open

The full firmware **links end-to-end**; `random.out`/`.bin`/`.hex` are produced
from a clean tree. Still to do (none block the build):

- Remove the legacy CCS files (`.cproject`, `.ccsproject`, `.project`,
  `makefile.defs`, `.config/`, `targetConfigs/`). SYS/BIOS support code now
  lives under `xCon/sysbios/`.
- Clean unused vars from `CMakePresets.json` (`TI_ROOT`/`TIRTOS_INSTALL`/
  `TIVAWARE_INSTALL`).

(C-library I/O retargeting — formerly open here — is **done**: standalone UART0,
[uart-console-retarget.md](uart-console-retarget.md).)

---

## 6. Maintenance

Regenerate (§3.5) **only** when you change kernel configuration in `xCon/sysbios/random.cfg`
(tick period, heap, statically-defined Hwis, the set of BIOS modules). Ordinary
app changes — adding tasks via `Task_create`, editing drivers — never require it.
After regenerating, re-sync the source list/feature defines in `xCon/sysbios/sysbios.cmake`
if the module set changed, then rebuild.
