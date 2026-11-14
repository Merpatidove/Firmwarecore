/* pico_st7789 — board constants for Raspberry Pi Pico (RP2040) / Pico 2
 * (RP2350) + external ST7789 SPI panel. Free wiring; defaults below are a
 * common hookup, override via build_flags. TN panels: BOARD_PANEL_IPS=false. */
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
#define BOARD_PANEL_IPS     true

/* ---- Board wires (free choice on RP2040; defaults below) ---- */
#define BOARD_PIN_SCK   10
#define BOARD_PIN_MOSI  11
#define BOARD_PIN_CS    9
#define BOARD_PIN_DC    8
#define BOARD_PIN_RST   12
#define BOARD_PIN_BL    13
#define BOARD_PIN_BTN   14

/* ---- MCU memory budget: RP2040 SRAM 264KB total ---- */
#define BOARD_BUF_SIZE      (96 * 1024)
#define BOARD_MAX_SECTIONS  64

/* ---- Serial: USB CDC via earlephilhower core (Serial IS the USB port) ---- */
#define BOARD_SERIAL_BAUD   115200

/* ---- Render tuning (shared renderer, rsvp_render.h) ---- */
#define BOARD_FONT      4
#define BOARD_CHAR_W    (6 * BOARD_FONT)
#define BOARD_PIVOT_X   (BOARD_LCD_W * 3 / 10)

#endif /* BOARD_H */
