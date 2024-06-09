/*
 * hardwareController.h
 *
 *  Created on: Jul 20, 2021
 *      Author: Noah Workstation
 */

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

#include "../gamepad/GamepadInput.h"
#include "driverlib/uart.h"

//Hardware controller task

void updateMotorPWMDuty(UArg arg0, UArg arg1);
