// firmware/fuzz_check.js — transliterates firmware/rsvp_core.c logic into JS
// and fuzzes it against the real rsvp-core.js (pivot + timing). Algorithm-level
// validation; it does NOT compile the C. BMP-only fuzz text: JS split() counts
// UTF-16 units, the C counts codepoints — identical for BMP, so astral chars
// excluded. Run: node firmware/fuzz_check.js
'use strict';
const R = require('../rsvp-core.js');
const assert = require('assert');
// NOTE: reaches up into the webapp tree to import the JS engine. This is the
// ONLY coupling between firmware/ and the webapp — a test-only dependency.

// ---- transliteration of device/rsvp_core.c ----
const isLead = (b) => (b & 0xC0) !== 0x80;

function cpCount(buf, s, e) {
  let n = 0;
  for (let i = s; i < e; i++) if (isLead(buf[i])) n++;
  return n;
}

function cpOffset(buf, s, e, idx) {
  let seen = 0;
  for (let i = s; i < e; i++) {
    if (isLead(buf[i])) {
      if (seen === idx) return i;
      seen++;
    }
  }
  return e;
}

function wordCount(buf, s, e) {
  let n = 0, inWord = false;
  for (let i = s; i < e; i++) {
    if (buf[i] === 0x20) inWord = false;
    else if (!inWord) { n++; inWord = true; }
  }
  return n;
}

// hold_ms: integer math exactly as in rsvp_core.c (single end-floor form)
function holdMs(buf, s, e, wpm, words) {
  let end = e;
  let mult = 10;
  if (end === s) return 0;
  let c = buf[end - 1];
  if (c === 0x22 || c === 0x27 || c === 0x29 || c === 0x5d) {
    end--;
    if (s === end) return 0;
    c = buf[end - 1];
  }
  if (end - s >= 3 && buf[end - 3] === 0xe2 && buf[end - 2] === 0x80 && buf[end - 1] === 0xa6)
    mult = 18;
  else if (c === 0x2e || c === 0x21 || c === 0x3f) mult = 18;
  else if (c === 0x2c || c === 0x3b || c === 0x3a) mult = 13;
  return Math.floor((60000 * words * mult) / (wpm * 10));
}

// rsvp_frame() pivot portion
function cFrame(buf, s, e, wpm) {
  const words = wordCount(buf, s, e);
  const total = cpCount(buf, s, e);
  if (total === 0) return { words, ms: holdMs(buf, s, e, wpm, words), pcp: -1, pb: -1 };
  let pcp = total <= 1 ? 0 : total <= 5 ? 1 : total <= 9 ? 2 : total <= 13 ? 3 : 4;
  if (pcp > total - 1) pcp = total - 1;
  while (pcp < total - 1 && buf[cpOffset(buf, s, e, pcp)] === 0x20) pcp++;
  return {
    words,
    ms: holdMs(buf, s, e, wpm, words),
    pcp,
    pb: cpOffset(buf, s, e, pcp) - s,
  };
}

// ---- fuzz driver ----
const MB = ['é', 'ü', 'ñ', '漢', 'こ', 'ж', 'ب', 'λ', '中', '字'];
const ASCII = ['a', 'b', 'c', 'x', 'y', 'z', 'q'];
const ENDERS = ['', '.', '!', '?', ',', ';', ':', '…', '."', ".'", ").", ",'", '…)', ",'"];

let seed = 0x12345678;
function rnd() {
  seed ^= seed << 13; seed ^= seed >>> 17; seed ^= seed << 5;
  return (seed >>> 0) / 4294967296;
}
function pick(arr) { return arr[Math.floor(rnd() * arr.length)]; }

function makeChunk() {
  const words = 1 + Math.floor(rnd() * 4);
  const parts = [];
  for (let w = 0; w < words; w++) {
    const len = 1 + Math.floor(rnd() * 14);
    let t = '';
    for (let i = 0; i < len; i++) t += rnd() < 0.35 ? pick(MB) : pick(ASCII);
    parts.push(t);
  }
  return parts.join(' ') + pick(ENDERS);
}

let checked = 0;
for (let iter = 0; iter < 3000; iter++) {
  const chunk = makeChunk();
  const wpm = 60 + Math.floor(rnd() * 540);
  const js = R.split(chunk);
  const jsWords = chunk.split(' ').length;
  const jsMs = R.durationMs(chunk, wpm);
  const buf = Buffer.from(chunk, 'utf8');
  const c = cFrame(buf, 0, buf.length, wpm);

  // JS split() returns the pivot CHARACTER (s.charAt); its index is left.length
  // (UTF-16 units == codepoints for BMP fuzz text).
  const jsPivotIdx = js.left.length;
  const jsCpTotal = [...chunk].length;
  const byteOff = Buffer.byteLength(chunk.slice(0, jsPivotIdx), 'utf8');

  assert.strictEqual(c.pcp, jsPivotIdx, `pivot_cp iter=${iter} chunk=${JSON.stringify(chunk)} c=${c.pcp} js=${jsPivotIdx}`);
  assert.strictEqual(c.pb, byteOff, `pivot_byte iter=${iter} chunk=${JSON.stringify(chunk)}`);
  assert.strictEqual(c.words, jsWords, `words iter=${iter}`);
  // integer math with delayed floor vs JS float: within 1ms
  assert.ok(Math.abs(c.ms - jsMs) <= 1, `ms iter=${iter} chunk=${JSON.stringify(chunk)} wpm=${wpm} c=${c.ms} js=${jsMs}`);
  assert.ok(c.pcp < jsCpTotal);
  assert.notStrictEqual(buf[c.pb], 0x20, `pivot on space iter=${iter} chunk=${JSON.stringify(chunk)}`);
  checked++;
}

console.log(`fuzz_check: OK (${checked} chunks, firmware core matches rsvp-core.js)`);
