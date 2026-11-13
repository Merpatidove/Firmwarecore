// Feature inventory guard: node check.js
'use strict';

const fs = require('fs');
const path = require('path');

const START = '<!-- FEATURE-INVENTORY:START -->';
const END = '<!-- FEATURE-INVENTORY:END -->';

const research = fs.readFileSync(path.join(__dirname, 'RESEARCH.md'), 'utf8');
const html = fs.readFileSync(path.join(__dirname, 'index.html'), 'utf8');

const start = research.indexOf(START);
const end = research.indexOf(END);
if (start === -1 || end === -1) {
  throw new Error(
    'check.js: FEATURE-INVENTORY markers not found in RESEARCH.md',
  );
}
const block = research.slice(start + START.length, end);

const required = [];
for (const line of block.split('\n')) {
  if (!line.startsWith('|')) continue;
  const cell = line.split('|')[1];
  if (!cell) continue;
  for (const m of cell.matchAll(/`([a-zA-Z][a-zA-Z0-9]*)`/g)) {
    if (!required.includes(m[1])) required.push(m[1]);
  }
}
required.sort();

const ids = new Set();
for (const m of html.matchAll(/id="([^"]+)"/g)) ids.add(m[1]);

const missing = required.filter((id) => !ids.has(id));

if (missing.length) {
  console.error('feature check: MISSING ids: ' + missing.join(', '));
  process.exit(1);
}
console.log('feature check: OK (' + required.length + ' ids)');
