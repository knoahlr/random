# On-target debugging with OpenOCD + GDB (TM4C129 ICDI, from WSL)

How to halt the running firmware, get backtraces, flash, and step through
SYS/BIOS / SimpleLink code over the on-board ICDI debugger — all from WSL.

This is the procedure that root-caused the WiFi bring-up hang (see the worked
example at the end). The reusable GDB scripts live in `debug/`.

---

## 1. What the ICDI is

The EK-TM4C129EXL LaunchPad has an on-board **ICDI** (In-Circuit Debug
Interface). It is a composite USB device (VID:PID `1cbe:00fd`) exposing two
independent interfaces:

| Interface        | Used by      | Linux device            |
|------------------|--------------|-------------------------|
| Virtual COM port | console/logs | `/dev/ttyACM0` (picocom)|
| Debug (JTAG/SWD) | OpenOCD      | libusb (no /dev node)   |

Because they are separate interfaces, **OpenOCD and picocom can run at the same
time** — you can watch UART logs while halting the core. The JTAG transport is
tunnelled over USB; there is no separate JTAG dongle.

## 2. One-time setup (WSL)

```bash
sudo apt-get install -y openocd gdb-multiarch
```

The Cortex-M4F target is debugged with `gdb-multiarch` (the Ubuntu
`gcc-arm-none-eabi` package does not ship `arm-none-eabi-gdb`).

The ICDI must be visible in WSL. `lsusb | grep 1cbe` should show
`In-Circuit Debug Interface`; `/dev/ttyACM0` should exist. If not, pass the USB
device through with `usbipd` from Windows (see `docs/wsl_flashing.md`).

## 3. Start the OpenOCD server

OpenOCD claims the debug interface via libusb, which needs USB access. The
simplest path in WSL is to run it under `sudo`:

```bash
sudo openocd -f board/ti_ek-tm4c1294xl.cfg
```

(The EK-TM4C129EXL uses the same ICDI + TM4C129 target as the `1294xl` board
file. The older `board/ek-tm4c1294xl.cfg` works too but warns it is deprecated.)

Leave it running. It listens on:

- **:3333** — GDB remote
- **:4444** — telnet monitor
- **:6666** — TCL

A healthy start ends with `Listening on port 3333 for gdb connections` and
**no** `LIBUSB_ERROR_ACCESS`. If you see `LIBUSB_ERROR_ACCESS`, OpenOCD lacks
USB permission — run it under `sudo`, or install a udev rule for `1cbe:00fd`.

> To avoid `sudo` every time, add `/etc/udev/rules.d/60-ti-icdi.rules`:
> `SUBSYSTEM=="usb", ATTRS{idVendor}=="1cbe", ATTRS{idProduct}=="00fd", MODE="0666"`
> then `sudo udevadm control --reload && sudo udevadm trigger` (and replug).

## 4. Connect GDB

In another terminal, from the repo root, point GDB at the ELF (for symbols) and
attach to the OpenOCD remote:

```bash
gdb-multiarch -q build/random.out
(gdb) target extended-remote localhost:3333
```

On connect, OpenOCD **halts the core in place**. If the firmware was hung, you
are now stopped exactly at the hang. Useful first commands:

```
(gdb) monitor halt          # force a halt (if needed)
(gdb) bt                    # backtrace of the current CPU context
(gdb) info registers
(gdb) monitor reset halt    # reset and stop at the reset vector
(gdb) load                  # flash build/random.out over the debug link
(gdb) continue              # run
```

`monitor <cmd>` forwards `<cmd>` to OpenOCD (e.g. `monitor reset halt`).

### Flashing from GDB

`load` programs flash directly over the debug link — no LM Flash Programmer
needed:

```
(gdb) monitor reset halt
(gdb) load
(gdb) monitor reset halt
(gdb) continue
```

> Caveat on this board: `monitor reset run` / SRST sometimes upsets the ICDI
> (`SRST error`, then `jtag status ... communication failure`). Prefer
> `monitor reset halt` followed by `continue`, or just press the physical RESET
> button.

## 5. Running GDB scripts non-interactively

Batch a command file with `-x` and bound it with `timeout` so a never-returning
`continue` can't wedge your shell:

```bash
timeout 40 gdb-multiarch -q -x debug/probe_hang.gdb build/random.out
```

The scripts in `debug/`:

