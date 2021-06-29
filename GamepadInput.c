/*
 * GamepadInput.c
 *
 *  Created on: Mar 18, 2020
 *      Author: Noah Workstation
 */

#include "GamepadInput.h"

#define FULL_STATE 17


void commandFrameParse(Gamepad *gamepad, char Input[])
{
    int ObjectID = Input[0] << 8 | Input [1];
    int length = Input[2] << 8 | Input [3];
    switch (ObjectID)
    {
        case FULL_STATE:
            gamepad->RightTrigger = Input[0];
            gamepad->LeftTrigger = Input[1];
            gamepad->RightButton = Input[2];
            gamepad->LeftButton = Input[3];
            gamepad->RightAnalog = Input[4];
            gamepad->RightAnalog = Input[5];
            gamepad->valid = true;
            break;
        default:
            gamepad->valid = false;
            break;
    }
}


