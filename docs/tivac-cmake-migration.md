# Migrating a TivaC / TI-RTOS (SYS/BIOS) project from CCS to CMake

This is the end-to-end procedure used to move this firmware off Code Composer
Studio + XDCtools (RTSC managed build, TI `armcl` compiler) to a **self-contained
CMake build** using `arm-none-eabi-gcc` in WSL — no CCS and no XDCtools at build
time. It doubles as a playbook for any TivaC + TI-RTOS project of this era.

For the narrower "how do I regenerate the kernel config" task, see
[regenerating-sysbios-config.md](regenerating-sysbios-config.md). This document
is the whole journey, including every dead end, so you can reproduce or adapt it.

---

## 0. What we started with / what we wanted

**Start:** TM4C129ENCPDT firmware. TI-RTOS bundle `tirtos_tivac_2_16_00_08`
(SYS/BIOS `bios_6_45_01_29`, TI-drivers, SimpleLink **CC3200 WiFi** host driver),
TivaWare `2.2.0.295`, configured by `random.cfg` and built by CCS via
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
5. **Vendor** the needed SDK subset into `third_party/` so the repo is
   self-contained.

---

## 1. The dependency map (know this before you start)

| Component | How it's used | In the CMake build |
| --- | --- | --- |
| **TivaWare** driverlib + `uartstdio` | plain C | compiled from source |
| **SYS/BIOS** kernel | RTSC package, **source-only** | 29 files compiled from source (configuro picks them) |
| **xdc.runtime** | headers + part of kernel | headers vendored; compiled into kernel |
| **ti.drivers** (GPIO/PWM/WiFi), **ports** | RTSC packages | **prebuilt** `.am4fg` linked |
| **CC3x00 WiFi host driver** | RTSC, **prebuilt-only** (no source ships) | **prebuilt** `.am4fg` linked |
| **gnu RTS** (`rtsv7M`/`boot`/`syscalls`), catalog `Boot`, `fatfs` | RTSC | **prebuilt** `.am4fg` linked |
| **NDK** | networking | **omitted** (SimpleLink provides sockets) |
| **XDCtools** | code generator | **one-time only**, not at build time |

---

## 2. Prerequisites (one-time host setup)

In WSL:
```bash
sudo apt-get install cmake ninja-build gcc-arm-none-eabi
```

For the single generation step you also need the **Linux XDCtools, same version
as the SDK (`3_32_00_06`), the variant WITH a bundled JRE** (critical — see §5).
The SDK itself can stay on the Windows side under `/mnt/c/ti`.

---

## 3. Procedure

### 3.1 Understand why SYS/BIOS must be built from source

This SYS/BIOS distribution ships **source only** — there are no prebuilt kernel
archives, just `.mak` recipes. `configuro` selects a config-specific subset (here
**29 files**: 25 `.c` + 4 `.sv7M` GNU-assembly) and builds a custom `sysbios`
library. We mirror that exact file list + its `-D` feature defines into CMake
(`cmake/sysbios.cmake`). The driver/WiFi/RTS pieces, by contrast, **do** ship
prebuilt GNU `.am4fg` archives, so we link those.

### 3.2 Make `random.cfg` target-agnostic (TI → GNU)

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

`tools/vendor-sdk.sh` rsyncs the pruned dependency set into `third_party/`
(headers + the GNU `.am4fg` libs + TivaWare/SYS-BIOS sources). It's reproducible;
re-run only to refresh against a new SDK. Result is ~74 MB (mostly TivaWare `inc`
headers). NDK is intentionally excluded.

### 3.4 Patch the newlib reentrancy canary

`ti/sysbios/rts/gnu/ReentSupport.c` contains a non-functional compile-time canary
(`volatile _LOCK_T lock; lock.init_done = 1;`) that checks for a pre-Java-9-era…
no — for the **old newlib lock layout**. Modern newlib (gcc 13.x) makes `_LOCK_T`
opaque, so it no longer compiles. Neutralize it (it does nothing functional). The
patch is applied in two places:
- the vendored copy (`third_party/.../ReentSupport.c`) — used by the CMake build;
- the SDK copy, applied idempotently by `tools/generate-bios-config.sh` — used by
  configuro's internal library build.

### 3.5 One-time generation with `configuro`

```bash
XDCTOOLS=/opt/ti/<xdctools-with-jre>/xdctools_3_32_00_06_core \
  ./tools/generate-bios-config.sh
```

This runs (see the script for the full invocation):
```
xs --xdcpath="<bios>;<tidrivers>;<ndk>;<xdctools>/packages" \
   xdc.tools.configuro -c /usr -t gnu.targets.arm.M4F \
   -p ti.platforms.tiva:TM4C129ENCPDT -r release -o generated random.cfg
```

Outputs that matter:
- `generated/package/cfg/random_pm4fg.c` / `.h` — the static kernel config.
- `generated/linker.cmd` — section + library-`INPUT()` fragment.
- `generated/compiler.opt` — reference include/define set.

### 3.6 Freeze

Force-add the four artifacts above (the rest of `generated/` is gitignored
intermediate output) and commit. XDCtools is now out of the build loop.

### 3.7 Build with CMake

```bash
cmake --preset arm-gcc        # toolchain-arm-none-eabi.cmake, Ninja
cmake --build --preset arm-gcc
```

