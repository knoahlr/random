/*
 * gamepad_input.c
 *
 *  Created on: Mar 18, 2020
 *      Author: Noah Workstation
 */

#include <input/gamepad_input.h>

enum controller_state {
    FULL_STATE     = 7,
    THROTTLE_STATE = 17,
};

// Data Format: [FSS, CommandID, Len, RT, LT, RB, LB, RA.x, RA.y, LA.x, LA.y, X, A, B, Y]
// Frame Synchronization Sequence: [255, 255, 255]
bool command_frame_parse(Gamepad *gamepad, uint8_t *input, size_t input_buffer_size)
{
    if ((gamepad == NULL) || (input == NULL) || (input_buffer_size < 5)) {
        return false;
    }

    uint8_t truncated_data[24];
    memset(truncated_data, 0, sizeof(truncated_data));

    // loop through buffer until FSS is found. Current FSS is [255, 255, 255]
    size_t buffer_index = 0;
    while (buffer_index + 2 < input_buffer_size)
    {
        if (input[buffer_index] == 255 && input[buffer_index + 1] == 255 && input[buffer_index + 2] == 255)
        {
            // found FSS
            if (buffer_index + 5 <= input_buffer_size)
            {
                // Found frame length in buffer
                uint8_t frame_length = input[buffer_index + 4]; // bufferIndex + length of FSS + 1 (commandID)
                size_t  frame_copy_size = (size_t)frame_length + 2U;
                if ((frame_copy_size <= sizeof(truncated_data)) &&
                    (buffer_index + 5U + frame_length <= input_buffer_size))
                {
                    // An entire frame has been found.
                    // +3 so truncated_data starts at CommandID; frame_length+2 covers
                    // the CommandID and FrameLength bytes.
                    memcpy(truncated_data, input + buffer_index + 3, frame_copy_size);
                    break;
                }
            }
            return false; // unable to find a full frame in the buffer
        }
        buffer_index++;
    }
    if (buffer_index + 2 >= input_buffer_size) {
        return false;
    }

    bool parsed = false;
    switch (truncated_data[0])
    {
        case FULL_STATE:
            gamepad->right_trigger  = truncated_data[2];
            gamepad->left_trigger   = truncated_data[3];
            gamepad->right_button   = truncated_data[4];
            gamepad->left_button    = truncated_data[5];
            gamepad->right_analog_x = truncated_data[6];
            gamepad->right_analog_y = truncated_data[7];
            gamepad->left_analog_x  = truncated_data[8];
            gamepad->left_analog_y  = truncated_data[9];
            gamepad->x_button       = truncated_data[10];
            gamepad->a_button       = truncated_data[11];
            gamepad->b_button       = truncated_data[12];
            gamepad->y_button       = truncated_data[13];
            gamepad->valid          = true;
            snprintf(gamepad->status, sizeof(gamepad->status),
                    "RT:%d LT:%d RB:%d LB:%d RA.X:%d RA.Y:%d LA.X:%d LA.Y:%d X:%d A:%d B:%d Y:%d",
                    gamepad->right_trigger, gamepad->left_trigger, gamepad->right_button,
                    gamepad->left_button, gamepad->right_analog_x, gamepad->right_analog_y,
                    gamepad->left_analog_x, gamepad->left_analog_y, gamepad->x_button,
                    gamepad->a_button, gamepad->b_button, gamepad->y_button);
            parsed = true;
            break;
        default:
            /* Unrecognized CommandID: don't report a (zeroed) frame as valid,
             * otherwise the motor task would be driven with all-zero input. */
            gamepad->valid = false;
            break;
    }
    memset(truncated_data, 0, sizeof(truncated_data));
    return parsed;
}
