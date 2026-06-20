# Retargeting the C library off semihosting onto UART0

How standalone (no-debugger) C-library I/O works in this firmware, and why the
build links `-lnosys` instead of `-lrdimon`. Read this before touching
`xCon/sysbios/syscalls_uart.c`, the `.heap` region in `xCon/linker/tm4c129encpdt.lds`, or the
`SemiHostSupport` line in `xCon/sysbios/random.cfg`.

## The problem

SYS/BIOS's `ti.sysbios.rts.gnu.SemiHostSupport` module routes C-library I/O
through **ARM semihosting**: at boot it calls `initialise_monitor_handles()`
(from `librdimon`), and newlib's `_write`/`_read`/`_sbrk` issue semihosting `SVC`
traps that an **attached debugger** services. With no debugger attached those
traps halt the core, so a standalone board stalls on:

- the first `printf()`/`puts()` (→ `_write` → semihosting), and
- the first `malloc()` if it were newlib-backed (→ `_sbrk` → semihosting
  `SYS_HEAPINFO`).

This board has no reason to run under a debugger in the field, and its console is
naturally UART0 — which is the **EK-TM4C129EXL ICDI virtual COM port** (SPMU372A
§2.1.7.2: *"UART0 is used for the ICDI virtual UART"*). The app already prints to
it via TivaWare `UARTprintf`; only the C-library `printf`/`malloc` paths were
still tied to semihosting.

## The fix (`xCon/sysbios/syscalls_uart.c`, no config regeneration)

Strong definitions that override newlib/libnosys defaults — the same override
mechanism as `xCon/sysbios/newlib_locks.c` (each default lives in its own archive object,
pulled only to satisfy an undefined reference, so our definition wins with no
multiple-definition error):

| Symbol | Behaviour |
| --- | --- |
| `_write` | stdout(1)/stderr(2) → UART0 with LF→CR/LF; other fds `EBADF`. Gated on the UART0 `UARTEN` bit so pre-`UARTStdioConfig` writes are dropped, not faulted. |
| `_isatty` / `_fstat` | report 0/1/2 as character devices → newlib **line-buffers** stdout, so `printf` flushes on `\n` (without these it is fully buffered and output is delayed). |
| `initialise_monitor_handles` | **no-op** override, so `SemiHostSupport`'s boot `lastFxn` no longer performs the semihosting handshake. |
| `_sbrk` | guard only — returns `ENOMEM` (see heap note). |
| `_read` / `_close` / `_lseek` / `_getpid` / `_kill` | minimal clean-failing stubs (no filesystem, no semihosting). |

Build change: link `-lnosys` (stub syscalls) instead of `-lrdimon` (semihosting).
`initialise_monitor_handles` is *not* in libnosys, so we must provide it — which
we do as the no-op above.

### Heap note — why `_sbrk` is only a guard

`malloc`/`free` are **not** newlib's `_sbrk`-based allocator here: they resolve to
the **SYS/BIOS heap** (`HeapMem`, emitted into the generated config and sized in
`xCon/sysbios/random.cfg`). So newlib shares the one RTOS heap, `_sbrk` is never called, and
`--gc-sections` drops it from the image. Consequently **no `.heap` size is
reserved** in `xCon/linker/tm4c129encpdt.lds` (`end == __HeapLimit`), saving SRAM. The
`_sbrk` guard exists only so that if some libc path ever did bypass the RTOS heap
it fails cleanly (`ENOMEM`) instead of trapping. To switch back to a newlib heap,
reserve a sized `.heap` in the linker script and the existing `_sbrk` becomes a
real bump allocator.

## Console ordering caveat

C-library output appears only **after** UART0 is enabled — i.e. after the app's
`configure_uart_interface()`/`UARTStdioConfig(0, 115200, ...)` runs. `printf`
calls before that (early init error paths) are silently dropped by the `UARTEN`
gate. If you need the earliest possible boot messages on the wire, move the
`UARTStdioConfig` call into board init (`EK_TM4C129EXL_initUART`).

## Optional follow-up (needs config regeneration)

`SemiHostSupport` is now inert (its only external call is our no-op), so it can
stay. To remove it entirely, delete the `xdc.useModule('...SemiHostSupport')`
block from `xCon/sysbios/random.cfg` and regenerate the config (see
[regenerating-sysbios-config.md](regenerating-sysbios-config.md)) — a kernel
module-set change, hence the one heavyweight step. Not required; the inert module
costs only its tiny startup `lastFxn`.

See also the `initialise_monitor_handles` entry in [quirks.md](quirks.md).
