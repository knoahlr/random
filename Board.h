/*
 * Copyright (c) 2015, Texas Instruments Incorporated
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __BOARD_H
#define __BOARD_H

#ifdef __cplusplus
extern "C" {
#endif

#include "EK_TM4C129EXL.h"

#define Board_initEMAC              EK_TM4C129EXL_initEMAC
#define Board_initGeneral           EK_TM4C129EXL_initGeneral
#define Board_initGPIO              EK_TM4C129EXL_initGPIO
#define Board_initI2C               EK_TM4C129EXL_initI2C
#define Board_initPWM               EK_TM4C129EXL_initPWM
#define Board_initSDSPI             EK_TM4C129EXL_initSDSPI
#define Board_initSPI               EK_TM4C129EXL_initSPI
#define Board_initUART              EK_TM4C129EXL_initUART
#define Board_initUSB               EK_TM4C129EXL_initUSB
#define Board_initUSBMSCHFatFs      EK_TM4C129EXL_initUSBMSCHFatFs
#define Board_initWatchdog          EK_TM4C129EXL_initWatchdog
#define Board_initWiFi              EK_TM4C129EXL_initWiFi

#define Board_LED_ON                EK_TM4C129EXL_LED_ON
#define Board_LED_OFF               EK_TM4C129EXL_LED_OFF
#define Board_LED0                  EK_TM4C129EXL_D1
#define Board_LED1                  EK_TM4C129EXL_D2
#define Board_LED2                  EK_TM4C129EXL_D2
#define Board_BUTTON0               EK_TM4C129EXL_USR_SW1
#define Board_BUTTON1               EK_TM4C129EXL_USR_SW2

#define Board_I2C0                  EK_TM4C129EXL_I2C7
#define Board_I2C1                  EK_TM4C129EXL_I2C8
#define Board_I2C_TMP               EK_TM4C129EXL_I2C7
#define Board_I2C_NFC               EK_TM4C129EXL_I2C7
#define Board_I2C_TPL0401           EK_TM4C129EXL_I2C7

#define Board_PWM0                  EK_TM4C129EXL_PWM0
#define Board_PWM1                  EK_TM4C129EXL_PWM1
#define Board_PWM2                  EK_TM4C129EXL_PWM2
#define Board_PWM3                  EK_TM4C129EXL_PWM3
#define Board_PWM4                  EK_TM4C129EXL_PWM4
#define Board_PWM5                  EK_TM4C129EXL_PWM5
#define Board_PWM6                  EK_TM4C129EXL_PWM6
#define Board_PWM7                  EK_TM4C129EXL_PWM7


#define Board_SDSPI0                EK_TM4C129EXL_SDSPI0
#define Board_SDSPI1                EK_TM4C129EXL_SDSPI1

#define Board_SPI0                  EK_TM4C129EXL_SPI2
#define Board_SPI1                  EK_TM4C129EXL_SPI3

#define Board_USBMSCHFatFs0         EK_TM4C129EXL_USBMSCHFatFs0

#define Board_USBHOST               EK_TM4C129EXL_USBHOST
#define Board_USBDEVICE             EK_TM4C129EXL_USBDEVICE

#define Board_UART0                 EK_TM4C129EXL_UART0

#define Board_WATCHDOG0             EK_TM4C129EXL_WATCHDOG0

#define Board_WIFI                  EK_TM4C129EXL_WIFI
#define Board_WIFI_SPI              EK_TM4C129EXL_SPI2

/* Board specific I2C addresses */
#define Board_TMP006_ADDR           (0x40)
#define Board_RF430CL330_ADDR       (0x28)
#define Board_TPL0401_ADDR          (0x40)

#ifdef __cplusplus
}
#endif

#endif /* __BOARD_H */
