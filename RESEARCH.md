# Research base & ground rules

The reader's behaviour is not a matter of taste — it's constrained by how human
reading actually works. This file collects the studies behind each rule, so the
next change can cite evidence instead of guessing.

Existing anchor: the pivot table in `rsvp-core.js` (`pivotIndex`) already follows
the Optimal Recognition Point (ORP) / optimal viewing position literature.

## What the evidence says

### Reading-rate norms
- **Brysbaert (2019)**, meta-analysis of 190 studies (18,573 readers):
  silent reading ≈ **238 wpm** (non-fiction), 260 wpm (fiction); oral ≈ 183 wpm.
  Most adults fall in **175–300 wpm** for non-fiction. The commonly-cited "300 wpm"
  is ~20% *above* the measured mean.
- **Rayner et al. (2016)**: there is a real speed/accuracy trade-off. Doubling or
  tripling reading speed without comprehension loss is not supported.

### ORP / optimal viewing position
- **O'Regan, Lévy-Schoen, Pynte & Brugaillère (1984)**; **O'Regan & Jacobs (1992)**;
  **Brysbaert & Nazir (2005)**: word-recognition is best when the eye fixates
  **left of centre, around the 3rd–5th letter**; each letter of deviation costs
  **20–30 ms**. This is the ORP table in `pivotIndex`.
- Spritz-style ORP pinning is the commercial application, but **the ORP mapping is
  not publicly validated for RSVP** (Spritz does not disclose it; see the
  *Journal of Second Language Teaching and Research* review). Treat ORP as a
  best-effort heuristic, not a proven optimum.

### RSVP vs. normal reading — comprehension
- **Potter, Kroll & Harris (1980)**; **Juola, Ward & McNamara (1982)**: RSVP
  comprehension ≈ normal reading *when total time is equal*; speeding the stream up
  drops performance ~50%.
- **Juola, Di Nocera & Ricciardi (2018)**, 209 participants: no comprehension
  difference at **250/300/350 wpm**, significant decline at **400 and 450 wpm**.
- **Ricciardi & Di Nocera (2017)** and *IJHFE (2020)*: comprehension loss appears
  **above ~250 wpm**; it is an *encoding* deficit, not an attention/effort effect.
- **Benedetto et al. (2015)** (Spritz): impairs **literal** comprehension and
  **increases visual fatigue** (fewer blinks); caused by losing parafoveal preview
  and regressions.
- **Acklin & Papesh (2017)**: static text beats RSVP at 700/1000 wpm; working
  memory capacity does not rescue it.

### Regressions & parafoveal preview (why RSVP costs comprehension)
- **Schotter, Tran & Rayner (2014)**: removing regressions (via trailing mask)
  **hurts comprehension**, even for unambiguous sentences. ~10–15% of reading time
  is normal regression.
- **Metzner et al. (2016)**: free reading beats word-by-word; regressions track the
  P600/good-comprehension response.
- **Rayner (1998/2009)**; **Vasilev & Angele (2016)**: perceptual span ≈ **3–4 chars
  left, 14–15 right** of fixation; preview benefit ~20–50 ms. One-word RSVP deletes
  this entirely.

### Chunk / window size
- **Cocklin, Ward, Chen & Juola (1984)**: optimal RSVP windows average **~12
  characters** (≈2–3 words); **idea-unit/phrase chunks beat equal-size random
  chunks**; overlapping successive segments *hurt*.
- **Masson (1983)**: a **sentence-end pause** (~500 ms, more gives no extra benefit)
  recovers page-level comprehension.
- **Williamson & Muter (1985)**: horizontal multi-word displays ≈ single-word;
  **vertical stacking is worse**.

### Speed-reading training, speed vs. eye movements
- **Miyata, Minagawa-Kawai, Watanabe, Sasaki & Ueda (2012)**, eye-tracking while
  reading Japanese novels: trained speed-readers read **2.1× faster but scored 15.7%
  lower** on comprehension; a high-level expert was **4.7× faster with 7.4% lower**
  comprehension — the trade-off narrows with expertise but never vanishes.
  Faster readers had **shorter fixations and larger horizontal saccades**; *higher*
  comprehension went with **more rightward regressions**. The expert read a nearly
  straight horizontal line and almost never looked back (0.1 s vs ~27 s for untrained).
  *Caveat: Japanese text (vertical, character-counted, no word spaces) — rates and
  spans don't transfer 1:1 to English.*