CMake structure:
- `cmake/toolchain-arm-none-eabi.cmake` — Cortex-M4F flags.
- `cmake/sysbios.cmake` — the 29 kernel files + `SYSBIOS_FEATURE_DEFS`.
- `CMakeLists.txt` — `sdk_config` (shared includes/defines) → `sysbios` +
  `tivaware` from source → link prebuilt `.am4fg` → `random.out` (gated on the
  frozen `generated/` config) → `objcopy` to `.bin`/`.hex`.

---

## 4. Dead ends & gotchas (so you don't repeat them)

1. **Windows XDCtools cannot drive a Linux compiler.** Its `gmake.exe` execs the
   Linux `arm-none-eabi-gcc` and fails with `CreateProcess error=193`. Use the
   **Linux** XDCtools. (And it deletes `random_pm4fg.c`/`linker.cmd` on failure,
   so partial runs look like they produced nothing.)
2. **No prebuilt SYS/BIOS libs** in this distribution → the kernel *must* be
   compiled; there's no "just link a lib" shortcut for it.
3. **gcc 13.2 vs the prebuilt `4.8.4` libs.** The vendored `.am4fg` archives were
   built with GCC ARM 4.8.4. Cortex-M4F AAPCS is stable, but newlib/libgcc
   differences can bite. The CC3x00 WiFi driver is **prebuilt-only** (no source),
   so you cannot fully escape this — a watch-item for link/runtime.
4. **Modern newlib `_LOCK_T` is opaque** → the `ReentSupport.c` canary breaks
   (§3.4). Isolated to one cosmetic line.
5. **XDCtools 3.32 (2015) needs a pre-Java-9 JRE.** A modern modular JDK (Java
   11/17/21) fails: the `xs` launcher probes the legacy `jre/lib/amd64/server`
   layout and a classic `rt.jar`, which modular runtimes lack. Symlink shims
   don't fully satisfy it. **Fix: install the XDCtools variant that bundles its
   own JRE** (ships a compatible Java 8 in `<xdctools>/jre/`). This is why the
   `XDCTOOLS=` path matters.
6. **`linker.cmd` is a fragment, not a complete script** — it has no `MEMORY{}`
   block and references `REGION_TEXT` defined by the target's base linker
   template. See §5 (remaining work).

---

## 5. The application link (now complete)

`random.out` links end-to-end and emits `.bin`/`.hex`. The pieces that got it there:

- **Memory map.** `generated/linker.cmd` is only a *fragment* (no `MEMORY{}`;
  references `REGION_TEXT`/`REGION_DATA`). We added a base GNU script,
  `linker/tm4c129encpdt.lds` (FLASH 1 MB @ 0x0, SRAM 256 KB @ 0x20000000, REGION
  aliases, standard sections), adapted from the TI-RTOS GNU example
  `DK_TM4C129X.lds`. Both are passed with `-Wl,-T,` — **base `.lds` first, then
  `linker.cmd`** — mirroring TI-RTOS `makedefs`. (Use the comma form so CMake
  doesn't de-duplicate the repeated `-T`.)
- **Strip `linker.cmd`'s `INPUT()` block.** It pulled the configuro-built
  `random_pm4fg.om4fg` + `sysbios.am4fg` (duplicating our from-source objects) and
  referenced libs by absolute `/mnt/c/ti` paths. We provide everything from
  `third_party/` via CMake; `generate-bios-config.sh` strips the block
  idempotently after regeneration.
- **Vendor + link the gnu RTS set:** `gnu.targets.arm.rtsv7M` (xdc.runtime bodies
  like `System_*printf_va__F`), `boot` (`_c_int00`), `syscalls`, and the TivaC
  catalog `Boot` (SysCtl clock init). Added to `SDK_PREBUILT_LIBS` +
  `tools/vendor-sdk.sh`. (`ti.mw.fatfs` was *not* needed — the SD/USB-FatFs board
  drivers are guarded out; see `docs/adding-ndk.md`.)
- **`-Dgcc` for TivaWare.** `cpu.c` selects its inline-asm (`CPUcpsie`/`CPUcpsid`)
  with the legacy `gcc` macro, not `__GNUC__`; without it `cpu.c` compiles empty.
- **`-lrdimon` not `-lnosys`.** The BIOS `SemiHostSupport` needs
  `initialise_monitor_handles` (semihosting). NOTE: semihosting I/O needs a
  debugger attached — retarget C-library I/O to UART for standalone runs.
- **`-R .vtable` on the `.bin`.** SYS/BIOS's RAM vector table has an SRAM LMA;
  without removing it `objcopy -O binary` spans the flash→SRAM gap (~512 MB). The
  `.out`/`.hex` are unaffected.
- **Newlib C-library locking** had to be ported to the gcc-13.2 newlib model —
  see `docs/newlib-locking-port.md`.

### Still open
- Remove the legacy CCS files (`.cproject`, `.ccsproject`, `.project`,
  `makefile.defs`, `.config/`, `targetConfigs/`) now that the CMake build is
  complete. (Note: `src/` now holds `newlib_locks.c`, so keep it.)
- Decide on C-library I/O retargeting (semihosting vs UART) for standalone boot.
- ABI watch-item: prebuilt `.am4fg` libs are gcc-4.8.4 vintage vs build gcc 13.2
  (see §4).

---

## 6. Maintenance

Regenerate (§3.5) **only** when you change kernel configuration in `random.cfg`
(tick period, heap, statically-defined Hwis, the set of BIOS modules). Ordinary
app changes — adding tasks via `Task_create`, editing drivers — never require it.
After regenerating, re-sync the source list/feature defines in `cmake/sysbios.cmake`
if the module set changed, then rebuild.
