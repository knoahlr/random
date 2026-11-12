/**
 * @file console.h
 * @brief Provide the UART console API for editing input, writing output, and managing the prompt.
 * @defgroup console_api UART console
 * @brief Provide an interactive console owned by one task using buffered TivaWare UART I/O.
 *
 * Call these functions from the UART messaging task after UART initialization.
 * The editor and command parser share state and do not support concurrent or
 * nested calls. These functions do not provide locking. Other tasks should submit background messages through uart_log().
 * @{
 */
#ifndef XCON_APP_CONSOLE_CONSOLE_H
#define XCON_APP_CONSOLE_CONSOLE_H

#include <stdarg.h>

/**
 * @brief Reset the editor, disable driver echo, and print the initial prompt.
 * @pre UARTStdioConfig() has configured UART0 and its interrupt handler.
 */
void console_init(void);
/**
 * @brief Drain available RX bytes and synchronously dispatch submitted commands.
 *
 * A carriage return or line feed submits the command. Backspace or DEL removes
 * the last character. Tab lists commands that match the entered prefix and
 * displays their help text without changing the input. Printable ASCII
 * characters are echoed and appended until the line reaches 95 characters.
 * Additional characters cause the console to emit a bell.
 * Other bytes are ignored. Command handlers run in the caller's task context.
 */
void console_poll(void);
/**
 * @brief Format and write at most 255 bytes, without adding a newline.
 * @param fmt Supply a non-NULL format string that follows printf() conventions.
 * @note Longer formatted output is silently truncated.
 */
void console_printf(const char *fmt, ...);
/**
 * @brief Format and write output using a va_list with the same limit as console_printf().
 * @param fmt Supply a non-NULL format string that follows printf() conventions.
 * @param ap Supply the arguments consumed by vsnprintf(). The caller must call va_end().
 */
void console_vprintf(const char *fmt, va_list ap);
/**
 * @brief Write a string verbatim, without adding a newline or moving the prompt.
 * @param s Supply a non-NULL string terminated by a NUL byte. The 255-byte formatting
 * limit does not apply to this string.
 */
void console_puts(const char *s);
/**
 * @brief Move off a visible prompt before a batch of background log output.
 * @note This function preserves the partial input. Call console_after_async_output()
 * after writing the background messages.
 */
void console_before_async_output(void);
/**
 * @brief Redraw the prompt and partial input after background output.
 * @note The caller must finish its log text with a suitable line ending.
 */
void console_after_async_output(void);

#endif

/** @} */
