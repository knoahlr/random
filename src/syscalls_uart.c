/*
 *  ======== syscalls_uart.c ========
 *  Standalone (no-debugger) retarget of the newlib C library off semihosting.
 *
 *  Background
 *  ----------
 *  SYS/BIOS's `ti.sysbios.rts.gnu.SemiHostSupport` module routes C-library I/O
 *  through ARM semihosting: at boot it calls `initialise_monitor_handles()`
 *  (from librdimon) and newlib's `_write`/`_read`/`_sbrk` issue semihosting SVC
 *  traps serviced by an attached debugger. With NO debugger attached those traps
 *  halt the core, so the firmware stalls on the first `printf()` -- and, because
 *  the app also calls `malloc()`, on the first heap growth too (`_sbrk` ->
 *  semihosting SYS_HEAPINFO).
 *
 *  This file severs that dependency so the board runs standalone:
 *    - `_write`  -> UART0 (PA0/PA1), which is the EK-TM4C129EXL ICDI virtual COM
 *                   port (SPMU372A 2.1.7.2: "UART0 is used for the ICDI virtual
 *                   UART"). printf()/puts() therefore appear on the same COM port
 *                   as the app's existing UARTprintf() console.
 *    - `_sbrk`   -> guard only. malloc()/free() resolve to the SYS/BIOS heap
 *                   (HeapMem, from the generated config), so newlib never grows
 *                   an _sbrk heap; this just fails cleanly instead of trapping.
 *    - `initialise_monitor_handles` -> no-op override, so SemiHostSupport's boot
 *                   lastFxn no longer performs the semihosting handshake.
 *
 *  Override mechanism: each of these is a strong definition. newlib (libnosys)
 *  keeps its default `_write`/`_sbrk`/... each in its own archive object pulled
 *  only to satisfy an undefined reference, so defining them here means the
 *  linker never pulls newlib's defaults -- no multiple-definition conflict.
 *  (Same trick as src/newlib_locks.c.) The build links -lnosys, not -lrdimon.
 *
 *  See docs/tivac-cmake-migration.md and the initialise_monitor_handles entry in
 *  docs/quirks.md.
 */

#include <stdint.h>          /* TivaWare headers assume these are already pulled */
#include <stdbool.h>
#include <stddef.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>

#include "inc/hw_memmap.h"   /* UART0_BASE             */
#include "inc/hw_uart.h"     /* UART_O_CTL, UART_CTL_UARTEN */
#include "inc/hw_types.h"    /* HWREG                  */
#include "driverlib/uart.h"  /* UARTCharPut            */

/* Heap bounds from linker/tm4c129encpdt.lds (.heap region). No size is reserved
 * there -- malloc is BIOS-backed -- so end == __HeapLimit and _sbrk below grants
 * nothing. The symbols are kept so _sbrk gives the correct ENOMEM answer. */
extern char end;          /* first free byte after .bss (a.k.a. _end) */
extern char __HeapLimit;  /* top of the (unreserved) heap window       */

/*
 *  ======== initialise_monitor_handles ========
 *  Override librdimon's semihosting handshake with a no-op. SemiHostSupport_startup
 *  (a BIOS lastFxn) calls this after the vector table is installed; with no debugger
 *  the real version would hang, so we make it do nothing.
 */
void initialise_monitor_handles(void)
{
}

/*
 *  ======== uart0_enabled ========
 *  True once UART0 has been brought up (UARTStdioConfig() in the app sets UARTEN).
 *  Writing to the UART before it is enabled would block or fault, so early stdio
 *  (before the console is configured) is dropped rather than risked.
 */
static int uart0_enabled(void)
{
    return (HWREG(UART0_BASE + UART_O_CTL) & UART_CTL_UARTEN) != 0;
}

/*
 *  ======== _write ========
 *  stdout (1) and stderr (2) go to UART0 with LF -> CR/LF translation so a host
 *  terminal renders newlines correctly. Other descriptors are unsupported.
 */
int _write(int fd, const char *buf, int len)
{
    if (fd != 1 && fd != 2) {
        errno = EBADF;
        return -1;
    }
    if (!uart0_enabled()) {
        return len;  /* console not up yet: discard, do not block */
    }
    for (int i = 0; i < len; i++) {
        if (buf[i] == '\n') {
            UARTCharPut(UART0_BASE, '\r');
        }
        UARTCharPut(UART0_BASE, (unsigned char)buf[i]);
    }
    return len;
}

/*
 *  ======== _isatty / _fstat ========
 *  Report stdin/stdout/stderr as character devices (a TTY). This makes newlib
 *  line-buffer stdout, so printf() flushes on '\n' to the UART instead of
 *  waiting for a full block buffer. Without these, newlib's default _isatty
 *  "always fails" -> stdout is fully buffered and console output is delayed.
 */
int _isatty(int fd)
{
    return (fd == 0 || fd == 1 || fd == 2) ? 1 : 0;
}

int _fstat(int fd, struct stat *st)
{
    (void)fd;
    st->st_mode = S_IFCHR;
    return 0;
}

/* Remaining low-level stubs: no filesystem on this target. They fail cleanly
 * (no semihosting trap) and keep the link free of "not implemented" warnings. */
int _read(int fd, char *buf, int len)   { (void)fd; (void)buf; (void)len; return 0; } /* EOF */
int _close(int fd)                      { (void)fd; return -1; }
int _lseek(int fd, int off, int dir)    { (void)fd; (void)off; (void)dir; return 0; }
int _getpid(void)                       { return 1; }
int _kill(int pid, int sig)             { (void)pid; (void)sig; errno = EINVAL; return -1; }

/*
 *  ======== _sbrk ========
 *  Guard only -- malloc/free are BIOS-backed, so newlib does not grow a heap
 *  through here. Bounded by the (unreserved) .heap window: returns (void *)-1
 *  with errno=ENOMEM instead of trapping to a debugger if anything ever calls it.
 */
void *_sbrk(ptrdiff_t incr)
{
    static char *brk = &end;
    char *prev = brk;

    if (brk + incr > &__HeapLimit) {
        errno = ENOMEM;
        return (void *)-1;
    }
    brk += incr;
    return prev;
}
