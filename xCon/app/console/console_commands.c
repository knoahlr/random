/**
 * @file console_commands.c
 * @brief Implement application commands and register their names, handlers, and help text.
 * @ingroup console_api
 *
 * Handlers execute synchronously in the UART task. argc includes the command
 * name. argv points into the parser's mutable input. Handlers must not retain
 * these pointers.
 */
#include <console/cmdline.h>
#include <console/console.h>

#include <stdint.h>
#include <string.h>

#include <comms/connection_manager.h>
#include <comms/server.h>

/**
 * @brief List all registered commands and ignore extra arguments.
 * @param argc Supply the number of tokens, including the command name.
 * @param argv Supply the tokens stored in the input buffer.
 * @return This function always returns zero.
 */
static int cmd_help(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    for (tCmdLineEntry *entry = g_psCmdTable; entry->pcCmd != NULL; entry++) {
        console_printf("  %-14s %s\r\n", entry->pcCmd, entry->pcHelp);
    }
    return 0;
}

/**
 * @brief Request a WPA/WPA2 connection without storing a profile.
 * @param argc Supply the number of tokens, including the command name.
 * @param argv Supply the tokens stored in the input buffer.
 * @return This function returns zero on success or CMDLINE_INVALID_ARG if the
 * arguments are invalid. It returns the negative connection manager error code
 * if the requested operation fails.
 */
static int cmd_connect(int argc, char *argv[])
{
    int16_t rc;

    if (argc != 3) {
        console_puts("usage: connect <ssid> <passkey>\r\n");
        return CMDLINE_INVALID_ARG;
    }

    rc = cm_connect_wpa2_ap(argv[1], argv[2]);
    if (rc == CM_PROFILE_INVALID_ARG) {
        console_puts("ssid must be 1-32 chars; WPA/WPA2 passkey must be 8-64 chars\r\n");
        return CMDLINE_INVALID_ARG;
    }
    if (rc < 0) {
        console_printf("connect failed: %d\r\n", rc);
        return rc;
    }

    console_printf("connect requested for '%s'\r\n", argv[1]);
    return 0;
}

/**
 * @brief Store a WPA/WPA2 profile for the auto-connect policy.
 * @param argc Supply the number of tokens, including the command name.
 * @param argv Supply the tokens stored in the input buffer.
 * @return This function returns zero on success or CMDLINE_INVALID_ARG if the
 * arguments are invalid. It returns the negative connection manager error code
 * if the requested operation fails.
 */
static int cmd_add_profile(int argc, char *argv[])
{
    int16_t rc;

    if (argc != 3) {
        console_puts("usage: add-profile <ssid> <passkey>\r\n");
        return CMDLINE_INVALID_ARG;
    }

    rc = cm_add_wpa2_profile(argv[1], argv[2]);
    if (rc == CM_PROFILE_INVALID_ARG) {
        console_puts("ssid must be 1-32 chars; WPA/WPA2 passkey must be 8-64 chars\r\n");
        return CMDLINE_INVALID_ARG;
    }
    if (rc < 0) {
        console_printf("profile add failed: %d\r\n", rc);
        return rc;
    }

    console_printf("stored profile '%s' at index %d\r\n", argv[1], rc);
    console_puts("auto-connect policy will use the stored profile\r\n");
    return 0;
}

/**
 * @brief Reload and display the stored WiFi profiles.
 * @param argc Supply the number of tokens, including the command name.
 * @param argv Supply the tokens stored in the input buffer.
 * @return This function returns zero on success or CMDLINE_INVALID_ARG if the
 * arguments are invalid.
 */
static int cmd_profiles(int argc, char *argv[])
{
    uint8_t count;

    (void)argv;

    if (argc != 1) {
        console_puts("usage: profiles\r\n");
        return CMDLINE_INVALID_ARG;
    }

    count = cm_load_saved_profiles();
    console_printf("%u stored profile(s)\r\n", count);
    cm_print_configured_profiles();
    return 0;
}

/**
 * @brief Request the current WiFi connection status.
 * @param argc Supply the number of tokens, including the command name.
 * @param argv Supply the tokens stored in the input buffer.
 * @return This function returns zero on success or CMDLINE_INVALID_ARG if the
 * arguments are invalid.
 */
static int cmd_status(int argc, char *argv[])
{
    (void)argv;

    if (argc != 1) {
        console_puts("usage: status\r\n");
        return CMDLINE_INVALID_ARG;
    }

    (void)cm_status();
    return 0;
}

/**
 * @brief Request the command-server status.
 * @param argc Supply the number of tokens, including the command name.
 * @param argv Supply the tokens stored in the input buffer.
 * @return This function returns zero on success or CMDLINE_INVALID_ARG if the
 * arguments are invalid.
 */
static int cmd_server(int argc, char *argv[])
{
    (void)argv;

    if (argc != 1) {
        console_puts("usage: server\r\n");
        return CMDLINE_INVALID_ARG;
    }

    server_status();
    return 0;
}

/**
 * @brief Request deletion of all WiFi profiles and reload the cache.
 * @param argc Supply the number of tokens, including the command name.
 * @param argv Supply the tokens stored in the input buffer.
 * @return This function returns zero on success or CMDLINE_INVALID_ARG if the
 * arguments are invalid.
 */
static int cmd_clear_profiles(int argc, char *argv[])
{
    (void)argv;

    if (argc != 1) {
        console_puts("usage: clear-profiles\r\n");
        return CMDLINE_INVALID_ARG;
    }

    cm_remove_all_connection_profiles();
    cm_load_saved_profiles();
    console_puts("requested profile delete\r\n");
    return 0;
}

/**
 * @brief Define commands in the order used for dispatch, help, and Tab suggestions.
 * @note Each command requires a name, handler, and help string. The final entry
 * must have a NULL name. Names are matched exactly and case-sensitively.
 */
tCmdLineEntry g_psCmdTable[] = {
    { "help",           cmd_help,           "list console commands" },
    { "connect",        cmd_connect,        "connect <ssid> <passkey>" },
    { "add-profile",    cmd_add_profile,    "add-profile <ssid> <passkey>" },
    { "status",         cmd_status,         "show WiFi connection status" },
    { "server",         cmd_server,         "show command-server status" },
    { "profiles",       cmd_profiles,       "list stored WiFi profiles" },
    { "clear-profiles", cmd_clear_profiles, "delete stored WiFi profiles" },
    { NULL,             NULL,               NULL }
};
