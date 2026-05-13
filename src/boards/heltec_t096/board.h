/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2018 Ha Thach for Adafruit Industries
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#ifndef _HELTEC_T096_H
#define _HELTEC_T096_H

#define _PINNUM(port, pin)    ((port)*32 + (pin))

/*------------------------------------------------------------------*/
/* LED
 *------------------------------------------------------------------*/
#define LEDS_NUMBER           1
#define LED_PRIMARY_PIN       _PINNUM(0, 28) // White LED
#define LED_STATE_ON          1              // active high

/*------------------------------------------------------------------*/
/* BUTTON
 * Both BUTTON_1 (BUTTON_DFU) and BUTTON_2 (BUTTON_FRESET) map to P1.10.
 * This is intentional and matches T114's pattern: T096 has only one
 * user-programmable button, so a single press satisfies both the DFU
 * and FRESET conditions in main.c, triggering OTA mode directly.
 * P0.18 (Reset) is the hardware nRESET line, not a GPIO button.
 *------------------------------------------------------------------*/
#define BUTTONS_NUMBER        2
#define BUTTON_1              _PINNUM(1, 10) // User button (BUTTON_DFU)
#define BUTTON_2              _PINNUM(1, 10) // Same pin (BUTTON_FRESET) — intentional
#define BUTTON_PULL           NRF_GPIO_PIN_PULLUP

//--------------------------------------------------------------------+
// Display — ST7735 not yet supported in bootloader.
// T096 has a 0.96" ST7735S TFT (160x80) on pins:
//   SCK=P0.20, MOSI=P0.17, CS=P0.22, DC=P0.15, RST=P0.13
//   BL=P1.12, power via Vext_Ctrl=P0.26 (active high)
// Status LED on P0.28 used instead for bootloader feedback.
//--------------------------------------------------------------------+

//--------------------------------------------------------------------+
// BLE OTA
//--------------------------------------------------------------------+
#define BLEDIS_MANUFACTURER   "Heltec AutoMation"
#define BLEDIS_MODEL          "HT-n5262G"

//--------------------------------------------------------------------+
// USB
// Note: PID 0x0071 is shared with the Heltec T114. Heltec ships the
// stock T096 bootloader using the same PID as T114 (their oversight).
// We match it so the bootloader self-update via UF2 drag-and-drop works
// out of the box for stock T096 boards. Trade-off: T114 and T096 are
// not distinguishable via USB descriptors when both are connected.
//--------------------------------------------------------------------+
#define USB_DESC_VID           0x239A
#define USB_DESC_UF2_PID       0x0071
#define USB_DESC_CDC_ONLY_PID  0x0071

//------------- UF2 -------------//
#define UF2_PRODUCT_NAME      "HT-n5262G"
#define UF2_VOLUME_LABEL      "HT-n5262G"
#define UF2_BOARD_ID          "HT-n5262G"
#define UF2_INDEX_URL         "https://heltec.org/project/t096/"

#endif
