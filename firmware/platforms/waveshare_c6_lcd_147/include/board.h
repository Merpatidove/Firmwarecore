/* waveshare_c6_lcd_147 — board constants for the Waveshare ESP32-C6-LCD-1.47
 * NON-TOUCH. Every hardware fact lives here; main.cpp has no magic numbers.
 * Pinmap verified on the physical board (photos + flash logs, Nov 2026). */
#ifndef BOARD_H
#define BOARD_H

/* ---- Panel: ST7789V, 1.47" IPS, native 172x320, GRAM 240x320 ---- */
#define BOARD_LCD_NATIVE_W  172
#define BOARD_LCD_NATIVE_H  320
#define BOARD_LCD_COL_OFF   34         /* (240-172)/2 on the short axis */
#define BOARD_LCD_ROW_OFF   0
#define BOARD_LCD_ROT       1          /* landscape: logical 320x172, constant */
#define BOARD_LCD_W         320        /* logical (post-rotation) */
#define BOARD_LCD_H         172
#define BOARD_PANEL_IPS     true

/* ---- Board wires (SPI + buttons), physical pin numbers ---- */
#define BOARD_PIN_SCK   7
#define BOARD_PIN_MOSI  6
#define BOARD_PIN_CS    14
#define BOARD_PIN_DC    15
#define BOARD_PIN_RST   21
#define BOARD_PIN_BL    22
#define BOARD_PIN_BTN   9   /* BOOT button */

/* ---- MCU memory budget: ESP32-C6 app DRAM ~320KB ---- */
#define BOARD_BUF_SIZE      (192 * 1024)
#define BOARD_MAX_SECTIONS  128

/* ---- Serial: native USB-CDC (NOT UART0). Requires
 * ARDUINO_USB_CDC_ON_BOOT=1 + ARDUINO_USB_MODE=1 in platformio.ini, else
 * host writes stall after the 64B HW FIFO fills. ---- */
#define BOARD_SERIAL_BAUD   115200

/* ---- Render tuning (shared renderer, rsvp_render.h) ---- */
#define BOARD_FONT      4                     /* 24px glyphs */
#define BOARD_CHAR_W    (6 * BOARD_FONT)
#define BOARD_PIVOT_X   (BOARD_LCD_W * 3 / 10)
#define BOARD_STATUS_RGB565  0x8C73           /* medium grey #8a8f98 */

#endif /* BOARD_H */
