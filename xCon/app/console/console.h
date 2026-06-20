#ifndef XCON_APP_CONSOLE_CONSOLE_H
#define XCON_APP_CONSOLE_CONSOLE_H

#include <stdarg.h>

void console_init(void);
void console_poll(void);
void console_printf(const char *fmt, ...);
void console_vprintf(const char *fmt, va_list ap);
void console_puts(const char *s);
void console_before_async_output(void);
void console_after_async_output(void);

#endif
