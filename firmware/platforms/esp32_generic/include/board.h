/* esp32_generic — board constants for a classic ESP32 DevKit + external
 * ST7789 SPI panel. Defaults are the common VSPI hookup; override via
 * build_flags (-DBOARD_PIN_SCK=25 etc.). TN panels: BOARD_PANEL_IPS=false. */
#ifndef BOARD_H
#define BOARD_H

/* ---- Panel: ST7789 240x320 (no offset on bare breakouts) ---- */
#define BOARD_LCD_NATIVE_W  240
#define BOARD_LCD_NATIVE_H  320
#define BOARD_LCD_COL_OFF   0
#define BOARD_LCD_ROW_OFF   0
#define BOARD_LCD_ROT       0          /* portrait; set 1 for landscape */
#define BOARD_LCD_W         240
#define BOARD_LCD_H         320
#define BOARD_PANEL_IPS     true       /* false for TN panels */

/* ---- Board wires (VSPI defaults) ---- */
#define BOARD_PIN_SCK   18
#define BOARD_PIN_MOSI  23
#define BOARD_PIN_CS    5
#define BOARD_PIN_DC    2
#define BOARD_PIN_RST   4
#define BOARD_PIN_BL    15
#define BOARD_PIN_BTN   0   /* BOOT button */

/* ---- MCU memory budget: classic ESP32 DRAM is tight (~160KB free) ---- */
#define BOARD_BUF_SIZE      (96 * 1024)
#define BOARD_MAX_SECTIONS  64

/* ---- Serial: UART0 bridge on devkits — no CDC flags needed ---- */
#define BOARD_SERIAL_BAUD   115200

/* ---- Render tuning (shared renderer, rsvp_render.h) ---- */
#define BOARD_FONT      4
#define BOARD_CHAR_W    (6 * BOARD_FONT)
#define BOARD_PIVOT_X   (BOARD_LCD_W * 3 / 10)

#endif /* BOARD_H */
