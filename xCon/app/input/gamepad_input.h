/*
 * gamepad_input.h
 *
 *  Created on: Mar 18, 2020
 *      Author: Noah Workstation
 */

#include "stdbool.h"
#include "string.h"
#include "stdio.h"
#include "stdint.h"

#ifndef GAMEPADINPUT_H_
#define GAMEPADINPUT_H_

typedef struct ControlInterpreter
{
    int  left_trigger;
    int  right_trigger;
    int  left_analog_x;
    int  left_analog_y;
    int  right_analog_x;
    int  right_analog_y;
    int  right_button;
    int  left_button;
    int  x_button;
    int  a_button;
    int  b_button;
    int  y_button;
    bool valid;
    char status[100];
} Gamepad;


bool command_frame_parse(Gamepad *gamepad, uint8_t *input, size_t input_buffer_size);

#endif /* GAMEPADINPUT_H_ */
