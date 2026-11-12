/**
 * @file console.c
 * @brief Edit command lines in a fixed buffer, dispatch commands, and restore the prompt.
 * @ingroup console_api
 */
#include <console/console.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <console/cmdline.h>
#include <utils/uartstdio.h>

/** @brief Define the prompt displayed at startup and after command or log output. */
#define CONSOLE_PROMPT          "$uart-0: "
/** @brief Define the input capacity, including the NUL byte reserved for dispatch. */
#define CONSOLE_INPUT_MAX       96
/** @brief Define the formatting buffer capacity, including the terminating NUL byte. */
#define CONSOLE_PRINT_BUF_MAX   256

/** @brief Store the partial input line. A terminating NUL byte is guaranteed only at dispatch. */
static char console_input[CONSOLE_INPUT_MAX];
/** @brief Track the number of valid bytes in console_input, excluding the terminator. */
static uint32_t console_input_len;
/** @brief Track whether output currently ends on an interactive prompt line. */
static bool prompt_visible;

/** @brief Forward a nonempty byte span to buffered UART output. */
static void console_write(const char *s, uint32_t len)
{
    if (len > 0) {
        UARTwrite(s, len);
    }
}

void console_puts(const char *s)
{
    console_write(s, strlen(s));
}

void console_vprintf(const char *fmt, va_list ap)
{
    char buf[CONSOLE_PRINT_BUF_MAX];
    int len = vsnprintf(buf, sizeof(buf), fmt, ap);

    if (len <= 0) {
        return;
    }

    if (len >= (int)sizeof(buf)) {
        len = (int)sizeof(buf) - 1;
    }
    console_write(buf, (uint32_t)len);
}

void console_printf(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    console_vprintf(fmt, ap);
    va_end(ap);
}

/** @brief Emit the prompt and mark it visible. */
static void console_prompt(void)
{
    console_puts(CONSOLE_PROMPT);
    prompt_visible = true;
}

/** @brief Compare a command name against a case-sensitive byte prefix. */
static bool command_matches_prefix(const char *cmd, const char *prefix,
                                   uint32_t prefix_len)
{
    return strncmp(cmd, prefix, prefix_len) == 0;
}

/**
 * @brief List commands matching the text before the first space, then redraw.
 * An empty prefix lists every command. The input remains unchanged even when
 * only one command matches. Arguments are not completed. If no command matches,
 * the console redraws the prompt without printing a diagnostic.
 */
static void console_print_matches(void)
{
    uint32_t prefix_len = 0;

    while ((prefix_len < console_input_len) &&
           (console_input[prefix_len] != ' ')) {
        prefix_len++;
    }

    console_puts("\r\n");
    for (tCmdLineEntry *entry = g_psCmdTable; entry->pcCmd != NULL; entry++) {
        if ((prefix_len == 0) ||
            command_matches_prefix(entry->pcCmd, console_input, prefix_len)) {
            console_printf("  %-14s %s\r\n", entry->pcCmd, entry->pcHelp);
        }
    }
    console_prompt();
    console_write(console_input, console_input_len);
}

/**
 * @brief Terminate and parse the line, report its result, then reset the editor.
 * Empty input only redraws the prompt. CR and LF are handled independently.
 */
static void console_run_command(void)
{
    int rc;

    prompt_visible = false;

    if (console_input_len == 0) {
        console_puts("\r\n");
        console_prompt();
        return;
    }

    console_puts("\r\n");
    console_input[console_input_len] = '\0';
    rc = CmdLineProcess(console_input);
    switch (rc) {
    case 0:
        break;
    case CMDLINE_BAD_CMD:
        console_puts("unknown command; type 'help' or press tab\r\n");
        break;
    case CMDLINE_TOO_MANY_ARGS:
        console_puts("too many arguments\r\n");
        break;
    case CMDLINE_TOO_FEW_ARGS:
        console_puts("too few arguments\r\n");
        break;
    case CMDLINE_INVALID_ARG:
        console_puts("invalid argument\r\n");
        break;
    default:
        console_printf("command failed: %d\r\n", rc);
        break;
    }

    console_input_len = 0;
    console_prompt();
}

/** @brief Remove and visually erase the last input byte, if present. */
static void console_backspace(void)
{
    if (console_input_len == 0) {
        return;
    }

    console_input_len--;
    console_puts("\b \b");
}

/** @brief Append and echo one printable byte, or ring the bell if full. */
static void console_add_char(unsigned char c)
{
    if (console_input_len >= (CONSOLE_INPUT_MAX - 1)) {
        console_puts("\a");
        return;
    }

    console_input[console_input_len++] = (char)c;
    console_write((const char *)&c, 1);
}

void console_init(void)
{
    console_input_len = 0;
    prompt_visible = false;
    UARTEchoSet(false);
    console_prompt();
}

void console_poll(void)
{
    while (UARTRxBytesAvail() > 0) {
        unsigned char c = UARTgetc();

        if ((c == '\r') || (c == '\n')) {
            console_run_command();
        } else if ((c == '\b') || (c == 0x7f)) {
            console_backspace();
        } else if (c == '\t') {
            console_print_matches();
        } else if ((c >= ' ') && (c <= '~')) {
            console_add_char(c);
        }
    }
}

void console_before_async_output(void)
{
    if (prompt_visible) {
        console_puts("\r\n");
        prompt_visible = false;
    }
}

void console_after_async_output(void)
{
    if (prompt_visible) {
        console_puts("\r\n");
    }

    console_prompt();
    console_write(console_input, console_input_len);
}
