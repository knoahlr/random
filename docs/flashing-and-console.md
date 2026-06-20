# Flashing the board and viewing UART prints (no CCS)

For the EK-TM4C129EXL (TM4C129ENCPDT). Everything below goes over the single
**Debug USB (U22)** connector, which is the on-board **ICDI** — it bridges the
MCU's JTAG to USB *and* presents a virtual COM port (UART0). See
[adding-ndk.md](adding-ndk.md)/[tivac-cmake-migration.md](tivac-cmake-migration.md)
for the build that produces `build/random.bin`.

Two things share that one cable:
- **Flashing** → JTAG side of the ICDI (a USB debug device).
- **Console / `printf`** → the ICDI **virtual COM port** (UART0, 115200 8N1).

---

## 1. Flashing with `lm4flash` on Windows (PowerShell)

### Is `lm4flash.exe` part of CCS?

**No.** `lm4flash` is **not** a CCS component. It is the open-source flasher from
the **`lm4tools`** project (community/TI). What ships *with* TI tooling instead:

- **CCS** flashes through its IDE and the CLI **`DSLite`** (Debug Server Lite,
  under `ccs/ccs_base/DebugServer/bin/`) — not `lm4flash`.
- **LM Flash Programmer** — TI's standalone GUI flasher (separate download).
- **UniFlash** — TI's standalone CLI/GUI flasher (separate download).

So to use `lm4flash.exe` you obtain it separately:
- easiest: it's bundled in **Energia** at
  `energia-*/hardware/tools/lm4f/bin/lm4flash.exe`, or
- build it from the `lm4tools` source.

Either way it needs the **Stellaris/ICDI USB driver** installed (comes with LM
Flash Programmer or the standalone Stellaris ICDI drivers) so the board
enumerates as the *In-Circuit Debug Interface*, not just a COM port.

### Commands (PowerShell)

```powershell
# If lm4flash.exe is on PATH:
lm4flash.exe .\build\random.bin

# Otherwise call it by full path (e.g. from an Energia install):
& "C:\energia\hardware\tools\lm4f\bin\lm4flash.exe" .\build\random.bin

# Verbose (handy if it can't find the ICDI):
lm4flash.exe -v .\build\random.bin
```

Flash the **`.bin`** (raw image at `0x0`), not the `.out`/`.hex`. `lm4flash`
erases, programs, verifies, and resets. If it reports no ICDI found, the USB
driver isn't installed or another tool (CCS/LM Flash Programmer) holds the probe.

### TI-tooling alternatives (also CLI/GUI, no CCS project needed)

```powershell
# LM Flash Programmer GUI: select the Tiva/ICDI (JTAG) interface, load random.bin at 0x0.

# UniFlash CLI:
& "C:\ti\uniflash_x.y.z\dslite.bat" -c <config>.ccxml -f -v .\build\random.bin

# DSLite that ships inside CCS:
& "C:\ti\ccs\ccs_base\DebugServer\bin\DSLite.exe" flash -c <config>.ccxml .\build\random.bin
```

> WSL note: WSL2 has no native USB. Either run the flasher on the **Windows** side
> (build `.bin` in WSL, it's visible at `/mnt/c/...` / from PowerShell), or bind
> the ICDI into WSL with **`usbipd-win`** (`usbipd attach --busid <id> --wsl`),
> after which Linux `lm4flash` / OpenOCD and `/dev/ttyACM0` work.

---

## 2. Viewing UART prints over the COM port (PowerShell)

The firmware's console (both `UARTprintf` and, after the retarget, C-library
`printf`) comes out **UART0 at 115200 8N1** on the ICDI virtual COM port. See
[uart-console-retarget.md](uart-console-retarget.md) for the retarget.

### Find the COM port

In Device Manager it appears under **Ports (COM & LPT)** as the *Stellaris/ICDI
Virtual COM Port*. From PowerShell:

```powershell
# Simple list of port names:
[System.IO.Ports.SerialPort]::GetPortNames()

# With friendly names so you can spot the ICDI one:
Get-CimInstance Win32_PnPEntity |
  Where-Object { $_.Name -match '\(COM\d+\)' } |
  Select-Object Name
```

### Open it and stream prints

PowerShell can drive a port directly via `System.IO.Ports.SerialPort` (replace
`COM5`):

```powershell
$port = New-Object System.IO.Ports.SerialPort 'COM5', 115200, 'None', 8, 'One'
$port.Open()
try {
    while ($true) {
        $data = $port.ReadExisting()
        if ($data) { Write-Host -NoNewline $data }
        Start-Sleep -Milliseconds 20
    }
}
finally {
    $port.Close()   # runs on Ctrl+C
}
```

To also **send** a line to the device (e.g. the shell prompt):

```powershell
$port.WriteLine("status")
```

> If the port won't open ("access denied"), another program holds it — close LM
> Flash Programmer's serial view, PuTTY/Tera Term, or any other terminal.
> Flashing and the COM stream can't both be active in conflicting tools at once,
> but flashing (JTAG) and watching the COM port from PowerShell are independent.

### Easier alternatives

For day-to-day monitoring, a terminal app is less fiddly than the SerialPort
loop: **PuTTY** (Serial, COM5, 115200) or **Tera Term**. The PowerShell method
above is here because you asked for it specifically and it needs nothing
installed.

> WSL: after `usbipd attach`, the same port is `/dev/ttyACM0` — use
> `screen /dev/ttyACM0 115200`, `picocom -b 115200 /dev/ttyACM0`, or `tio`.
