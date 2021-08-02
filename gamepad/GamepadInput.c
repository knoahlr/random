/*
 * GamepadInput.c
 *
 *  Created on: Mar 18, 2020
 *      Author: Noah Workstation
 */

#include <gamepad/GamepadInput.h>

//#define FULL_STATE 7

enum ControllerState {
    FULL_STATE = 7,
    THROTTLE_STATE = 17,
};

//Data Format: [FSS, CommandID, Len, RT, LT, RB, LB, RA.x, RA.y, LA.x, LA.y, X, A, B, Y]
//Frame Synchronization Sequence: [255, 255, 255]
bool commandFrameParse(Gamepad *gamepad, char Input[], size_t inputBufferSize)
{

//    char bufferDuplicate[100];

    char truncatedData[24];
    memset(truncatedData, 0, sizeof(truncatedData));
//    memcpy(bufferDuplicate, Input, inputBufferSize);
//    int ObjectID = truncatedData[0];
//    int length = truncatedData[1];

    //loop through buffer until FSS is found. Current FSS is [255, 255, 255]
    int bufferIndex = 0;
    while (1)
    {
        if(Input[bufferIndex] == 255 && Input[bufferIndex+1] == 255 && Input[bufferIndex+2] == 255)
        {
            //found FSS
            if(bufferIndex + 5 < inputBufferSize)
            {
                //Found framelength in buffer
                uint8_t frameLength = Input[bufferIndex+4]; //essentially bufferIndex + length of FSS + 1 (commandID)
                if (bufferIndex+5+frameLength < (inputBufferSize))
                {
                    //An entire frame has been found
                    memcpy(truncatedData, Input+bufferIndex+3, frameLength+2);
                    //add 4 so truncated data contains information from CommandID onwards
                    //framelength+2 to take into account CommandID and FrameLength bytes
                    break;
                }
            }
            return false; //unable to find a full frame in the buffer
        }
        bufferIndex ++;
        if(bufferIndex > sizeof(Input)){return false;}
    }

    switch (7)
    {
        case 7:
            gamepad->RightTrigger = truncatedData[2];
            gamepad->LeftTrigger = truncatedData[3];
            gamepad->RightButton = truncatedData[4];
            gamepad->LeftButton = truncatedData[5];
            gamepad->RightAnalog_X = truncatedData[6];
            gamepad->RightAnalog_Y = truncatedData[7];
            gamepad->LeftAnalog_X = truncatedData[8];
            gamepad->LeftAnalog_Y = truncatedData[9];
            gamepad->X_Button = truncatedData[10];
            gamepad->A_Button = truncatedData[11];
            gamepad->B_Button = truncatedData[12];
            gamepad->Y_Button = truncatedData[13];
            gamepad->valid = true;
            sprintf(gamepad->status, "RT:%d LT:%d RB:%d LB:%d RA.X:%d RA.Y:%d LA.X:%d LA.Y:%d X:%d A:%d B:%d Y:%d",
                    gamepad->RightTrigger, gamepad->LeftTrigger, gamepad->RightButton,
                    gamepad->LeftButton, gamepad->RightAnalog_X, gamepad->RightAnalog_Y, gamepad->LeftAnalog_X,
                    gamepad->LeftAnalog_Y, gamepad->X_Button, gamepad->A_Button, gamepad->B_Button, gamepad->Y_Button);
            break;
        default:
            gamepad->valid = false;
            break;
    }
    memset(truncatedData, 0, sizeof(truncatedData));
    return true;
}


