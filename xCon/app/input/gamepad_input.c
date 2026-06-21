/*
 * gamepad_input.c
 *
 *  Created on: Mar 18, 2020
 *      Author: Noah Workstation
 *
 * Decodes a relay GamepadState payload into the local Gamepad struct. The wire
 * format is documented byte-for-byte in docs/gamepad-protocol.md; this file owns
 * only the payload decode (the 16-byte message envelope/framing lives in the
 * transport layer, net_session.c).
 */

#include <input/gamepad_input.h>

/* GamepadState payload layout (little-endian), per docs/gamepad-protocol.md:
 *    0  u8  device_id        8  s16 rx (right stick X)
 *    1  u8  flags           10  s16 ry (right stick Y)
 *    2  u16 buttons         12  u16 l2 (left trigger  0..65535)
 *    4  s16 lx (L stick X)  14  u16 r2 (right trigger 0..65535)
 *    6  s16 ly (L stick Y)  16  s16 dpad_x   18 s16 dpad_y   20.. reserved
 */
#define GP_OFF_BUTTONS  2
#define GP_OFF_LX       4
#define GP_OFF_LY       6
#define GP_OFF_RX       8
#define GP_OFF_RY      10
#define GP_OFF_L2      12
#define GP_OFF_R2      14

/* Button bitmask (uint16 at offset 2); PS5/DualSense naming. */
#define BTN_CROSS     0x0001  /* A */
#define BTN_CIRCLE    0x0002  /* B */
#define BTN_SQUARE    0x0004  /* X */
#define BTN_TRIANGLE  0x0008  /* Y */
#define BTN_L1        0x0010  /* LB */
#define BTN_R1        0x0020  /* RB */

static inline uint16_t rd_u16(const uint8_t *p)
{
    return (uint16_t)(p[0] | ((uint16_t)p[1] << 8));
}

static inline int16_t rd_s16(const uint8_t *p)
{
    return (int16_t)rd_u16(p);
}

bool gamepad_parse_payload(Gamepad *gamepad, const uint8_t *payload, size_t length)
{
    if ((gamepad == NULL) || (payload == NULL) ||
        (length < GAMEPAD_PAYLOAD_BASE_SIZE)) {
        return false;
    }

    uint16_t buttons = rd_u16(payload + GP_OFF_BUTTONS);

    gamepad->left_analog_x  = rd_s16(payload + GP_OFF_LX);
    gamepad->left_analog_y  = rd_s16(payload + GP_OFF_LY);
    gamepad->right_analog_x = rd_s16(payload + GP_OFF_RX);
    gamepad->right_analog_y = rd_s16(payload + GP_OFF_RY);

    /* Triggers arrive as uint16 0..65535; the motor maps right_trigger over
     * 0..255, so scale both into a byte range (>> 8) to keep that contract. */
    gamepad->left_trigger   = rd_u16(payload + GP_OFF_L2) >> 8;
    gamepad->right_trigger  = rd_u16(payload + GP_OFF_R2) >> 8;

    gamepad->a_button     = (buttons & BTN_CROSS)    ? 1 : 0;
    gamepad->b_button     = (buttons & BTN_CIRCLE)   ? 1 : 0;
    gamepad->x_button     = (buttons & BTN_SQUARE)   ? 1 : 0;
    gamepad->y_button     = (buttons & BTN_TRIANGLE) ? 1 : 0;
    gamepad->left_button  = (buttons & BTN_L1)       ? 1 : 0;
    gamepad->right_button = (buttons & BTN_R1)       ? 1 : 0;
    gamepad->valid        = true;

    snprintf(gamepad->status, sizeof(gamepad->status),
             "RT:%d LT:%d RB:%d LB:%d RA(%d,%d) LA(%d,%d) X:%d A:%d B:%d Y:%d",
             gamepad->right_trigger, gamepad->left_trigger, gamepad->right_button,
             gamepad->left_button, gamepad->right_analog_x, gamepad->right_analog_y,
             gamepad->left_analog_x, gamepad->left_analog_y, gamepad->x_button,
             gamepad->a_button, gamepad->b_button, gamepad->y_button);
    return true;
}
