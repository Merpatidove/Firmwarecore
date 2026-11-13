#!/usr/bin/env python3
# send_book.py — push a .txt book to the RSVP reader over serial (SOH protocol).
# Wire format (firmware): plain lines = chunks; SOH (0x01) control lines:
#   0x01 S <byte_offset>\t<title>   section row (repeatable, BEFORE the text)
#   0x01 E                           end of transmission -> start playing
#   0x01 T <idx>                     theme preset (0 night, 1 paper, 2 ocean, 3 forest)
# usage: py send_book.py --port COM4 --file book.txt [--theme 0] [--dry-run]
import argparse
import re
import sys
import time

MAX_WORDS = 8
BREAK = re.compile(r'[.;:!?…,]$')
HEADING = re.compile(
    r'^(BAB\s+\S+.*|DAFTAR.*|KATA PENGANTAR.*|PKM-KI.*|ABSTRAK.*|'
    r'Pendahuluan|Tujuan|Ruang Lingkup|Konsep Pelaksanaan Program|'
    r'Luaran|Manfaat|Penutup|Simpulan|Daftar Pustaka|Lampiran.*|'
    r'Bab\s+\S+.*)$',
    re.IGNORECASE)


def clean_line(l):
    """Drop PDF-extraction garbage: font-subset 'id' prefixes, repeated
    'id' tokens, page numbers, dot-leader TOC lines, binary junk."""
    l = l.strip()
    if not l or '\ufffd' in l:
        return ''
    l = l.encode('ascii', 'ignore').decode()
    l = re.sub(r'\bid(?=[A-Z])', '', l)          # idDAFTAR -> DAFTAR
    l = re.sub(r'(?:\bid\s+)+', '', l).strip()   # "id id id" page headers
    l = re.sub(r'^\d{1,3}\s+', '', l)            # leading page number
    if not l or re.search(r'\.{6,}', l):         # TOC dot leaders
        return ''
    if any(ord(c) < 32 for c in l):              # font-subset binary junk
        return ''
    if re.search(r'[\[\]{}<>~^|@#$_=+*%\\]', l):  # symbol soup from subsets
        return ''
    good = sum(1 for c in l if c.isalpha() or c == ' ')
    if good < 0.6 * len(l) or len(l) < 2:
        return ''
    return l


def is_heading(l):
    """Generic heading: short, few Title/UPPER words, no ending punctuation."""
    if not 2 < len(l) < 48 or l.endswith(('.', ',', ';', ':', '!', '?')):
        return False
    words = l.split()
    if len(words) > 7:
        return False
    caps = sum(1 for w in words if w[:1].isupper() or w[:1].isdigit())
    return caps >= max(1, len(words) - 1)


def chunk(text):
    """Same rule as webapp chunker: <=8 words, break early at phrase punct."""
    out, cur = [], []
    for w in text.split():
        cur.append(w)
        if BREAK.search(w) or len(cur) >= MAX_WORDS:
            out.append(' '.join(cur))
            cur = []
    if cur:
        out.append(' '.join(cur))
    return out


def build(a, lines):
    sections = []  # (title, text)
    cur_title, cur = '', []
    for l in lines:
        if is_heading(l):
            if cur:
                sections.append((cur_title, '\n'.join(cur)))
                cur = []
            cur_title = l
        else:
            cur.append(l)
    if cur:
        sections.append((cur_title, '\n'.join(cur)))
    if sections and not sections[0][0]:
        sections[0] = ('awal', sections[0][1])

    buf = bytearray()
    rows = []
    for title, text in sections:
        piece = ('\n'.join(chunk(text)) + '\n').encode('ascii')
        if len(buf) + len(piece) > a.limit:
            print(f'stop: buffer penuh sebelum "{title[:30]}"')
            break
        if len(rows) < a.max_sections:
            rows.append((len(buf), title.replace('\t', ' ')[:48]))
        buf += piece
    return rows, buf


def send_book(a, rows, buf):
    try:
        import serial
    except ImportError:
        print('butuh pyserial: pakai python PlatformIO')
        sys.exit(1)
    ser = serial.Serial()
    ser.port = a.port
    ser.baudrate = a.baud
    ser.timeout = 2
    ser.write_timeout = 5
    ser.dtr = False   # preset LOW: avoids the USB-serial-jtag reset wiggle
    ser.rts = False
    ser.open()
    print(f'port {a.port} open')
    time.sleep(0.5)
    ser.dtr = True    # ESP32 USB CDC gates data on DTR
    time.sleep(1.0)

    acc = b''
    deadline = time.time() + 8
    while time.time() < deadline:
        n = ser.in_waiting
        if n:
            acc += ser.read(n)
        if b'[rsvp] ready' in acc:
            break
        time.sleep(0.2)
    print('ready line:', b'[rsvp] ready' in acc)

    try:
        if a.theme is not None:
            ser.write(f'\x01T {a.theme}\n'.encode('ascii'))
            ser.flush()
            time.sleep(0.2)
        for off, title in rows:
            ser.write(f'\x01S {off}\t{title}\n'.encode('ascii'))
            ser.flush()
            time.sleep(0.01)
        i, n = 0, len(buf)
        while i < n:
            ser.write(bytes(buf[i:i + 256]))
            i += 256
            time.sleep(0.005)
        ser.flush()
        time.sleep(0.3)
        ser.write(b'\x01E\n')
        ser.flush()
    except serial.SerialTimeoutException:
        print('WRITE TIMEOUT — cabut-pasang kabel USB board, lalu ulangi.')
        ser.close()
        sys.exit(2)

    out = b''
    deadline = time.time() + 5
    while time.time() < deadline:
        n = ser.in_waiting
        if n:
            out += ser.read(n)
        if b'[rsvp] len=' in out:
            break
        time.sleep(0.2)
    print('device:', out[-200:])
    ser.close()
    print('DONE')


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--port', default='COM4')
    ap.add_argument('--file', required=True)
    ap.add_argument('--baud', type=int, default=115200)
    ap.add_argument('--theme', type=int, default=None)
    ap.add_argument('--limit', type=int, default=190 * 1024,
                    help='max bytes (C6 book buffer = 192KB)')
    ap.add_argument('--max-sections', type=int, default=120)
    ap.add_argument('--dry-run', action='store_true')
    a = ap.parse_args()

    raw = open(a.file, encoding='utf-8', errors='replace').read()
    lines = [c for c in (clean_line(l) for l in raw.splitlines()) if c]
    rows, buf = build(a, lines)
    print(f'sections={len(rows)} bytes={len(buf)} limit={a.limit}')
    if a.dry_run:
        for off, t in rows[:12]:
            print(f'  {off:7d}  {t}')
        if len(rows) > 12:
            print('  ...')
        return
    send_book(a, rows, buf)


if __name__ == '__main__':
    main()
