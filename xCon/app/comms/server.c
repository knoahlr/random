/*
 * server.c
 *
 *  Created on: Jun 28, 2021
 *      Author: Noah Workstation
 */

/* XDCtools Header files */
#include <comms/connection_manager.h>
#include <comms/comms_config.h>
#include <input/gamepad_input.h>
#include <hardware/stepper_control.h>
#include <comms/server.h>
#include <xdc/std.h>
#include <xdc/runtime/System.h>
#include <ti/sysbios/knl/Semaphore.h>
#include <ti/sysbios/knl/Mailbox.h>
#include <ti/sysbios/knl/Clock.h>
#include <ti/sysbios/hal/Seconds.h>
#include <utils/uartstdio.h>
#include "socket.h"
#include "udp_discovery.h"
#include "inet.h"

static bool conn_active = false;

// Helper to connect as client (new function)
int connect_to_relay_app(const char* app_ip, int app_port) {
    int sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(sockfd < 0) return -1;
    sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(app_port);
    serv_addr.sin_addr.s_addr = inet_addr_simple(app_ip);
    if(connect(sockfd, (sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        close(sockfd);
        return -1;
    }
    return sockfd;
}

bool is_conn_active(int client_fd){
    uint8_t ping_message[] = "stormblessed";
    int bytes_sent = send(client_fd, ping_message, strlen((char*)ping_message), 0);
    conn_active = (bytes_sent > 0);
    uart_log("Connection Status:%s\n", conn_active ? "Active" : "Not Active");
    return conn_active;
}

void tcp_message_handler(int server, Mailbox_Handle mail) {

  int         bytes_rcvd;
  int         bytes_sent;
  int         client_fd = -1;

  sockaddr_in client_addr;
  socklen_t   addrlen = sizeof(client_addr);

  uint8_t        buffer[APP_TCP_PACKET_SIZE];

  int listen_rc = listen(server, 0);
  if (listen_rc < 0) {
    uart_log("Error: listen failed.\n");
    return;
  }

  for(;;) {
    client_fd = accept(server, (sockaddr *)&client_addr, &addrlen);
    if (client_fd < 0) {
        Task_sleep(100);
        continue;
    }

    uint32_t time_at_accept_s = Seconds_get();
    uint32_t time_since_last_byte = time_at_accept_s;

    while(client_fd >= 0) {

        GPIO_toggle(Board_LED1);
        bytes_rcvd = recv(client_fd, buffer, APP_TCP_PACKET_SIZE, 4);

        Gamepad gamepad_state = { 0 };

        while ( bytes_rcvd > 0 ) {
            time_since_last_byte = Seconds_get();
            bool command_frame_parse_rc = command_frame_parse(&gamepad_state, buffer, sizeof(buffer));
            memset(buffer, 0, APP_TCP_PACKET_SIZE);

            if(command_frame_parse_rc) {
                bool rc = Mailbox_post(mail, &gamepad_state, MAILBOX_TIMEOUT);
                if(!rc) {
                    uart_log("Error: Unable to post gamepad state to mailbox.\n");
                }
            }

            bytes_sent = send(client_fd, gamepad_state.status, sizeof(gamepad_state.status), 0);

            if (bytes_sent < 0) {
              uart_log("Error: send failed.\n");
              close(client_fd);
              client_fd = -1;
              break;
            }
            bytes_rcvd = recv(client_fd, buffer, APP_TCP_PACKET_SIZE, 4);
        }
        Task_sleep(100);

        // if 10 seconds has passed without a message check that connection is still active
        if((Seconds_get() - time_since_last_byte) > CONN_RETRY_TIMER_S){
            if(!is_conn_active(client_fd)) {
                close(client_fd);
                client_fd = -1;
            }
        }
    }
  }
}

// --- Main dual-mode loop ---
void server_task(UArg arg0, UArg arg1)
{
    _u32        semaphore_timeout;
    bool        semaphore_post_status = false;
    struct sockaddr_in local_addr;
    int         status;
    int server = -1;
    server_state_t current_mode = SERVER_MODE;
    semaphore_timeout = 20000 * Clock_tickPeriod;
    Semaphore_Handle sem = (Semaphore_Handle)arg1;
    Mailbox_Handle mail = (Mailbox_Handle)arg0;
    // Init UDP discovery
    udp_discovery_init();

    // Wait for network connection/IP acquired
    semaphore_post_status = Semaphore_pend(sem, semaphore_timeout);
    if (!semaphore_post_status) return;
    GPIO_write(Board_LED1, 0);

    uint32_t last_cycle = Seconds_get();
    relay_app_beacon_t relay = {0};
    while(1) {
        // --- Periodic UDP beacon & scan (every second) ---
        if (Seconds_get() - last_cycle >= 1) {
            udp_send_beacon();
            if (udp_check_for_app_beacon(&relay) && relay.valid) {
                current_mode = CLIENT_MODE;
            }
            last_cycle = Seconds_get();
        }
        if (current_mode == SERVER_MODE) {
            // Open or restart server socket
            if (server < 0) {
                server = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
                if (server < 0) continue;
                memset(&local_addr, 0, sizeof(local_addr));
                local_addr.sin_family = AF_INET;
                local_addr.sin_addr.s_addr = htonl(INADDR_ANY);
                local_addr.sin_port = htons(APP_TCP_SERVER_PORT);
                status = bind(server, (const struct sockaddr *)&local_addr, sizeof(local_addr));
                if (status == -1) { close(server); server = -1; continue; }
            }
            tcp_message_handler(server, mail);
        } else if (current_mode == CLIENT_MODE) {
            int client_fd = connect_to_relay_app(relay.ip, relay.port);
            if (client_fd >= 0) {
                char handshake[] = "{\"type\":\"tm4c_connect\",\"device_id\":\"" APP_DEVICE_ID "\"}";
                send(client_fd, handshake, strlen(handshake), 0);
                uint8_t buffer[APP_TCP_PACKET_SIZE];
                int bytes_rcvd;
                while ((bytes_rcvd = recv(client_fd, buffer, APP_TCP_PACKET_SIZE, 0)) > 0) {
                    Gamepad gamepad_state = { 0 };
                    bool command_frame_parse_rc = command_frame_parse(&gamepad_state, buffer, sizeof(buffer));
                    memset(buffer, 0, APP_TCP_PACKET_SIZE);
                    if(command_frame_parse_rc) {
                        bool rc = Mailbox_post(mail, &gamepad_state, MAILBOX_TIMEOUT);
                        if(!rc) {
                            uart_log("Error: Unable to post gamepad state to mailbox.\n");
                        }
                    }
                    send(client_fd, gamepad_state.status, sizeof(gamepad_state.status), 0);
                }
                close(client_fd);
            }
            current_mode = SERVER_MODE;
            server = -1;
        }
    }
}
