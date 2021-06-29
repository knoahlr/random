/*
 * GamepadInput.h
 *
 *  Created on: Mar 18, 2020
 *      Author: Noah Workstation
 */

#include "stdbool.h"

#ifndef GAMEPADINPUT_H_
#define GAMEPADINPUT_H_

#define TCPPACKETSIZE   256

typedef struct  ControlInterpreter
    {
            int LeftTrigger;
            int RightTrigger;
            int LeftAnalog;
            int RightAnalog;
            int RightButton;
            int LeftButton;
            bool valid;
} Gamepad;


void commandFrameParse(Gamepad *gamepad, char Input[]);

#endif /* GAMEPADINPUT_H_ */
