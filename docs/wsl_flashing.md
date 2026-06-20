# WSL USB flashing and UART console

WSL2 does not get direct access to Windows USB devices by default. To use the
EK-TM4C129EXL ICDI from WSL, share the USB device on the Windows side with
`usbipd-win`, then attach it to WSL.

The important distinction: share the USB **bus ID**, not the Windows COM port
name. `COM5` may be the serial interface, but `usbipd` works with IDs like
`3-7` from `usbipd list`.

## 1. Find the USB device

Open PowerShell on Windows:

```powershell
usbipd list
```

Find the board's ICDI entry. It may show up as a Stellaris/Tiva ICDI/debug
interface and/or virtual COM device. Note the `BUSID`, for example `3-7`.

## 2. Bind once as Administrator

Open a local **PowerShell as Administrator** on Windows and run:

```powershell
usbipd bind --busid 3-7
usbipd list
```

The device state should change from:

```text
Not shared
```

to:

```text
Shared
```

This sharing step is persistent, so it usually only needs to be done once per
device/bus ID. If `attach` reports this error:

```text
usbipd: error: Device is not shared; run 'usbipd bind --busid 3-7' as administrator first.
```

then the `bind` step has not succeeded for that bus ID.

Prefer a local elevated PowerShell for `bind`. An administrator account over SSH
can still run into UAC/session restrictions around device operations.

## 3. Attach to WSL

Keep a WSL shell open so the WSL2 VM is running. Then, from Windows PowerShell
(admin is not required after the bind):

```powershell
usbipd attach --wsl --busid 3-7
usbipd list
```

The device state should become:

```text
Attached
```

While attached, Windows cannot use the same USB device/COM port. The device is
owned by WSL until it is detached, unplugged, or WSL is shut down.

## 4. Verify inside WSL

Inside WSL:

```bash
lsusb
dmesg | tail -50
ls -l /dev/ttyUSB* /dev/ttyACM* 2>/dev/null
```

The ICDI virtual COM port normally appears as `/dev/ttyACM0` for this board.
Use 115200 8N1 for the firmware console:

```bash
screen /dev/ttyACM0 115200
# or
picocom -b 115200 /dev/ttyACM0
# or
tio /dev/ttyACM0
```

For flashing from WSL, use Linux tools such as `lm4flash` or OpenOCD against the
attached ICDI device.

## 5. Detach when done

From Windows PowerShell:

```powershell
usbipd detach --busid 3-7
```

After detach, Windows can use the COM port again.

## Quick sequence

```powershell
# Windows PowerShell as Administrator, once:
usbipd list
usbipd bind --busid 3-7

# Windows PowerShell, later as a normal user:
usbipd attach --wsl --busid 3-7
```

```bash
# WSL:
lsusb
ls -l /dev/ttyACM* /dev/ttyUSB* 2>/dev/null
screen /dev/ttyACM0 115200
```
