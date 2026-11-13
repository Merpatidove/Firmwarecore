// rsvp-core.js — pure RSVP logic. Loads as a classic browser script (window.RSVP)
// and as a Node module (require) so it has exactly one runnable self-check.
(function (root, factory) {
  var api = factory();
  if (typeof module !== 'undefined' && module.exports) module.exports = api;
  else root.RSVP = api;
})(typeof globalThis !== 'undefined' ? globalThis : this, function () {
  'use strict';

  // Optimal Recognition Point: where the eye should land.
  // Rule #10: this is a heuristic, not a proven optimum (Spritz never disclosed
  // its table), so it stays editable — tune ORP_TABLE and nothing else.
  // Orp index per word length: n<=1, n<=5, n<=9, n<=13, anything longer.
  var ORP_TABLE = [0, 1, 2, 3, 4];
  function pivotIndex(s) {
    var n = s.length;
    return n <= 1 ? ORP_TABLE[0]
         : n <= 5 ? ORP_TABLE[1]
         : n <= 9 ? ORP_TABLE[2]
         : n <= 13 ? ORP_TABLE[3]
         : ORP_TABLE[4];
  }

  // Normalise messy extracted text: control chars, PDF dot-leaders, decorative
  // rules, hyphenated line breaks, runaway whitespace. Pure — see self-check.
  function sanitize(text) {
    return String(text)
      .replace(/\r\n?/g, '\n')
      .replace(/[\u0000-\u0008\u000B\u000C\u000E-\u001F\u007F]/g, ' ')
      .replace(/(\w)-\s*\n\s*(\w)/g, '$1$2')             // de-hyphenate line breaks
      .replace(/\.{3,}/g, ' \u2026 ')                    // dot leaders / long ellipsis -> …
      .replace(/^\s*[.\-=_*~·•\u2022]{2,}\s*$/gm, '')    // decorative rule lines
      .replace(/([^\w\s])\1{2,}/g, '$1')                 // collapse repeated punctuation
      .replace(/[ \t]{2,}/g, ' ')
      .replace(/ *\n */g, '\n')
      .replace(/\n{3,}/g, '\n\n')
      .trim();
  }

  // Rule #5: end a chunk on a phrase/clause boundary when one is within reach,
  // instead of always cutting on a fixed word count. The boundary is searched
  // on either side of the nominal cut (still ±1 word, never past
  // `size + SLACK`) so rule #4 (≈2-3 words) still holds.
  var SLACK = 1;
  function endsClause(word) {
    return /[.!?\u2026,;:\u2013\u2014]["')\]]?$/.test(word);
  }

  // Split text into chunks of `size` words. Whitespace-only -> [].
  function chunk(text, size) {
    size = Math.max(1, Math.floor(size) || 1);
    var w = String(text).split(/\s+/).filter(Boolean);
    var out = [], i = 0;
    while (i < w.length) {
      var base = i + size;
      if (base >= w.length) { out.push(w.slice(i).join(' ')); break; }
      var end = base;                       // fixed size if nothing aligns
      var hit = 0;
      for (var d = 0; d <= SLACK && !hit; d++) {
        var fwd = base + d, bwd = base - d;
        if (fwd <= w.length && endsClause(w[fwd - 1])) hit = fwd;
        else if (d > 0 && bwd > i && endsClause(w[bwd - 1])) hit = bwd;
      }
      if (hit) end = hit;
      if (end > w.length) end = w.length;
      out.push(w.slice(i, end).join(' '));
      i = end;
    }
    return out;
  }

  // Split a chunk into left / pivot / right so the renderer can pin the pivot
  // to screen center (left-right padding = |L-R| ch) and the eye never moves.
  function split(s) {
    s = String(s);
    var p = Math.min(pivotIndex(s), Math.max(0, s.length - 1));
    while (p < s.length - 1 && /\s/.test(s.charAt(p))) p++; // never pivot on a space
    return { left: s.slice(0, p), pivot: s.charAt(p), right: s.slice(p + 1) };
  }

  // ms a chunk stays on screen. Longer chunks scale linearly; punctuation
  // adds a comprehension pause. wpm is the single calibration knob.
  function durationMs(s, wpm) {
    var w = s ? s.split(' ').length : 1;
    var ms = (60000 / Math.max(1, wpm)) * w;
    if (/[.!?\u2026]["')\]]?$/.test(s)) ms *= 1.8;
    else if (/[,;:]["')\]]?$/.test(s)) ms *= 1.3;
    return ms;
  }

  // Rule #15: section boundaries are a heuristic over *sanitized* text. Formats
  // that declare structure emit a "\u00A7" marker line (see index.html extractors);
  // everything else falls back to heading-like lines. Returns the text with markers
  // removed (titles stay readable) plus WORD offsets, which survive a chunk-size
  // change in a way chunk indices would not.
  var HEAD_RE = /^(chapter|part|book|volume|section|prologue|epilogue|appendix|introduction|conclusion|preface|foreword|afterword|interlude|coda|acknowledg(e)?ments)\b/i;
  function tidyLine(s) { return String(s).replace(/\s+/g, ' ').trim(); }
  function wordCount(s) {
    var w = String(s).split(/\s+/), n = 0;
    for (var i = 0; i < w.length; i++) if (w[i]) n++;
    return n;
  }
  function looksHeading(s) {
    var t = tidyLine(s);
    if (!t || t.length > 90) return false;
    if (HEAD_RE.test(t)) return true;
    if (wordCount(t) <= 8 && /^\d{1,3}(?:\.\d{1,3}){0,3}[.)]?\s+\S/.test(t)) return true;
    var letters = t.replace(/[^A-Za-z]/g, '');
    return letters.length >= 4 && letters === letters.toUpperCase() &&
           wordCount(t) <= 6 && !/[.,;:!?]$/.test(t);
  }
  function sections(text) {
    var lines = String(text).split('\n'), out = [], marks = [], seen = {}, words = 0;
    for (var i = 0; i < lines.length; i++) {
      var line = lines[i], m = /^\s*\u00A7\s*(.*)$/.exec(line), title = null;
      if (m) { line = tidyLine(m[1]); title = line; }        // sentinel: title optional
      else if (looksHeading(line)) title = tidyLine(line);
      if (title !== null) {
        var prev = marks[marks.length - 1];
        if (title && seen[title]) {
          // a running head that already named a section (repeats per page)
        } else if (title && prev && !prev.t && prev.w === words) {
          prev.t = title; seen[title] = 1;     // page break + real heading: keep the heading
        } else if (!(prev && prev.w === words)) {
          if (title) seen[title] = 1;
          marks.push({ t: title, w: words });
        }
      }
      out.push(line);
      words += wordCount(line);
    }
    // A real chapter list wins outright: sentinel/page marks are the fallback, so a
    // PDF with headings never shows both kinds at once (a mixed list reads as broken).
    var titled = [];
    for (var k = 0; k < marks.length; k++) if (marks[k].t) titled.push(marks[k]);
    if (titled.length >= 2) marks = titled;
    for (var k = 0, n = 0; k < marks.length; k++) if (!marks[k].t) marks[k].t = 'Section ' + (++n);
    return { text: out.join('\n'), marks: marks };
  }

  return { pivotIndex: pivotIndex, sanitize: sanitize, chunk: chunk, split: split, durationMs: durationMs, sections: sections };
});

// Self-check: `node rsvp-core.js`
if (typeof require !== 'undefined' && typeof module !== 'undefined' && require.main === module) {
  var assert = require('assert');
  var R = require('./rsvp-core.js');
  assert.deepStrictEqual(R.chunk('a b c d e', 2), ['a b', 'c d', 'e']);
  assert.deepStrictEqual(R.chunk('  ', 1), []);
  assert.deepStrictEqual(R.chunk('The cat sat, and the dog ran.', 3), ['The cat sat,', 'and the dog ran.']);
  assert.deepStrictEqual(R.chunk('alpha beta gamma,', 2), ['alpha beta gamma,']);
  assert.strictEqual(R.pivotIndex('a'), 0);
  assert.strictEqual(R.pivotIndex('hello'), 1);
  assert.strictEqual(R.pivotIndex('abcdefghij'), 3);
  assert.strictEqual(R.split('hello').pivot, 'e');
  assert.strictEqual(R.split('a b c').pivot, 'b');
  assert.strictEqual(R.split('').pivot, '');
  assert.ok(R.durationMs('word.', 300) > R.durationMs('word', 300), 'sentence pause');
  assert.ok(R.durationMs('word', 600) < R.durationMs('word', 300), 'faster wpm = shorter');
  assert.ok(R.durationMs('two words', 300) > R.durationMs('one', 300), 'longer chunk = longer');
  assert.strictEqual(R.sanitize('foo...........bar'), 'foo \u2026 bar');
  assert.strictEqual(R.sanitize('1.1 Introduction...........5'), '1.1 Introduction \u2026 5');
  assert.strictEqual(R.sanitize('exam-\nple'), 'example');
  assert.strictEqual(R.sanitize('=====\nhi'), 'hi');
  assert.strictEqual(R.sanitize('a\u0000b'), 'a b');
  assert.strictEqual(R.sanitize('x!!!y'), 'x!y');
  // sections: sentinel marks, heading heuristic, dedupe, word offsets
  var S1 = R.sections('Chapter One\nIt was a bright cold day.\nChapter Two\nMore.');
  assert.deepStrictEqual(S1.marks, [{ t: 'Chapter One', w: 0 }, { t: 'Chapter Two', w: 8 }]);
  assert.strictEqual(S1.text, 'Chapter One\nIt was a bright cold day.\nChapter Two\nMore.');
  var S2 = R.sections('intro\n\u00A7 Page A\nbody words\n\u00A7\nmore');
  assert.deepStrictEqual(S2.marks, [{ t: 'Page A', w: 1 }, { t: 'Section 1', w: 5 }]);
  assert.strictEqual(S2.text, 'intro\nPage A\nbody words\n\nmore');
  assert.strictEqual(R.sections('CHAPTER I\na b\nCHAPTER I\nc d').marks.length, 1, 'adjacent running head deduped');
  assert.deepStrictEqual(R.sections('2.3 Methods\nbody').marks, [{ t: '2.3 Methods', w: 0 }]);
  assert.strictEqual(R.sections('plain prose only. another line here.').marks.length, 0, 'prose is not a heading');
  assert.strictEqual(R.sections('THE BOY WHO LIVED\nx').marks[0].t, 'THE BOY WHO LIVED');
  // a PDF with one repeated running head falls back to page marks, no duplicates
  var S3 = R.sections('\u00A7\nCHAPTER ONE\npage 1 body\n\u00A7\nCHAPTER ONE\npage 2 body\n\u00A7\nno head here');
  assert.deepStrictEqual(S3.marks, [
    { t: 'CHAPTER ONE', w: 0 }, { t: 'Section 1', w: 5 }, { t: 'Section 2', w: 10 }
  ]);
  // a PDF with a real chapter list shows chapters only, never the page marks
  var S4 = R.sections('\u00A7\nCHAPTER ONE\ntext\n\u00A7\nCHAPTER ONE\ntext\n\u00A7\nCHAPTER TWO\ntext');
  assert.deepStrictEqual(S4.marks, [{ t: 'CHAPTER ONE', w: 0 }, { t: 'CHAPTER TWO', w: 6 }]);
  console.log('rsvp-core self-check: OK');
}
