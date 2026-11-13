# rsvp-core MCU firmware

Firmware inti RSVP reader untuk MCU — dipisah dari webapp. Binary
companion (chunking/sections) → board lewat protokol serial SOH; board hanya
memutar dan menggambar. C99 freestanding, zero heap, 1.179 byte.

## Struktur

```
rsvp_core.c/h    core playback (pivot ORP, timing, tema, save/load state)
test_core.c      self-check assert (butuh -DRSVP_TEST -DRSVP_EXPOSE_STATE)
fuzz_check.js    validasi algoritma vs engine JS (3000 chunk)
platforms/
  esp32_st7789/  glue ESP32: env c6_lcd_147 (Waveshare C6 non-touch) +
                 esp32_generic
  pico_st7789/   glue RP2040/RP2350: env pico, pico_w (pico2 di-comment,
                 butuh core earlephilhower)
```

## Build

```
cd platforms/esp32_st7789
pio run -e c6_lcd_147            # Waveshare ESP32-C6-LCD-1.47
pio run -e c6_lcd_147 -t upload --upload-port COM4
```

Angka terukur: C6 RAM 65,3% / Flash 26,5% — RP2040 RAM 78,8% / Flash 3,1%.

Pinmap, wiring per board, dan matriks fitur: lihat BOARDS.md di repo utama
(Text-scroller). Protokol serial + render contract: README.md versi lama di
repo utama; ringkasannya ada di komentar `src/main.cpp`.
