#!/usr/bin/env python3
# check_rsvp.py — reference-model checker for .rsvp files. Mirrors
# firmware/rsvp_stream.c rs_feed() semantics line-for-line, so a .rsvp
# produced by send_book.py --out can be validated without a C host compiler.
# usage: py check_rsvp.py <file.rsvp> [--expect-sections N] [--max-buf N]
import argparse
import sys

CTL_MAX = 96
TITLE_MAX = 48


def check(path, expect_sections=None, max_buf=192 * 1024):
    data = open(path, 'rb').read()
    buf = bytearray()
    truncated = False
    sections = []
    titles = []
    end_seen = False
    in_ctl = False
    ctl = b''
    theme_req = -1

    for c in data:
        b = bytes([c])
        if end_seen:
            continue
        if in_ctl:
            if c == 0x0A:  # '\n'
                in_ctl = False
                if ctl.startswith(b'S ') and len(sections) < 128:
                    parts = ctl[2:].split(b'\t', 1)
                    if len(parts) == 2:
                        off = int(parts[0])
                        title = parts[1][:TITLE_MAX - 1]
                        sections.append(off)
                        titles.append(title)
                elif ctl.startswith(b'E'):
                    end_seen = True
                elif ctl.startswith(b'T'):
                    theme_req = int(ctl[1:] or 0)
                ctl = b''
            elif len(ctl) < CTL_MAX - 1:
                ctl += b
            continue
        if c == 0x01:
            in_ctl = True
            ctl = b''
            continue
        if len(buf) + 1 < max_buf:
            buf += b
        else:
            truncated = True

    print(f'file         : {path}')
    print(f'size         : {len(data)} bytes')
    print(f'buf_len      : {len(buf)}  (cap {max_buf})')
    print(f'truncated    : {truncated}')
    print(f'sections     : {len(sections)}')
    print(f'end_seen     : {end_seen}')
    print(f'theme_req    : {theme_req}')
    if sections:
        print(f'first title  : {titles[0].decode("ascii", "replace")!r}')
        print(f'last  title  : {titles[-1].decode("ascii", "replace")!r}')

    ok = True
    if not end_seen:
        print('FAIL: no 0x01 E terminator — device would never start playback')
        ok = False
    # monotonic offsets: core seek requires ascending section table
    for i in range(1, len(sections)):
        if sections[i] <= sections[i - 1]:
            print(f'FAIL: section {i} offset {sections[i]} not > '
                  f'{sections[i - 1]}')
            ok = False
    for off in sections:
        if off >= len(buf):
            print(f'FAIL: section offset {off} beyond buf {len(buf)}')
            ok = False
    if expect_sections is not None and len(sections) != expect_sections:
        print(f'FAIL: expected {expect_sections} sections, got {len(sections)}')
        ok = False
    print('PASS' if ok else 'CHECK FAILED')
    return 0 if ok else 1


if __name__ == '__main__':
    ap = argparse.ArgumentParser()
    ap.add_argument('file')
    ap.add_argument('--expect-sections', type=int, default=None)
    ap.add_argument('--max-buf', type=int, default=192 * 1024)
    a = ap.parse_args()
    sys.exit(check(a.file, a.expect_sections, a.max_buf))
