# Regenerating the SYS/BIOS configuration with XDCtools

This project no longer needs Code Composer Studio or XDCtools to **build**. The
SYS/BIOS static configuration is generated **once** by XDCtools `configuro`,
committed under `xCon/generated/`, and from then on compiled as ordinary C by CMake.

You only need this document if you change kernel configuration in `xCon/sysbios/random.cfg`
(tick period, heap size, statically-defined interrupts, which BIOS modules are
pulled in, etc.) and therefore must regenerate. Day-to-day application changes
(adding tasks via `Task_create`, editing drivers, etc.) do **not** require
regeneration — see [BUILD.md](../BUILD.md).

---

## 1. The mental model (RTSC / XDC in one minute)

SYS/BIOS is an **RTSC package**. You don't configure it in C; you configure it
in a meta-language script (`xCon/sysbios/random.cfg`, written in JavaScript/Rhino). A tool
called `configuro` (part of XDCtools) reads that script plus the BIOS packages
and **emits C + linker artifacts** that embody your configuration.

```
xCon/sysbios/random.cfg ──(configuro)──▶  random_pm4fg.c   (static kernel config, C)
   +                         random_pm4fg.h
BIOS packages               linker.cmd        (GNU linker script)
   +                         compiler.opt      (include paths + -D defines)
gnu.targets.arm.M4F          sysbios makefile  (which 29 BIOS files to compile)
```

We compile/link those outputs ourselves with `arm-none-eabi-gcc`. XDCtools is
just a **code generator** that runs once.

---

## 2. What configuro actually produces

| Artifact | What it is | Who consumes it |
| --- | --- | --- |
| `random_pm4fg.c` / `.h` | The static kernel image in C: the **Hwi interrupt vector table**, the `Clock` tick + `Timer`, the BIOS heap (`BIOS.heapSize`), the `SysMin` output buffer, and the module state for `Task`/`Semaphore`/`Mailbox`/`Queue`/`Idle`/`Error`/`Text`. This is the C embodiment of `xCon/sysbios/random.cfg`. | Compiled into the app by CMake. |
| `linker.cmd` | A **GNU ld linker script**: TM4C129ENCPDT memory map (FLASH @ 0x0, SRAM @ 0x2000_0000), section placement (`.resetVecs` at 0, `.text`, `.bss`, `.data`, `.stack`, heap), and the library `GROUP`. | Passed to the linker via `-T`. |
| `compiler.opt` | The exact `-I` include roots and `-D` defines (e.g. `xdc_target_types__`, `xdc_target_name__=M4F`, and the module `__D` feature defines) so the app + generated config compile **consistently** with the kernel. | `CMakeLists.txt` harvests `-I`/`-D` from it. |
| `package/cfg/random_pm4fg.src/sysbios/makefile` | A standalone makefile listing the **29 SYS/BIOS source files** (25 `.c` + 4 `.sv7M` assembly) configuro selected for *this* config, plus the `-D` feature flags they must be built with. | We mirror this file list + flags into the CMake `sysbios` static-library target. |

> Note: configuro does **not** generate the driver / WiFi / NDK libraries. Those
> ship prebuilt in the SDK, including the GNU `.am4fg` variants we link.

---

## 3. Prerequisites for regeneration

The build machine never needs these; only the machine doing the (rare)
regeneration does.

- **XDCtools 3.32.00.06** — must be a build that can run the target compiler.
  - ⚠️ **The Windows XDCtools cannot drive the Linux `arm-none-eabi-gcc`.**
    During the custom SYS/BIOS library build it execs the compiler via Windows
    `gmake.exe`, which fails on a Linux ELF (`CreateProcess error=193`). It then
    deletes `random_pm4fg.c` and `linker.cmd` and aborts.
  - ✅ Install the **Linux** XDCtools in WSL so `xs`, `gmake`, and
    `arm-none-eabi-gcc` are all native with POSIX paths. This is the supported
    path for this repo.
- **GNU Arm toolchain** (`arm-none-eabi-gcc`) on `PATH`.
- The TI SDK packages (BIOS, TI-drivers, NDK) reachable on disk (e.g. under
  `/mnt/c/ti` or a local copy).

---

## 4. Required `xCon/sysbios/random.cfg` edits for the GNU target

These are already applied in this repo. If you start from a fresh TI example
`.cfg` you will need them again, because the stock files assume the TI compiler:

