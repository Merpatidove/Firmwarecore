# Board Specs — target hardware untuk firmware `firmware/`

Firmware core (`rsvp_core.c/h`) board-agnostic. Glue layer per board ada di
`firmware/platforms/`. Tiga board target resmi:

| Board | MCU | RAM | Flash | Panel | Status build | Status hardware |
|---|---|---|---|---|---|---|
| **Waveshare ESP32-C6-LCD-1.47** (non-touch) | ESP32-C6 (RISC-V, 160MHz) | 512KB | 4MB | ST7789 172×320, SPI, terpasang | ✅ `c6_lcd_147` | ✅ di tangan, pinmap terverifikasi |
| **ESP32 generik** (DevKit + panel eksternal) | ESP32 dual-core Xtensa 240MHz | 520KB | 4MB+ | ST7789 240×320 SPI, wiring bebas | ✅ `esp32_generic` | belum diuji |
| **Raspberry Pi Pico 2** | RP2350 dual-core Cortex-M33 150MHz | 520KB | 4MB | ST7789 SPI, wiring bebas | ⚠️ env di-comment (butuh platform earlephilhower) | belum diuji |

---

## 1. Waveshare ESP32-C6-LCD-1.47 (board utama, terverifikasi)

> **Hati-hati:** ada DUA produk Waveshare nyaris identik. Yang ini **NON-TOUCH**
> (judul listing tanpa kata "Touch", 4MB flash, tanpa IMU). Kembarannya
> ESP32-C6-**Touch**-LCD-1.47 (8MB, touch AXS5106, IMU, pinmap beda semua).

### Panel & pinmap (dari wiki resmi Waveshare + diverifikasi di unit nyata)

| Fungsi | GPIO | Catatan |
|---|---|---|
| LCD SCLK | **7** | |
| LCD MOSI | **6** | |
| LCD CS | **14** | |
| LCD DC | **15** | |
| LCD RST | **21** | |
| LCD BL (backlight) | **22** | drive via transistor, aktif HIGH |
| SD MISO | 5 | belum dipakai firmware ini |
| SD CS | 4 | belum dipakai |
| SD MOSI/SCLK | share 6/7 | |
| RGB LED | 8 | WS2812, belum dipakai |
| Tombol BOOT | **9** | dipakai sebagai play/pause |

- Driver panel: **ST7789** (bukan JD9853 milik varian Touch)
- Resolusi: **172 × 320**, rotasi 0, offset kolom/baris **34** (wajib, kalau
  tidak gambarnya geser)
- Tidak ada: touch, IMU, bus I2C (GPIO18/19 kosong — normal untuk varian ini)
- USB: native USB-C (COM port muncul sebagai USB CDC; kalau hilang setelah
  flash cepat, re-plug kabel)

### Env PlatformIO: `esp32_st7789/platformio.ini` → `[env:c6_lcd_147]`

- `platform = espressif32`, `board = esp32-c6-devkitc-1`, framework Arduino
- Arduino_GFX (moononournation) sebagai driver panel
- Build terukur: **RAM 65,3% (214.036 / 327.680 B)** — 192KB di antaranya
  adalah buffer buku statis; **Flash 26,5% (347.078 / 1.310.720 B)**
- Flash: `pio run -e c6_lcd_147 -t upload --upload-port COM4`
  (set `$env:PYTHONUTF8='1'; $env:PYTHONIOENCODING='utf-8'; chcp 65001`
  dulu di PowerShell — tanpa itu progress bar Unicode bikin console hang)

### Keputusan desain untuk board ini

- Tidak ada tombol selain BOOT → play/pause = GPIO9; WPM/tema/bab dikontrol
  via serial (protokol SOH di `firmware/README.md`)
- Tanpa touch, tanpa IMU: fitur swipe dsb. tidak ada — sengaja, core cukup
- NVS (Preferences) dipakai untuk blob persist 12-byte `rsvp_state_blob`
  (wpm + posisi + tema) — **belum ditulis** (TODO di `main.cpp`)

---

## 2. ESP32 generik (DevKit + panel ST7789 eksternal)