| Script              | Purpose                                                        |
|---------------------|---------------------------------------------------------------|
| `inspect.gdb`       | Attach to the live (hung) target, dump current context.       |
| `hang_bt.gdb`       | Reset, run a few seconds, freeze, print backtrace + registers. |
| `probe_hang.gdb`    | Walk the SimpleLink blocking primitives to find the wedge.     |
| `flash_verify.gdb`  | Flash, then prove a specific call now returns.                 |

## 6. Techniques that matter here

### Synchronous vs async stops
`set target-async on` + `continue &` + `interrupt` lets a script "run then stop
after N seconds" (the scripted equivalent of Ctrl-C). But the `interrupt` is
itself asynchronous — a `bt` issued immediately after races the halt and fails
with *"Selected thread is running."* For batch scripts, prefer a **synchronous**
approach: set breakpoints and let `continue` stop on them. To probe *where* a
task wedges, breakpoint the blocking primitive itself — its entry **is** the
wedge point, captured with a clean live `bt`.

### Counting an event without stopping
Attach `commands … silent / printf / continue / end` to a breakpoint so each hit
prints a line and resumes. Counting the printed lines tells you whether an ISR
or task body ever ran, without halting the flow:

```
break SPITivaDMA_hwiFxn
commands
  silent
  printf "[hwiFxn] SPI-DMA completion ISR fired\n"
  continue
end
```

### No SYS/BIOS thread awareness
OpenOCD has no RTOS plugin for TI-RTOS/SYS/BIOS, so `info threads` does **not**
list BIOS tasks — `bt` only unwinds the *current* CPU context. To see where a
specific blocked task is, either breakpoint the function it is stuck in (so it
becomes the live context when hit), or manually read its saved SP from its
`Task_Struct` and unwind by hand.

### Reading a raw PC against the map
A backtrace showing `PC = 0x00000040` looks alarming, but cross-checking the
symbol table (`arm-none-eabi-nm build/random.out | sort`) showed `0x40 =
ti_sysbios_family_arm_m3_Hwi_dispatch__I` — i.e. the **interrupt dispatcher**,
not a crash. Always resolve a suspicious address against the ELF before
concluding it is a fault. (The Hwi vector table — `Hwi_resetVectors` — is at
address `0x0` in this image, so low addresses are vectors/dispatch, not garbage.)

### Useful symbol greps
```bash
arm-none-eabi-nm build/random.out | grep -iE 'osi_Sync|osi_Lock|_SlDrv|Spawn|SPI_transfer|sl_Start'
arm-none-eabi-nm build/random.out | grep -iE 'excHandler|Hwi_dispatch|resetVectors'
```

---

## Worked example: the WiFi bring-up hang

**Symptom:** every SimpleLink command issued after `sl_Start` blocked forever;
the CC3100 HOST-IRQ (PM3) appeared never to assert.

**Investigation (all via the steps above):**

1. On attach, the CPU PC was in `Task_allBlockedFunction` → *every* task was
   blocked (pending on a semaphore), not spinning or faulted.
2. `probe_hang.gdb` ran past `sl_Start` (so device init succeeded), then made
   `osi_SyncObjWait` / `osi_LockObjLock` stop-and-`bt` breakpoints and
   `SPITivaDMA_hwiFxn` / `vSimpleLinkSpawnTask` silent counters.
3. The connection manager cleanly acquired the command lock — but the repeating
   backtrace was a **different task**: `server_task → udp_discovery_init →
   sl_Socket → _SlDrvMsgReadCmdCtx → osi_SyncObjWait`.
4. `PC = 0x40` between waits resolved to `Hwi_dispatch__I` (interrupts were
   fine), ruling out the "crash/fault" theory.

**Root cause:** `server_task()` called `udp_discovery_init()` (which opens a
socket via `sl_Socket`) **before** `Semaphore_pend()` on the connection
semaphore. So at boot the server task entered the single-threaded SimpleLink
command/response pipe concurrently with the connection manager's bring-up
commands. Two outstanding commands deadlocked the NWP response path → every
post-`sl_Start` command hung.

**Fix:** in `server_task()`, pend on the connection semaphore first; only call
`udp_discovery_init()` after the link is up.

**Verification:** `flash_verify.gdb` flashed the rebuilt image, broke at
`sl_NetCfgGet`, and `finish` returned `0` (NWP responded) — then execution
proceeded to `sl_WlanPolicySet`, past the old hang. The board then associated
and acquired an IP on the console.
