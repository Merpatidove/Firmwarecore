#!/usr/bin/env python3
# check_rfile.py — behavioral reference model of firmware/rsvp_file.c.
# Mirrors the windowed reader math (skip map, rf_file_off, read_logical,
# rf_prime positioning, rf_attach margins) and ASSERTS against a direct
# reconstruction, using a tiny window to force constant refills.
# usage: py check_rfile.py
import sys

RF_MARGIN = 256
RF_SKIP_MAX = 160


def make_book():
    part1 = ("\x01S 0\tBAB 1\n"
             "alpha beta gamma,\ndelta epsilon zeta.\nepsilon zeta eta theta.\n"
             "\x01S 78\tBAB 2\n"
             "one two three.\nfour five six seven eight nine ten eleven.\n")
    body = "".join(f"word{i} word{i+100} tail.\n" for i in range(40))
    part2 = "\x01T 2\n\x01E\n"
    return (part1 + body + part2).encode('ascii')


def expected_stream(file_bytes):
    """Direct reconstruction: drop SOH control lines, keep chunk bytes."""
    out = bytearray()
    in_ctl = False
    for c in file_bytes:
        if in_ctl:
            if c == 0x0A:
                in_ctl = False
            continue
        if c == 0x01:
            in_ctl = True
            continue
        out.append(c)
    return bytes(out)


def scan(file_bytes):
    """Mirror rf_scan: skip map with stream_start, sections, lengths."""
    skips = []  # (file_start, len, stream_start)
    stream_len = 0
    sections = []
    end_seen = False
    theme_req = -1
    in_ctl = False
    ctl = b''
    for i, c in enumerate(file_bytes):
        if in_ctl:
            skip = True
            if c == 0x0A:
                in_ctl = False
                if ctl.startswith(b'S '):
                    parts = ctl[2:].split(b'\t', 1)
                    if len(parts) == 2:
                        sections.append((int(parts[0]),
                                         parts[1].decode('ascii')))
                elif ctl.startswith(b'E'):
                    end_seen = True
                elif ctl.startswith(b'T'):
                    theme_req = int(ctl[1:] or 0)
                ctl = b''
            elif len(ctl) < 95:
                ctl += bytes([c])
        else:
            if c == 0x01:
                in_ctl = True
                skip = True
            else:
                skip = False
                stream_len += 1
        if skip:
            if skips and skips[-1][0] + skips[-1][1] == i:
                s, l, ss = skips[-1]
                skips[-1] = (s, l + 1, ss)
            else:
                skips.append((i, 1, stream_len))
            assert len(skips) <= RF_SKIP_MAX, "skip map overflow"
    return skips, stream_len, sections, end_seen, theme_req


def rf_file_off(skips, logical):
    off = logical
    for (fs, ln, ss) in skips:
        if ss <= logical:
            off += ln
        else:
            break
    return off


def read_logical(skips, stream_len, file_bytes, logical, length):
    assert logical + length <= stream_len, "range past EOF"
    dst = bytearray()
    done = 0
    while done < length:
        cur = logical + done
        fo = rf_file_off(skips, cur)
        run = length - done
        for (fs, ln, ss) in skips:
            if ss > cur:
                d = ss - cur
                if d < run:
                    run = d
                break
        if fo + run > len(file_bytes):
            run = len(file_bytes) - fo
        assert run > 0, "zero-length run"
        dst += file_bytes[fo:fo + run]
        done += run
    return bytes(dst)


def rf_prime(skips, stream_len, file_bytes, win_cap, logical_pos):
    assert logical_pos <= stream_len
    back = min(win_cap // 4, logical_pos)
    # big-window only: keep a full margin behind, never shrink forward room
    if win_cap >= 4 * RF_MARGIN and back < RF_MARGIN and \
            logical_pos > RF_MARGIN:
        back = RF_MARGIN
    start = logical_pos - back
    avail = stream_len - start
    length = min(win_cap, avail)
    # pos at the trailing edge is only legal parked at end-of-stream
    # (off == len: empty frame, no scan). Otherwise slide the window back.
    if logical_pos < stream_len and logical_pos - start >= length and \
            start > 0:
        start = logical_pos + 1 - length
        avail = stream_len - start
        length = min(win_cap, avail)
    assert length >= 2 and start <= logical_pos
    assert logical_pos < start + length or logical_pos == stream_len, \
        "pos outside window"
    return start, read_logical(skips, stream_len, file_bytes,
                               start, length)


def main():
    file_bytes = make_book()
    skips, stream_len, sections, end_seen, theme_req = scan(file_bytes)
    exp = expected_stream(file_bytes)

    print(f'file={len(file_bytes)}B stream={stream_len}B skips={len(skips)} '
          f'sections={len(sections)} end={end_seen} theme={theme_req}')
    assert end_seen and theme_req == 2
    assert len(sections) == 2 and sections[0][1] == 'BAB 1'
    assert exp == read_logical(skips, stream_len, file_bytes, 0, stream_len), "full reconstruction mismatch"

    # tiny window (64B): forces constant refills across the whole stream
    win_cap = 64
    step = 7
    p = 0
    while p < stream_len:
        start, win = rf_prime(skips, stream_len, file_bytes, win_cap, p)
        inwin = p - start
        n = min(len(win) - inwin, 32)
        assert win[inwin:inwin + n] == exp[p:p + n], \
            f"window bytes mismatch at pos {p}"
        p += step
    print(f'window walk OK (win={win_cap}B, pos step={step})')

    # section offsets resolve past window edges: prime at each section
    for off, title in sections:
        start, win = rf_prime(skips, stream_len, file_bytes, win_cap, off)
        inwin = off - start
        assert win[inwin:inwin + 6] == exp[off:off + 6], \
            f"section {title!r} offset {off} not resolvable"
    print('section seek OK')

    # attach margin rule: every modeled core access stays inside window
    class Rsvp:  # minimal mirror of the fields rf_attach touches
        def __init__(self):
            self.stream = None
            self.len = 0
            self.off = 0

    r = Rsvp()
    pos = 0
    win_start, win_len = -1, 0
    for i in range(500):
        # margin rule mirrored from rsvp_file.c rf_attach. Effective
        # margin = min(RF_MARGIN, win_cap/4): the core never scans further
        # than one line (<=63B) each side, so a small window just shrinks
        # the margin. rf_prime backs up win/4 so front margin is free.
        eff = min(RF_MARGIN, win_cap // 4)
        front_ok = pos >= win_start and pos - win_start >= eff
        if not (front_ok and pos + eff <= win_start + win_len):
            win_start, win = rf_prime(skips, stream_len, file_bytes,
                                      win_cap, pos)
            win_len = len(win)
        r.off = pos - win_start
        # off == win_len only legal parked at end of stream
        assert 0 <= r.off <= win_len, \
            f'pos={pos} win_start={win_start} win_len={win_len} i={i}'
        assert r.off < win_len or pos == stream_len
        pos += 6  # one word (~6.5B avg) per step
        pos = min(pos, stream_len)
    assert pos == stream_len
    print(f'attach/sync walk OK (reached pos={pos}/{stream_len})')

    print('ALL RFILE CHECKS PASSED')
    return 0


if __name__ == '__main__':
    sys.exit(main())
