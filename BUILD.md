# Building (CMake + GNU Arm, in WSL)

This project builds with CMake and `arm-none-eabi-gcc`. It no longer requires
Code Composer Studio. TI's XDCtools is used **once** to generate the SYS/BIOS
static configuration, after which the generated output is committed and the
build is pure CMake.

## Why there is a one-time generation step

The SYS/BIOS kernel configuration in `random.cfg` (interrupt vector table,
Clock tick, heap, statically-built modules) is emitted as a generated C file by
TI's `configuro` tool. SYS/BIOS cannot be configured by hand the way FreeRTOS
can, so we generate that file once and commit it. The application itself creates
its tasks/semaphore/mailbox at runtime, so this static config rarely changes.

## Prerequisites (one time)

Inside WSL:

```bash
sudo apt-get install cmake ninja-build gcc-arm-none-eabi
# or install ARM's official toolchain and put its bin/ on PATH
```

The TI SDKs are expected under `/mnt/c/ti` (Windows install, used via WSL):

- `xdctools_3_32_00_06_core`  (only for the generation step)
- `tirtos_tivac_2_16_00_08`   (SYS/BIOS, TI drivers, SimpleLink WiFi, NDK)
- `TivaWare_C_Series-2.2.0.295`

## Step 1 — Generate the BIOS config (run once, and after any `random.cfg` change)

```bash
GNU_TOOLS=/usr/lib/arm-none-eabi ./tools/generate-bios-config.sh
```

This runs `configuro` for the `gnu.targets.arm.M4F` target, writes the generated
config + `compiler.opt` + linker command file into `generated/`, and converts the
Windows paths it emits into WSL paths. Commit the `generated/` directory.

> Adjust the install paths and `GNU_TOOLS` at the top of the script if your
> layout differs.

## Step 2 — Configure & build

```bash
cmake --preset arm-gcc
cmake --build --preset arm-gcc
```

Outputs land in `build/`: `random.out` (ELF), `random.bin`, `random.hex`,
`random.map`, and a size report.

## Flashing

Use whatever you already use (UniFlash, OpenOCD, or CCS for debug). Example with
OpenOCD:

```bash
openocd -f board/ek-tm4c1294xl.cfg -c "program build/random.out verify reset exit"
```

## Notes / things to expect on first build

- The `generated/` artifacts are target-specific. If you switch compilers or
  targets, re-run Step 1.
- `CMakeLists.txt` harvests include paths and defines from
  `generated/compiler.opt` so the app compile matches the BIOS config exactly.
- The generated linker script pulls in the prebuilt BIOS/driver/WiFi/NDK
  libraries; TivaWare `driverlib` + `uartstdio` are compiled from source.
- First link may surface a few missing-symbol or section-placement issues that
  need a small tweak to flags — expected when moving an XDC project to plain
  CMake.
