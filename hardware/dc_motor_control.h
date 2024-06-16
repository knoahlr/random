/*
 * hardwareController.h
 *
 *  Created on: Jul 20, 2021
 *      Author: Noah Workstation
 */

#include <gamepad/gamepad_input.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <math.h>
/* XDCtools Header files */
#include <xdc/std.h>
#include <xdc/runtime/System.h>

#include "Board.h"
#include <ti/drivers/PWM.h>
#include <ti/sysbios/knl/clock.h>
#include <ti/sysbios/knl/Mailbox.h>

#include "driverlib/uart.h"

//Hardware controller task

#define MAILBOX_TIMEOUT (20 * Clock_tickPeriod)

void pwm_motor_proc_init(UArg arg0, UArg arg1);

void log_gamepad_input(Gamepad *gamepadState);

void update_motor_state();