### Display format: ticker (smooth scroll) vs block (RSVP) vs page
- **Kang & Muter (1989)**: smooth-scrolling "Times Square" vs multi-word RSVP —
  **comprehension equal at 100–300 wpm**, but readers **preferred smooth scroll**
  over RSVP. The ticker is read via **optokinetic nystagmus** (continuous tracking).
- The older "scrolling is worse" findings used **choppy jumps**, not smooth motion:
  **Sekey & Tietz (1982)** (Times Square worst; line-flash "saccadic scrolling" ≈
  print), **Granaas et al. (1984)** (discrete leading < page),
  **Chen & Tsoi (1988)** (jump size matters, ~5 chars best at fast rates;
  user-controlled rate didn't improve comprehension).
- **Öquist & Lundin (2007)**, mobile: **page/paging fastest**, scrolling & RSVP
  slower, **RSVP slowest**; no comprehension difference; RSVP **suppressed** eye
  movements — which did *not* help readability.
- **Valsecchi et al. (2013)**: drifting text is read by smooth-pursuit + saccades
  (OKN-like); ORP / preferred-viewing effects still hold.
- **Harvey et al. (2017)** / **Harvey & Walker (2017)**: scrolling preserves
  word-level processing but **reduces sentence-level integration and inference**.
- **Fine & Peli (1995)**: normal vision reads RSVP faster than scroll;
  **Akthar et al. (2021)**: with central vision loss, scroll > static > RSVP.
- **Takeaway (alphabet):** a *smooth* ticker ≈ block/RSVP for **literal**
  comprehension; both cost **deep/inferential** comprehension vs a page. The
  ticker's edge is continuous tracking (OKN) + user preference — exactly why
  "leftmost = current" fits a ticker but not a stationary block (see rule below).

## Ground rules

Each rule cites its evidence and the code it binds. Status: **ENFORCED** (already
true), **TUNE** (present but should be re-checked), **TODO** (not yet built).

| # | Rule | Evidence | Code anchor | Status |
|---|------|----------|-------------|--------|
| 1 | Default speed sits in the normal reading range (≤300 wpm) | Brysbaert 2019 | `index.html` WPM default 300 | ENFORCED |
| 2 | Above ~350 wpm is *skimming* — label it, don't sell it as reading | Juola 2018, Ricciardi 2017, Miyata 2012 | `#skim` label via `syncSpeed()`, `Blast` preset | ENFORCED |
| 3 | Never claim comprehension is unaffected by speed | Rayner 2016 | UI copy | ENFORCED |
| 4 | Window ≈ 2–3 words (~12 chars); default 3; single horizontal line | Cocklin 1984, Williamson & Muter 1985 | `index.html` default 3, `#line` | ENFORCED |
| 5 | Prefer phrase/idea-unit boundaries over fixed word counts | Cocklin 1984 (Exp 3) | `rsvp-core.js` `chunk()` | ENFORCED — ends on the clause boundary nearest the nominal cut (±1 word, either side), so the window still obeys rule #4 |
| 6 | Pin the ORP left-of-centre; never centre words | O'Regan 1984/1992 | `rsvp-core.js` `pivotIndex`/`split` | ENFORCED |
| 7 | Keep a sentence-end pause (~500 ms equivalent) | Masson 1983 | `rsvp-core.js` `durationMs` (1.8×) | ENFORCED |
| 8 | Replay / regression must always be available | Schotter 2014, Metzner 2016, Miyata 2012 | seek + ←/→ keys | ENFORCED |
| 9 | Never stack words vertically | Williamson & Muter 1985 | `#line` flex row | ENFORCED |
| 10 | ORP is a tunable heuristic, not a proven optimum — keep it editable | Spritz undisclosed | `rsvp-core.js` `pivotIndex`, single `ORP_TABLE` | ENFORCED — one constant, tuned in one place |
| 11 | Reader font must stay monospace (pivot centring uses `ch` units) | rule #6 mechanism | `index.html` `#font` / `--read-font` | ENFORCED |
| 12 | A mode's highlight marks its own "current" unit: block → whole block; ticker → leading word; never borrow the other's convention | Kang & Muter 1989 (OKN ticker) + block semantics | `render()` (block) / `paintTicker()` (ticker) | ENFORCED |
| 13 | The ticker scrolls smoothly (continuous rAF translate), never choppy jumps | Kang & Muter 1989 vs Sekey & Tietz 1982 | `tickTicker()` rAF loop | ENFORCED |
| 14 | Sanitize extracted text before chunking: de-hyphenate line breaks, dot-leaders→…, drop decorative rule lines, collapse runaway punctuation/whitespace | robustness — PDF/MD artifacts ("Introduction…..5") poison ORP + chunker | `RSVP.sanitize()` in `setText()` | ENFORCED |
| 15 | Section detection is a heuristic that must degrade to nothing: use extractor-declared breaks (§ markers), fall back to heading-like lines, and hide the control below 2 sections | structural navigation needs structure extraction would otherwise discard; a wrong chapter list is worse than no list | `rsvp-core.js` `sections()`, `#sec` | ENFORCED |

## Control & feature governance

Reading rules say *what* the reader should do; these say *how we're allowed to
change it*. The core tension: the user gets **freedom** (toggles), but the app
owns **internal logic** (sensible defaults). A control without a reason is debt;
a removed control is a regression.

**Why a toggle exists at all:** only when (a) users genuinely disagree, or (b) no
single default is acceptable. Everything else is an internal default. The
redesign that hid `chunk`/`mode`/`font`/`size`/`word`/paste behind the `tune`
panel removed *no* behaviour but did silently relocate it — that is exactly the
class of change these rules exist to catch.

| # | Rule | Why |
|---|------|-----|
| G1 | A control earns its place: add a toggle/input only when users genuinely differ or no default is acceptable. Otherwise ship a default. | No control for a value that never changes (YAGNI). |
| G2 | Defaults are the product; toggles are overrides. The app must be fully usable untouched. | A toggle overrides a default — it never fills a design gap. |
| G3 | Internal logic decides what it can; the user decides only what's subjective. Mode-dependent behaviour (e.g. highlight unit: block=group, ticker=leading word) is encoded, never exposed. | Don't ask the user to configure what the app can decide. |
| G4 | Every control is registered in the inventory below (id, purpose, why-a-control, default, scope). No control ships without a row. | A control without a recorded reason can't be defended or safely removed. |
| G5 | The inventory is a contract: a rewrite/redesign must keep every row, or move it to *Deprecated* with a reason. No silent removal. | Feature loss by redesign is the failure this rule prevents. |
| G6 | No silent relocation. Moving a control (e.g. behind a hidden panel) changes discoverability and must be recorded — discoverability is a feature. | "It's still there, just hidden" is still a regression for the user. |
| G7 | Verify before "done": run `node check.js` (inventory ↔ HTML) **and** `node rsvp-core.js` after any frontend change. | Make the contract executable, not aspirational. |
| G8 | One stated reason per control, tied to a reading rule (#n) or a concrete need — never "nice to have". | Forces the design question instead of accreting knobs. |
| G9 | A contested design/UX change is debated to convergence in `DEBATE.md` (one thread per question) before it is coded; the thread's `Decision:` block is the record. | Disagreement is where the good option is found — make it explicit and findable. |

### Feature inventory (contract)

IDs the runtime depends on. The check script parses the first column of this
table and asserts each id still exists in `index.html`.

<!-- FEATURE-INVENTORY:START -->
| id | purpose | why a control (vs internal default) | default | scope |
|----|---------|--------------------------------------|---------|-------|
| `open`, `file`, `stage` | choose / drop / click-to-open a document | an OS file dialog is required; there is no default file | — | all |
| `play`, `seek`, `pos` | transport + position | required actions, no sensible default | paused, 0 | all |
| `wpm`, `wpmv` | reading speed | subjective and text-dependent (rules #1–#3) | 300 | block base |
| `skim` | >350 wpm honesty badge | internal default, deliberately **not** a toggle (rule #2) | hidden | all |
| `chunk`, `chunkv` | words per flash | window size is a genuine preference (rule #4) | 3 | block |
| `pace`, `pacev` | ticker speed trim | motion blur at wpm parity; perception varies | 60 | ticker |
| `mode`, `segMode` | block vs ticker display | both ≈ equal for literal comprehension but users differ (rules #12–#13) | block | all |
| `presets` | one-tap wpm+chunk starting points | a shortcut, not a default — a control (G1) | Balanced | all |
| `font` | monospace family | legibility preference, constrained to mono (rule #11) | Cascadia Mono | all |
| `fsize` | font size | legibility | 32pt | all |
| `wcol`, `bgcol`, `gcol` | highlight / background / guide-line colours | appearance is subjective | #ffb347 / #08090c / #2a2e36 | all |
| `whex`, `bhex`, `ghex` | type an exact hex for word / background / guide | the native picker cannot take pasted hex — exact entry is a user request | #FFB347 / #08090C / #2A2E36 | all |
| `wchips` | accent presets (amber / white / green / red / blue) | a handful of presets beats a blank picker; the swatch stays the custom option (user request) | amber | all |
| `restart`, `fs` | reset reading progress / fullscreen | required actions (restart sits immediately left of `segMode`, grouped with transport, so it reads as a reading action not a bar action — G6) | — | all |
| `sec` | jump to a detected section / chapter — hidden below 2 sections | coarse movement in a long text: `seek` scrubs precisely but says nothing about where you are (rule #15) | hidden | all |
| `resetDock` | return the floating control bar to its bottom dock | discoverability: the bar can be moved, so returning it must be a visible action (explicit counterpart to the double-click-grip reset) | — | all |
| `tune`, `tuneBtn` | "Options" toggle — reveal the rare knobs (label "Options"; sits at the **left** of the HUD; relocated from bottom-right on user request, G6) | progressive disclosure keeps the reader chrome quiet | closed | all |
| `grip` | drag handle (⋮⋮) for the floating bottom bar; double-click resets it to the bottom dock | discoverability — the bar is only draggable if it *looks* draggable (empty bar space also drags) | — | all |
| `ta`, `load` | paste text | a second input path | — | all |
| `meta` | source + size readout | feedback | — | all |
| `dock`, `guide`, `line`, `ticker`, `track`, `msg`, `prog` | reader surface + progress; `dock` is the draggable/floating wrapper of the control bar | ORP pinning / mode surfaces / ambient progress (rules #6, #9, #12) | — | all |
<!-- FEATURE-INVENTORY:END -->

Internal behaviours guarded by `node rsvp-core.js` (no toggle, by design):
ORP pivot (`pivotIndex`/`split`, rules #6/#10), sentence pause (`durationMs`,
rule #7), text sanitisation (`sanitize`, rule #14).

## Bibliography

- Acklin, D., & Papesh, M. H. (2017). Modern speed-reading apps do not foster reading comprehension. *American Journal of Psychology, 130*(2), 183–199.
- Akthar, F., Harvey, H., Subramanian, A., Liversedge, S. P., & Walker, R. (2021). A comparison of reading, in people with simulated and actual central vision loss, with static text, horizontally scrolling text, and rapid serial visual presentation. *Journal of Vision, 21*(12), 5.
- Benedetto, S., Carbone, A., Drai-Zerbib, V., Pedrotti, M., & Baccino, T. (2015). Rapid serial visual presentation in reading: The case of Spritz. *Computers in Human Behavior, 45*, 352–358.
- Brysbaert, M. (2019). How many words do we read per minute? A review and meta-analysis of reading rate. *Journal of Memory and Language, 109*, 104047.
- Brysbaert, M., & Nazir, T. (2005). Visual constraints in written word recognition: Evidence from the optimal viewing position effect. *Journal of Research in Reading, 28*(3), 216–228.
- Chen, H.-C., & Tsoi, K.-C. (1988). Factors affecting the readability of moving text on a computer display. *Human Factors, 30*(1), 25–33.
- Cocklin, T. G., Ward, N. J., Chen, H.-C., & Juola, J. F. (1984). Factors influencing readability of rapidly presented text segments. *Memory & Cognition, 12*(5), 431–442.
- Fine, E. M., & Peli, E. (1995). Scrolled and rapid serial visual presentation texts are read at similar rates by the visually impaired. *Journal of the Optical Society of America A, 12*(10), 2286–2292.
- Granaas, M. M., McKay, T. D., Laham, R. D., Hurt, L. D., & Juola, J. F. (1984). Reading moving text on a CRT screen. *Human Factors, 26*(1), 97–104.
- Harvey, H., Godwin, H. J., Fitzsimmons, G., Liversedge, S. P., & Walker, R. (2017). Oculomotor and linguistic processing effects in reading dynamic horizontally scrolling text. *Journal of Experimental Psychology: Human Perception and Performance, 43*(10), 1810–1823.
- Harvey, H., & Walker, R. (2017). Reading comprehension and its relationship with working memory capacity when reading horizontally scrolling text. *Quarterly Journal of Experimental Psychology*.
- Juola, J. F., Di Nocera, F., & Ricciardi, O. (2018). Rapid serial visual presentation: degradation of inferential reading comprehension as a function of speed. *International Journal of Human Factors and Ergonomics, 5*(4), 293–303.
- Juola, J. F., Ward, N. J., & McNamara, T. P. (1982). Visual search and reading of rapid serial presentations of letter strings, words, and text. *Journal of Experimental Psychology: General, 111*(2), 208–227.
- Kang, T. J., & Muter, P. (1989). Reading dynamically displayed text. *Behaviour & Information Technology, 8*(1), 33–42.
- Masson, M. E. J. (1983). Conceptual processing of text during skimming and rapid sequential reading. *Memory & Cognition, 11*(3), 262–274.
- Metzner, P., von der Malsburg, T., Vasishth, S., & Rösler, F. (2016). The importance of reading naturally: Evidence from combined recordings of eye movements and event-related brain potentials. *Cognitive Science*.
- Miyata, H., Minagawa-Kawai, Y., Watanabe, S., Sasaki, T., & Ueda, K. (2012). Reading speed, comprehension and eye movements while reading Japanese novels: Evidence from untrained readers and cases of speed-reading trainees. *PLoS ONE, 7*(5), e36091. https://doi.org/10.1371/journal.pone.0036091
- Nazir, T. A., Heller, D., & Sussmann, C. (1992). Letter visibility and word recognition: The optimal viewing position in printed words. *Perception & Psychophysics, 52*(3), 315–328.
- O'Regan, J. K., & Jacobs, A. M. (1992). Optimal viewing position effect in word recognition: A challenge to current theory. *Journal of Experimental Psychology: Human Perception and Performance, 18*(1), 185–197.
- O'Regan, J. K., Lévy-Schoen, A., Pynte, J., & Brugaillère, B. (1984). Convenient fixation location within isolated words of different length and structure. *Journal of Experimental Psychology: Human Perception and Performance, 10*(2), 250–257.
- Öquist, G., & Lundin, K. (2007). Eye movement study of reading text on a mobile phone using paging, scrolling, leading, and RSVP. *Proceedings of the 6th International Conference on Mobile and Ubiquitous Multimedia (MUM)*, 176–183.
- Potter, M. C., Kroll, J. F., & Harris, C. (1980). Comprehension and memory in rapid sequential reading. In R. S. Nickerson (Ed.), *Attention and Performance VIII*.
- Rayner, K. (1998). Eye movements in reading and information processing: 20 years of research. *Psychological Bulletin, 124*(3), 372–422.
- Rayner, K., Schotter, E. R., Masson, M. E. J., Potter, M. C., & Treiman, R. (2016). So much to read, so little time: How do we read, and can speed reading help? *Psychological Science in the Public Interest, 17*(1), 4–34.
- Ricciardi, O., & Di Nocera, F. (2017). Not so fast: A reply to Benedetto et al. (2015). *Computers in Human Behavior, 69*, 381–385.
- Schotter, E. R., Tran, R., & Rayner, K. (2014). Don't believe what you read (only once): Comprehension is supported by regressions during reading. *Psychological Science, 25*(6), 1218–1226.
- Sekey, A., & Tietz, J. (1982). Text display by "saccadic scrolling." *Visible Language, 16*(1), 62–76.
- Valsecchi, M., Gegenfurtner, K. R., & Schütz, A. C. (2013). Saccadic and smooth-pursuit eye movements during reading of drifting texts. *Journal of Vision, 13*(10), 8.
- Vasilev, M. R., & Angele, B. (2017). Parafoveal preview effects from word N+1 and word N+2 during reading: A critical review and Bayesian meta-analysis. *Psychonomic Bulletin & Review, 24*(3), 666–689.
- Williamson, N. J., & Muter, P. (1985). Reading without eye movements: Comprehension of RSVP text. *Unpublished manuscript / reported in Williamson et al. (1986)*.
