# random

Firmware for the **TM4C129ENCPDT** (TI EK-TM4C129EXL LaunchPad) that drives
motors from a gamepad over Wi-Fi, with an interactive UART console for managing
the Wi-Fi connection at runtime.

A host app sends gamepad command frames to the board over TCP (the board runs as
a server, or connects out to a relay app discovered via UDP beacons); the board
parses those frames and drives PWM motor outputs.

## Hardware / stack

- **MCU:** TM4C129ENCPDT (Cortex-M4F) on the EK-TM4C129EXL LaunchPad
- **Wi-Fi:** SimpleLink **CC3100** host driver over SSI2 + uDMA (station mode)
- **RTOS:** TI-RTOS **SYS/BIOS** (statically configured), TivaWare drivers
- **Console:** UART0 = the on-board ICDI virtual COM port (PA0/PA1), **115200 8N1**
- **Build:** CMake + `arm-none-eabi-gcc` in WSL — **no Code Composer Studio**

## Build & flash

See **[BUILD.md](BUILD.md)** for the full toolchain setup and the one-time
SYS/BIOS config generation. In short:

```bash
cmake --preset arm-gcc
cmake --build --preset arm-gcc      # -> build/random.{out,bin,hex}
lm4flash build/random.bin           # flash over the ICDI (or use UniFlash/OpenOCD)
```

Open the console with any serial terminal at 115200 8N1, e.g.:

```bash
picocom -b 115200 /dev/ttyACM0
```

## UART console

The console shares UART0 with the asynchronous log stream. Log lines are
bracketed so they never land on top of a half-typed command — the prompt is
repainted with your input restored after each burst of output. Press **Tab** to
list/complete commands.

| Command          | Description                                  |
|------------------|----------------------------------------------|
| `help`           | list console commands                        |
| `connect <ssid> <passkey>`     | connect to a WPA/WPA2 AP now   |
| `add-profile <ssid> <passkey>` | store a WPA/WPA2 profile on the NWP |
| `profiles`       | list stored Wi-Fi profiles                   |
| `status`         | show Wi-Fi association / IP status            |
| `clear-profiles` | delete all stored profiles                   |

Wi-Fi connects via the NWP **auto-connect policy** against stored profiles
(their keys live on the CC3100 and cannot be read back). On first boot with no
profiles, a compiled-in default is added; thereafter profiles persist across
reboots and are managed from the console.

## Architecture

Tasks created in [`xCon/app/main.c`](xCon/app/main.c) (SYS/BIOS):

- **`uart_messaging_service`** — owns UART0. Drains the log queue and runs the
  interactive console; RX is interrupt-woken (the UART0 Hwi posts a service
  semaphore). See [`comms/uart_interface.c`](xCon/app/comms/uart_interface.c)
  and [`console/`](xCon/app/console).
- **`cm_connection_mgr`** — opens the Wi-Fi driver, configures station mode +
  DHCP + auto-connect, and releases the server once associated with an IP.
  [`comms/connection_manager.c`](xCon/app/comms/connection_manager.c).
- **`server_task`** — waits for the link, then runs the TCP server / relay-client
  loop and posts parsed gamepad frames to the motor mailbox.
  [`comms/server.c`](xCon/app/comms/server.c).
- **`pwm_motor_proc_init`** — consumes gamepad frames and drives the motors.
  [`hardware/`](xCon/app/hardware).

Logging is decoupled: any task calls `uart_log()` (non-blocking, enqueues to a
Mailbox); only the console task touches the wire, so concurrent producers never
interleave. Fatal paths use `uart_log_fatal()` (polled) so the reason reaches
the wire before `System_abort()`.

## Repository layout

```
xCon/app/         application (main, comms, console, hardware, input)
xCon/bsp/         board support (EK_TM4C129EXL.c: pins, drivers, UART0 Hwi)
xCon/sysbios/     SYS/BIOS .cfg (configured once, frozen)
xCon/generated/   committed configuro output (BIOS config, linker.cmd)
xCon/third_party/ vendored TI SDK (SYS/BIOS, TivaWare, SimpleLink)
docs/             build, migration, console, and debugging notes
debug/            OpenOCD/GDB scripts (see docs/openocd-gdb-debugging.md)
```

## Documentation

- [BUILD.md](BUILD.md) — toolchain, one-time config generation, build, flash
- [docs/flashing-and-console.md](docs/flashing-and-console.md) — flashing + opening the COM port
- [docs/uart-console-retarget.md](docs/uart-console-retarget.md) — C-library I/O retarget to UART0
- [docs/openocd-gdb-debugging.md](docs/openocd-gdb-debugging.md) — on-target debugging over the ICDI from WSL
- [docs/tivac-cmake-migration.md](docs/tivac-cmake-migration.md) — the CCS → CMake migration playbook
- [docs/quirks.md](docs/quirks.md) — symptom → cause → fix index
