/*
 * dc_motor_control.h
 *
 *  Created on: Jul 20, 2021
 *      Author: Noah Workstation
 */
#ifndef APP_HARDWARE_DC_MOTOR_CONTROL_H
#define APP_HARDWARE_DC_MOTOR_CONTROL_H

#include <xdc/std.h>

/*
 * Motor control task. arg0 = Mailbox_Handle carrying parsed Gamepad frames.
 * Opens the motor PWM channels and drives duty from the right trigger, with a
 * failsafe that stops the motors if no command arrives within the timeout.
 */
void pwm_motor_proc_init(UArg arg0, UArg arg1);

#endif /* APP_HARDWARE_DC_MOTOR_CONTROL_H */