Fallback murah: ESP32-DevKitC klasik + panel ST7789 240×320 tanpa touch.

### Pinmap default (`[env:esp32_generic]`, wiring bebas — ubah via build_flags)

| Fungsi | GPIO |
|---|---|
| SCLK | 18 (VSPI) |
| MOSI | 23 (VSPI) |
| CS | 5 |
| DC | 2 |
| RST | 4 |
| BL | 15 |
| Tombol | 0 (BOOT) |

- Resolusi default 240×320, offset 0 — sesuaikan kalau panel beda
- Heap lebih lega (~300KB bebas setelah WiFi off) → buffer buku bisa naik
- Belum pernah dibangunkan di hardware; build saja yang hijau

---

## 3. Raspberry Pi Pico 2 (RP2350)

Target portability: membuktikan core jalan di luar ESP. Pico 2 dipilih di
atas Pico (RP2040) karena RAM sama-sama 520KB tapi core 150MHz M33 lebih
cepat untuk render, dan flash 4MB sama.

### Wiring default (`pico_st7789/platformio.ini`, `[env:pico]` — dipakai juga untuk Pico 2)

| Fungsi | GPIO |
|---|---|
| SCLK | 10 (SPI1) |
| MOSI | 11 |
| CS | 9 |
| DC | 8 |
| RST | 12 |
| BL | 13 |
| Tombol | 14 |

- Panel apa pun ST7789 SPI; offset 0, 240×320 default
- **Env aktif:** `pico` (RP2040) — build ✅ terukur: RAM 78,8% (206.600 /
  262.144 B), Flash 3,1% (65.608 B) — membuktikan core + glue muat di chip
  RAM-kecil sekalipun
- **Env `pico2` (RP2350) di-comment:** PlatformIO fork `maxgerhardt` yang
  dipakai belum kenal board id `pico2`. Untuk mengaktifkan:
  1. Ganti `platform` di `platformio.ini` ke core **earlephilhower**
     resmi (mendukung RP2350), atau pasang sebagai platform terpisah
  2. Uncomment `[env:pico2]`
  3. `pio run -e pico2`
- Kode glue 100% sama dengan ESP (satu `main.cpp`, guard
  `ARDUINO_ARCH_RP2040` hanya untuk pemilihan bus SPI) — porting = build env
  baru, bukan kode baru
- Persist: EEPROM emulation arduino-pico (`EEPROM.put/get` blob 12-byte) —
  padanan NVS ESP, belum ditulis
- Pico 2 **W** (WiFi CYW43): tidak dibutuhkan — companion over serial saja

---

## Matriks kapabilitas fitur per board (fitur core 1–6)

| Fitur | C6-LCD-1.47 | ESP32 generik | Pico 2 |
|---|---|---|---|
| 1. WPM adjustment | ✅ serial | ✅ serial | ✅ serial |
| 2. Chapter navigation | ✅ serial + BOOT | ✅ | ✅ |
| 3. All language (pivot UTF-8) | ✅ matematika OK; glyph tergantung font panel | ✅ idem | ✅ idem |
| 4. Block mode real RSVP | ✅ | ✅ | ✅ |
| 5. Color theme presets | ✅ 4 preset, `T n` serial | ✅ | ✅ |
| 6. Companion kirim teks | ✅ USB serial SOH | ✅ | ✅ |
| Persist antar mati lampu | ⚠️ blob di RAM, NVS TODO | ⚠️ idem | ⚠️ idem |
| Touch UI | ❌ (board tanpa touch) | ❌ | ❌ |

Satu-satunya gap yang sama di ketiga board: penulisan blob persist ke flash
(NVS/EEPROM) belum diimplement — state aman selama device nyala, hilang
saat mati. Itu kerja berikutnya setelah flash pertama sukses.

---

## Referensi

- Protokol serial + render contract: `firmware/README.md`
- Pinmap resmi Waveshare: wiki `ESP32-C6-LCD-1.47`
- Thread diagnosis kembaran Touch/non-Touch: Tasmota #24876, HA forum
  `esp32-c6-with-1-47-touch-i-lost-track` (kasih konteks kenapa pinmap ini
  yang benar)