1. **Guard the reset-vectors section.** The GNU target does not pre-populate
   `Program.sectMap[".resetVecs"]`, so the TI-style assignment throws
   `TypeError: Cannot set property "loadAddress" of undefined`:

   ```js
   if (Program.sectMap[".resetVecs"] === undefined) {
       Program.sectMap[".resetVecs"] = new Program.SectionSpec();
   }
   Program.sectMap[".resetVecs"].loadAddress = 0;
   ```

2. **Don't force TI compiler flags onto gcc.** `BIOS.customCCOpts` in the stock
   file contains armcl flags (`-mv7M4 --abi=eabi …`) that gcc cannot parse. Only
   apply them for TI targets and let the GNU target use its defaults:

   ```js
   if (Program.build.target.$name.match(/ti\.targets/)) {
       BIOS.customCCOpts = "--endian=little -mv7M4 --abi=eabi …";
   }
   ```

---

## 5. Running the generation

A helper script wraps the invocation: [`tools/generate-bios-config.sh`](../tools/generate-bios-config.sh).
Point it at your Linux XDCtools + SDK + toolchain and run:

```bash
GNU_TOOLS=/usr ./tools/generate-bios-config.sh
```

The underlying command it runs is:

```bash
xs --xdcpath="<bios>/packages;<tidrivers>/packages;<ndk>/packages;<xdctools>/packages" \
   xdc.tools.configuro \
   -c "<gnu-toolchain-root>" \        # dir containing bin/arm-none-eabi-gcc
   -t gnu.targets.arm.M4F \           # the target (compiler + ISA)
   -p ti.platforms.tiva:TM4C129ENCPDT \  # the platform (chip + memory map)
   -r release \
   -o generated \                     # output directory
   xCon/sysbios/random.cfg
```

Key arguments:
- `-t gnu.targets.arm.M4F` — selects gcc + Cortex-M4F. (TI build used
  `ti.targets.arm.elf.M4F`.)
- `-p ti.platforms.tiva:TM4C129ENCPDT` — supplies the device memory map that
  ends up in `linker.cmd`.
- `-c <root>` — the toolchain root; configuro runs the compiler from here.

If you used the **Windows** XDCtools, the emitted `compiler.opt` / `linker.cmd`
contain `C:/...` paths; the helper script converts them to `/mnt/c/...`. With the
**Linux** XDCtools and POSIX SDK paths, no conversion is needed.

---

## 6. After regenerating

1. Confirm `xCon/generated/` contains `random_pm4fg.c`, `random_pm4fg.h`, and the
   linker script (`linker.cmd` or `*_pm4fg.cmd`).
2. If the **set of BIOS modules changed**, re-check the source list in
   `xCon/generated/package/cfg/random_pm4fg.src/sysbios/makefile` against the
   `sysbios` target's file list in `CMakeLists.txt`, and sync the `-D` feature
   defines (`BIOS_DEFS` in that makefile).
3. Commit the updated `xCon/generated/` directory.
4. `cmake --build --preset arm-gcc` and verify it links.

---

## 7. Toward writing these by hand

If you eventually want to drop XDCtools entirely and author the equivalents
yourself, here is what each generated piece really is — and how hard it is:

- **`linker.cmd`** — straightforward. It's a normal GNU linker script: define
  `MEMORY` regions for the TM4C129 (FLASH 1 MB @ 0x0, SRAM 256 KB @ 0x20000000),
  then `SECTIONS` placing `.resetVecs`/`.text`/`.rodata` in FLASH and
  `.data`/`.bss`/`.stack`/heap in SRAM. This is the easiest to take ownership of;
  any TM4C129 GNU example script is a fine starting point.
- **The Hwi vector table** — a C array of function pointers at address 0 (the
  initial SP + reset handler + NVIC entries). SYS/BIOS routes most IRQs through
  its dispatcher. Hand-writing this means replicating the dispatcher wiring,
  which is the hard part of `random_pm4fg.c`.
- **Static object instances** (`Clock`, `Task`, `Semaphore`, …) — `random_pm4fg.c`
  pre-allocates and initializes the module state structs. Because this app
  creates everything at **runtime** (`Task_create`, `Semaphore_create`,
  `Mailbox_create`), the static instance content is minimal — most of the file is
  module *config* (tick source, heap, dispatcher options), not app objects.
- **Module feature defines** (`ti_sysbios_*__D=…`) — these compile-time switches
  (in `BIOS_DEFS`) prune/configure the kernel. To build BIOS without configuro
  you must supply a consistent set of these yourself; that's the fiddly part.

Realistic recommendation: take ownership of `linker.cmd` first (low risk, high
clarity), keep `random_pm4fg.c` generated until you've fully internalized the
Hwi/dispatcher setup, and only then attempt a hand-written kernel config.
