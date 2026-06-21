/*
 * uart_interface.h
 *
 *  Created on: Mar 5, 2023
 *      Author: Noah Workstation
 */

#include <stdbool.h>
#include <stdint.h>

#include <ti/sysbios/knl/Task.h>

#include <xdc/std.h>
#include <xdc/runtime/System.h>

/* uartstdio is built with UART_BUFFERED; its buffered-only prototypes use bool,
 * so stdbool.h must precede this include. */
#include <utils/uartstdio.h>

#ifndef UART_IF_HEADER_FILE_H
#define UART_IF_HEADER_FILE_H

/* Maximum length of a single formatted log line (longer lines are truncated). */
#define UART_LOG_MSG_LEN 256

/*
 * Initialize the console log queue. Must be called once (before BIOS_start, in
 * main) so producers can post before the consumer task runs.
 */
void uart_log_init(void);

/*
 * printf-style, non-blocking console logging. Formats the line and posts it to
 * the log queue; the message is dropped if the queue is full. Safe to call from
 * any task context. The actual UART writes happen on the single consumer task
 * (uart_messaging_service), so concurrent producers never interleave on the wire.
 */
void uart_log(const char *fmt, ...);

/*
 * Blocking, polled console write that bypasses the queue and the uartstdio TX
 * ring. Use only on fatal paths just before System_abort(), where the
 * interrupt-driven drain can no longer flush. Adds CR before LF.
 */
void uart_log_fatal(const char *s);

/*
 * Fatal-path decimal output that avoids printf/vsnprintf/newlib locks. Intended
 * for Hwi/fault paths that must put one diagnostic value on UART before abort.
 */
void uart_log_fatal_u32(const char *prefix, uint32_t value, const char *suffix);
void uart_log_fatal_hex32(const char *prefix, uint32_t value, const char *suffix);

/*
 * Console log consumer task: owns UART0, draining the log queue to the wire.
 * Run exactly one instance (started from main).
 */
void uart_messaging_service(UArg arg0);

#endif
