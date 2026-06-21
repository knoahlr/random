#include <console/cmdline.h>
#include <console/console.h>

#include <stdint.h>
#include <string.h>

#include <comms/connection_manager.h>
#include <comms/server.h>

static int cmd_help(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    for (tCmdLineEntry *entry = g_psCmdTable; entry->pcCmd != NULL; entry++) {
        console_printf("  %-14s %s\r\n", entry->pcCmd, entry->pcHelp);
    }
    return 0;
}

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
