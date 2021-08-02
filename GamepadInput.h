/*
 * GamepadInput.h
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

#define TCPPACKETSIZE   256

typedef struct  ControlInterpreter
    {
            int LeftTrigger;
            int RightTrigger;
            int LeftAnalog_X;
            int LeftAnalog_Y;
            int RightAnalog_X;
            int RightAnalog_Y;
            int RightButton;
            int LeftButton;
            int X_Button;
            int A_Button;
            int B_Button;
            int Y_Button;
            bool valid;
            char status[100];
} Gamepad;


bool commandFrameParse(Gamepad *gamepad, char Input[], size_t inputBufferSize);

#endif /* GAMEPADINPUT_H_ */
