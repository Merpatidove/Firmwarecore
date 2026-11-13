# firmware/ — RSVP playback core for MCUs

**This is NOT the webapp.** The webapp lives at the repo root (`index.html`,
`rsvp-core.js`). This directory is a standalone C99 firmware core for ESP32 /
RP2040 / any MCU board. It shares **no code, no build, no tests** with the
webapp — the only link is the wire format below and the research rules it
ports (rule numbers cite `../RESEARCH.md`).

C99 port of `rsvp-core.js`'s playback math (block mode only). No heap, no OS: a streaming cursor
over a buffer the companion app pushes over serial/BLE/WiFi.

## Wire format (companion → board)

- **Chunk stream:** UTF-8 text, chunks separated by a single `'\n'` byte. Chunks
  are the JS `chunk()` output: words joined by single spaces, so `0x0A` never
  appears inside a chunk or a UTF-8 multibyte sequence. The board never chunks,
  sanitizes, or extracts sections (rules #4/#5/#14 — the companion owns them).
- **Section table:** `rsvp_section[]` (`byte_offset` into the stream + title
  string), precomputed by the companion's `sections()` output (rule #15).
  Delivery is the board bring-up's choice; the core only consumes the table.

## Core contract

`rsvp_next/prev` scan to the next/previous `'\n'` — O(bytes of one chunk) per
step, no chunk index table. State: buffer pointer, length, byte offset, wpm,
section table pointer. `rsvp_frame()` returns the current chunk as an
`rsvp_view`: text, byte length, ORP pivot (byte offset + codepoint index),
word count, and hold time in ms (rule #7: `60000×words/wpm`, ×18/10 on
sentence end, ×13/10 on clause end; integer math stays within ~1ms of the
JS float values). Empty stream → `text=NULL, len=0`.

## Themes (feature 5)

`RSVP_THEMES[]` holds `RSVP_THEME_PRESETS` named presets (night / paper /
ocean / forest), each with bg/fg/accent/guide `rsvp_color`s in both RGB888
and RGB565. Boards pick an index and paint from it; no theme storage in the
core. The `night` preset equals the `RSVP_*_DEFAULT` colors.

## Persistence (board-owned storage)

`rsvp_save(r, theme_idx, &blob)` fills a 12-byte `rsvp_state_blob` (magic
"RSP1", version, wpm, chunk-aligned byte offset, theme index);
`rsvp_load(r, &blob, &theme_idx)` applies it and returns `false` on
magic/version mismatch (board falls back to defaults). The core never
touches flash — the board puts the blob in NVS (ESP32) / EEPROM emulation
(Pico) / a sidecar file (SD).

## Build flags for the self-check

`test_core.c` needs two defines when compiled with the freestanding
cross-toolchain: `-DRSVP_EXPOSE_STATE` (unhide `struct rsvp_s` so the test
can stack-allocate it) and `-DRSVP_TEST` (assert → no-op, drops libc).

## Render contract (board implements ONE function)

```c
void draw(const rsvp_view *v); /* draw on timer tick while playing */
```

- Draw `v->text` in a **monospace** font, fg color (rule #11); then draw the
  pivot character — or the whole current word — in the accent color. `pivot_byte`
  and `pivot_cp` tell you where (rule #6).
- Pin the pivot like the webapp pins with `ch` units:
  `left_pad_cp = pivot_cp`, `right_pad_cp = total_cp − pivot_cp − 1`. The core
  never pivots on a space, so the pinned glyph is always a real character.
- Guide line / centering: optional per board.
- Never pivot on a space is guaranteed by the core; never stack words
  vertically (rule #9) is the board's job — block mode, one line.

## Button map (board wiring → core calls)

| Button | Call |
|---|---|
| play/pause | board-side flag around a timer calling `draw(rsvp_frame(r))` |
| wpm up/down | `rsvp_set_wpm(r, wpm ± 25)` (feature 1; clamps ≥60, 0 → 300) |
| prev / next | `rsvp_prev(r)` / `rsvp_next(r)` (rule #8: regression always available) |
| section jump | `rsvp_seek_chunk(r, rsvp_section_at(r, i, &title))` (feature 2) |
| restart | `rsvp_seek_chunk(r, 0)` |

## Port-rule map

| Rule | Where it lives |
|---|---|
| #5 chunking (phrase boundaries) | companion JS `chunk()` — not ported; core consumes `'\n'` chunks |
| #6 ORP pivot, never on space | `rsvp_frame()` pivot fields (`pivot_byte`/`pivot_cp`) |
| #7 sentence/clause pause | `durationMs` port in `hold_ms()` (×18/10, ×13/10) |
| #8 regression | `rsvp_prev()` |
| #10 ORP table, one-place tuning | `ORP_TABLE` in `rsvp_core.c` — the only knob |
| #11 monospace | board render contract above (core supplies pivot, board supplies mono font) |
| #15 sections | companion JS `sections()` → `rsvp_section` table → `seek_chunk` |

## Board glue (`platforms/`)

Two ready-to-flash PlatformIO projects, both driving ST7789 SPI panels via
Arduino_GFX and ingesting the wire format over USB serial:

| | `esp32_st7789` | `pico_st7789` |
|---|---|---|
| Default env | `c6_lcd_147` — Waveshare ESP32-C6-LCD-1.47 **non-touch** (SCK=7 MOSI=6 CS=14 DC=15 RST=21 BL=22, 172×320, offset 34) | `pico` (RP2040) — free wiring, default SCK=10 MOSI=11 CS=9 DC=8 RST=12 BL=13 BTN=14 |
| Other envs | `esp32_generic` (classic ESP32 + panel) | `pico_w` (same build; WiFi needs earlephilhower core) |
| Control | BOOT button (GPIO9) = play/pause; serial `T <n>` = theme | same |
| Build | `pio run -e c6_lcd_147` ✅ | `pio run -e pico` ✅ |

Measured footprints: C6 = 214KB RAM / 347KB flash; RP2040 = 207KB RAM /
66KB flash (192KB of RAM is the book buffer, statically reserved).

**Serial ingest protocol** (companion → board): plain UTF-8 lines are chunk
bytes pushed into the book buffer verbatim; control lines start with SOH
(`0x01`):

- `0x01 S <byte_offset>\t<title>` — one section-table row
- `0x01 T <n>` — switch to theme preset n
- `0x01 E` — end of transmission → `rsvp_init` + resume + play

Persistence: on every chunk the board refreshes a 12-byte
`rsvp_state_blob` (`rsvp_save`); writing it to NVS/EEPROM is a board TODO
(debounce ~5s / on pause).

## Build & self-check (host)

```
cc -std=c99 -Wall -Wextra -o test_core test_core.c rsvp_core.c && ./test_core
```

Prints `firmware core self-check: OK`.

**Note:** no host C toolchain on the dev machine — the asserts are verified
two ways: `node firmware/fuzz_check.js` (3000 chunks against the real JS
engine) and a compile check with the riscv32 cross-gcc:

```
riscv32-esp-elf-gcc -std=c99 -Wall -Wextra -Wpedantic --freestanding -Os \
  -DRSVP_TEST -DRSVP_EXPOSE_STATE -c test_core.c rsvp_core.c
```

## Relationship to the webapp (the one-line contract)

The webapp **produces** (`chunk()` output + `sections()` table over a transport
you choose); this core **consumes**. That handoff is the entire interface.
`fuzz_check.js` is the only file that reaches into webapp code, and only to
test — nothing in the webapp imports or links anything from `firmware/`.

## Deliberately stripped

- No ticker (rules #12–#13 void on device): block mode only.
- No `sanitize()` (rule #14) — companion owns it; raw bytes pass through.
- No `chunk()` (rule #5) — companion owns it.
- No `sections()` extraction (rule #15) — companion sends the table.
- No board glue (timers, serial, drivers, `#ifdef ARDUINO`) — bring-up's job.
