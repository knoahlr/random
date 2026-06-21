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


/* Base GamepadState payload size, see docs/gamepad-protocol.md. */
#define GAMEPAD_PAYLOAD_BASE_SIZE 26

/*
 * Decode a GamepadState payload (the bytes after the 16-byte message envelope)
 * into `gamepad`. `length` is the envelope's payload byte count; the base form
 * is 26 bytes (sensor frames are 38 and decode the same base fields). All
 * multi-byte fields are little-endian. Returns false if the payload is short.
 */
bool gamepad_parse_payload(Gamepad *gamepad, const uint8_t *payload, size_t length);

#endif /* GAMEPADINPUT_H_ */
