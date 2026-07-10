# Speech: lip-sync animations & read-along text

Spec for replacing the single looping TALKING animation with **phoneme-driven
mouth animation**, plus **on-screen dialogue text** — as a fallback for lines
whose audio is missing, as an accessibility layer, and as a **learn-to-read
aid**: the text of the line is displayed while it is spoken, with the
currently spoken word **highlighted karaoke-style** so a child can follow
along.

The problem splits cleanly in two, and this spec keeps them decoupled:

1. **Offline:** extract mouth-shape timing ("cues") and per-word timing from
   each dialogue WAV, per locale, with existing tools — committed to the repo
   as tiny text sidecar files, so the game, CI, and the web build never run
   the tools.
2. **Runtime:** load the sidecars next to the WAVs, drive the talking
   animation's frame from the active cue, and drive the text highlight from
   the active word, while the audio plays.

Written to be implemented phase by phase (each phase is an independent PR that
leaves the game working); tracking issue
[#69](https://github.com/potomak/VaniaVolpe/issues/69). Deliberate corner-cuts
are marked **[cut]** — this aims to be *clearly better than the status quo*,
not perfect.

## Current behaviour

- `actor_talk(actor, Mix_Chunk *dialog)` (`src/actor.c`) computes the line's
  duration (`get_chunk_time_ms`), plays the chunk, starts the TALKING
  animation — a small sprite sheet (fox: 3 frames) looping at 12 FPS,
  unrelated to what is being said — and returns to IDLE when the duration
  elapses.
- Dialogue is audio-only. Gina's lines are an exception in the data model:
  her Italian text lives as **string literals in C code** (`gina_say(gina,
  "Non ci arrivo!", voice())`, logged as `"Gina: %s"` for the headless test)
  while a single shared silent `voice.wav` placeholder satisfies the asset
  loader. Vania's lines exist only as per-line WAVs; the Italian transcript
  is `DIALOGS.md` (documentation, not data).

## Requirements

- **R1** — While a line plays, the actor's mouth matches the audio: closed in
  pauses, open/rounded/wide shapes tracking the phonemes.
- **R2** — Cue extraction is **offline** and works for every locale (it_IT
  today, en_US placeholders, future languages) without per-language setup.
- **R3** — Output of the offline step is a small, committed, git-diffable
  text file per WAV; builds and CI need no new tool.
- **R4** — Runtime cost is negligible: no audio analysis in the game; cue
  lookup is a cached linear scan.
- **R5** — Graceful degradation, in tiers: cues + viseme sheet → cue-driven
  mouth; cues missing or sheet not migrated → today's looping animation;
  audio missing but text present → talking state with estimated duration +
  forced text overlay.
- **R6** — A dialogue line is rendered as an on-screen **text overlay** while
  it is spoken: **on by default** (it is an accessibility and learn-to-read
  feature, not only a fallback), disable-able via a setting
  (`--subtitles=0` / `$VANIA_SUBTITLES=0` / web `?subtitles=0`), and **always
  shown** when the line has no audio. Text is per-locale.
- **R7** — While the audio plays, the **currently spoken word is
  highlighted** in the overlay, advancing through the line roughly in sync
  with the voice — close enough for a pre-reader to follow along, not
  studio-subtitle exact.
- **R8** — The headless tests keep passing: dialogue lines keep being emitted
  via `SDL_Log` in the existing format (`"Gina: %s"`).
- **R9** — Works unchanged across all four builds and with the depth-variant
  and camera specs (`DEPTH_AND_CAMERA.md`): mouth frames apply to the active
  depth variant; overlays are screen-space UI.

Non-goals: perfect phoneme accuracy, co-articulation/blending between shapes,
frame-exact word alignment (see the estimation trade-off below), runtime
speech analysis, voice-activity detection in placeholder WAVs,
right-to-left/complex text shaping, syllable-level highlighting.

## Part 1 — offline: audio → mouth cues

### Tool choice (trade-offs)

| Approach | Languages | Needs transcript | Output | Weight |
|---|---|---|---|---|
| **Rhubarb Lip Sync** *(chosen)* | English tuned; `phonetic` mode is language-agnostic | Optional (helps English mode) | Mouth shapes A–F + G,H,X with timestamps — exactly what we render | Single MIT-licensed CLI binary, WAV in → TSV/JSON out |
| Forced aligners (Montreal Forced Aligner, Gentle, aeneas) | Per-language models/dictionaries | **Required** | Phoneme-level timing (we'd still map phonemes → visemes ourselves) | Heavy (Kaldi/Python server); MFA has Italian models but big setup |
| ML phoneme recognizers (allosaurus, wav2vec2-phoneme) | Broad | No | Raw phoneme stream (again we map + smooth ourselves) | Python + model weights; overkill |
| Hand-authoring (Papagayo etc.) | Any | n/a | Exact | Human time per line; doesn't scale across locales |

**Rhubarb Lip Sync** ([github.com/DanielSWolf/rhubarb-lip-sync](https://github.com/DanielSWolf/rhubarb-lip-sync),
MIT) wins on every axis that matters here: it was built for exactly this use
case (2D adventure games — Thimbleweed Park), it outputs *mouth shapes with
durations* rather than raw phonemes (so the phoneme→viseme mapping problem
disappears), it is one static binary with no runtime, and its `-r phonetic`
recognizer works on any language by matching sounds instead of words —
which is what makes R2 hold for Italian. **[cut]** phonetic mode is less
accurate than transcript-guided English mode; for a toddler game where the
bar is "mouth closed in silence, moving on syllables, roughly the right
shape", that accuracy is plenty.

### Mouth-shape set (trade-offs)

Full phoneme sets (~40) and the classic Preston Blair 10 need more art and
buy little at this sprite size. Binary open/closed loses the charm. Chosen:
**Rhubarb's 6 basic shapes + rest = 7 frames**, its own recommended minimum:

| # | Shape | Mouth | Example sounds |
|---|---|---|---|
| 0 | X | rest / closed, relaxed | silence |
| 1 | A | closed, pressed lips | P, B, M |
| 2 | B | slightly open, teeth | K, S, T, EE |
| 3 | C | open | EH, AE |
| 4 | D | wide open | AA |
| 5 | E | rounded | AO, ER |
| 6 | F | puckered | UW, OO, W |

The extended shapes are disabled at generation (`--extendedShapes ""`), and
the runtime parser still accepts them defensively, mapping **G→B** (F/V) and
**H→C** (L) **[cut]** — so regenerating with extended shapes later needs no
code change. The frame indices above are the **canonical frame order** of a
cue-driven talking sprite sheet; index 0 = rest is deliberate so a stopped
animation (frame 0) shows a closed mouth.

### `.cues` file format

One sidecar per dialogue WAV, same basename: `examine_gate_1.wav` →
`examine_gate_1.cues`, living in the **same locale directory** (cues time
against a specific recording, so they are locale assets by nature;
`asset_resolve` and the web `--preload-file` setup pick them up with zero
build changes). Format — one cue per line, `<start_ms> <shape letter>`,
ASCII, sorted by time, the shape holding until the next cue; Rhubarb always
emits a final `X`:

```
0 X
230 B
340 D
520 A
620 C
910 X
```

Strict parser rules (the `.anim` parser's lesson, #44): `strtol` with bounds,
`0 <= ms`, non-decreasing, single letter in `XABCDEFGH`, ≤ `LIPSYNC_MAX_CUES`
(512 — a 10 s line is ~60 cues), tolerate `\r` and a missing final newline;
**any** violation → `SDL_LogError`, free, and return "no cues" (tier-2
fallback, R5).

### Transcript sidecars (`.txt`)

One per WAV, same basename (`examine_gate_1.txt`), UTF-8, the spoken line on
a single line. Triple purpose: the **overlay text** (Part 3), the input for
**word-timing estimation** (below), and the **Rhubarb dialog file** for
English-mode runs. Authoring: Vania it_IT lines copied out of `DIALOGS.md`
(which stays as the human-readable overview); Gina's lines moved out of the
C string literals — closing a real localization gap (#50: her Italian is
currently hardcoded in code and invisible to the locale system).

### Word timings (`.words`) — the read-along highlight data

Rhubarb outputs mouth shapes, **not** word timestamps, so the highlight (R7)
needs its own timing source. Options:

- **Forced alignment** (aeneas, Montreal Forced Aligner, Gentle): real
  audio↔text alignment. aeneas is the lightest and supports Italian via
  espeak, but word-level granularity is where these tools are weakest, and
  each adds a heavyweight Python/Kaldi dependency to the pipeline for every
  contributor who re-records a line.
- **ASR with word timestamps** (WhisperX & co.): accurate and multilingual,
  but a GPU-sized hammer for 5–15-word toddler lines.
- **Estimate from data we already have** *(chosen)* **[cut]**: the `.cues`
  file already says exactly *when speech happens* (every non-`X` span) and
  the `.txt` says *what words* are spoken. Distributing the words over the
  speech spans proportionally to their length gives a highlight that starts
  with the voice, pauses when the voice pauses, and lands within a word or
  so of the truth — plenty for follow-along reading, and it works for every
  locale by construction with **zero new tools**.

The estimate lives **offline** in the generator (not at runtime) and is
written to a third sidecar, so the engine only ever reads timings — if a
locale ever wants studio-grade sync, a forced aligner can produce the *same
file format* and nothing in the game changes.

Format — one word per line, `<start_ms> <end_ms> <word>`, in spoken order,
words matching the `.txt` split on spaces (punctuation stays attached):

```
230 480 Il
480 900 cancello
980 1150 è
1150 1600 chiuso.
```

Strict parser rules as for `.cues`: bounded `strtol`, `start <= end`,
non-decreasing starts, ≤ `LIPSYNC_MAX_WORDS` (64), non-empty word, tolerate
`\r`/missing final newline; any violation → log, free, "no word timings"
(the overlay still shows, just without the highlight).

Estimation algorithm (in `tools/gen_lipsync.py`, precise so it can be
implemented mechanically):

1. `words = text.split()`; `weights[i] = len(words[i]) + 1` (the `+1` keeps
   one-letter Italian words like "è" from vanishing).
2. Speech spans = maximal runs of consecutive non-`X` cues, each
   `[span_start, span_end)` (a span ends at the next `X` cue's time; Rhubarb
   always terminates with `X`). If there are no spans (silent placeholder
   WAV), use one span covering the whole chunk duration.
3. `ms_per_weight = total_span_ms / total_weight`. Walk the spans start to
   end, allocating each word `weights[i] * ms_per_weight` of *speech* time in
   order; a word's `start_ms`/`end_ms` are where its allocation begins/ends
   (an allocation that crosses a silence gap simply continues in the next
   span, so its window includes the gap — acceptable).
4. Round to int ms; force the last word's `end_ms` to the last span's end.

### Generator — `tools/gen_lipsync.py`

Python 3 (stdlib only, like the other tools). For every
`src/adventures/*/assets/<locale>/**/dialog/*.wav`:

1. Skip if the `.cues` file exists and is newer than the WAV (idempotent;
   `--force` to regenerate).
2. Run Rhubarb: `rhubarb -r phonetic --extendedShapes "" -f tsv -o <tmp>
   <wav>`; if a sibling `.txt` exists **and** the locale is `en_US`, use
   `-r pocketSphinx --dialogFile <txt>` instead (transcripts only help the
   English recognizer).
3. Convert the TSV (`<seconds>\t<shape>`) to the `.cues` format
   (`round(seconds * 1000)`), write next to the WAV.
4. If a sibling `.txt` exists, run the word-timing estimation above and
   write `<name>.words` next to the WAV too.

Binary discovery: `$RHUBARB` env var, else `rhubarb` on `PATH`; if absent,
exit with a one-line install hint (download the release binary; do **not**
add it to the repo or CI — `.cues` files are committed, R3). Pin the tested
version in the script header (1.14.0). Silent placeholder WAVs (the whole
`en_US` tree, Gina's `voice.wav`) yield a single `0 X` cue — harmless.

## Part 2 — runtime: cue-driven talking animation

### Rendering approach (trade-offs)

- **Mouth overlay sprite** (separate small mouth sheet composited over the
  head): art is shared across body poses and cheap per depth variant, but
  needs an anchor point per body frame, flip-aware anchor math, and a second
  render pass — several new failure modes.
- **New actor states per shape** (`TALKING_A` … ): explodes `ActorState`,
  the animation table, and the depth-variant validation matrix. No.
- **Full-frame viseme sheet in the existing TALKING slot** *(chosen)*: the
  talking sheet simply becomes "one full-body frame per mouth shape, in
  canonical order", and the engine **selects the frame from the active cue
  instead of time-looping**. Zero new render paths — `render_animation`
  already draws whatever `current_frame` says, `animation_update` ignores
  non-playing animations (verified in `image.c`), flip and depth variants
  work unchanged. Cost **[cut]**: the body is frozen per shape (no head-bob
  while talking) and each depth variant needs its own 7-frame sheet — both
  acceptable.

**Zero-art enablement trick:** a `.anim` file is just a list of source
rects, so a **7-line `talking.anim` that maps the canonical shapes onto the
existing 3 drawn frames** (e.g. X,A → the closed frame; B,E,F → the mid
frame; C,D → the open frame) turns on cue-driven mouths with **no new
image**. That alone beats the status quo — the mouth shuts during pauses and
flaps in rhythm with the actual speech — and real per-shape art later is
just a redrawn sheet, no code change.

### Data model

`src/lipsync.{c,h}` (new; add to `GAME_SRCS`):

```c
typedef enum mouth_shape { // values == canonical frame indices
  MOUTH_X, MOUTH_A, MOUTH_B, MOUTH_C, MOUTH_D, MOUTH_E, MOUTH_F,
  MOUTH_SHAPE_COUNT, // 7
} MouthShape;

#define LIPSYNC_MAX_CUES 512

typedef struct mouth_cue { Uint32 at_ms; Uint8 shape; } MouthCue;
typedef struct mouth_cues { MouthCue *cues; int length; } MouthCues; // length 0 = none

bool lipsync_load(Asset asset, MouthCues *out); // strict; false + empty on error
void lipsync_free(MouthCues *cues);
// Shape active at `ms`. *cursor caches the last index (init 0); resets
// backwards automatically if ms went backwards. O(1) amortized.
MouthShape lipsync_shape_at(const MouthCues *cues, Uint32 ms, int *cursor);
```

`ChunkData` (`sound.h`) grows the sidecars, loaded in `load_scene_chunks`
(`scene.c`) right after the WAV — derive sidecar names by swapping the
`.wav` suffix; **a missing sidecar is not an error** (SFX chunks have none):

```c
typedef struct chunk_data {
  Mix_Chunk *chunk;
  const char *filename;   // e.g. "examine_gate_1.wav"
  const char *directory;
  char *text;             // from <name>.txt (trimmed single line), or NULL
  MouthCues cues;         // from <name>.cues;  length 0 when absent/invalid
  WordTimings words;      // from <name>.words; length 0 when absent/invalid
} ChunkData;
```

with, in `lipsync.{c,h}` next to the cue types:

```c
#define LIPSYNC_MAX_WORDS 64

typedef struct word_timing { Uint32 start_ms; Uint32 end_ms; char *word; } WordTiming;
typedef struct word_timings { WordTiming *words; int length; } WordTimings;

bool lipsync_load_words(Asset asset, WordTimings *out); // strict; false + empty on error
void lipsync_free_words(WordTimings *words);
// Index of the word active at `ms`, or -1 (before/after/in a gap).
// *cursor caches like lipsync_shape_at.
int lipsync_word_at(const WordTimings *words, Uint32 ms, int *cursor);
```

`free_scene_chunks` frees `text` and calls `lipsync_free` /
`lipsync_free_words` (and nulls, per #38's convention).

### Actor changes (`actor.{c,h}`)

```c
// ActorSpec gains:
  const char *display_name; // "Vania", "Gina" — dialogue log prefix (R7)
  int talk_shape_frames;    // 0 = legacy looping TALKING; MOUTH_SHAPE_COUNT
                            // = cue-driven (sheet uses the canonical order)

// Actor gains:
  const MouthCues *talking_cues; // active line's cues; NULL = legacy mode
  int cue_cursor;
```

New talk API (replaces `actor_talk(Actor *, Mix_Chunk *)`; `fox_talk` /
`hen_talk` wrappers and all call sites migrate mechanically):

```c
// text == NULL → use dialog->text. dialog == NULL → no audio: text-only line.
void actor_talk(Actor *actor, const ChunkData *dialog, const char *text);
```

Behaviour:

1. Guards as today (return while WALKING). Resolve `line_text` (param else
   `dialog->text`) and `chunk` (`dialog ? dialog->chunk : NULL`); if both are
   NULL, return.
2. `talking_duration`: `get_chunk_time_ms(chunk)` if there is audio, else an
   estimate from the text — `SDL_max(1500, 80 * SDL_strlen(line_text))` ms
   (~12.5 chars/s; **[cut]** byte length over-counts accented UTF-8 slightly
   — fine).
3. If `line_text`: `SDL_LogInfo(…, "%s: %s", spec->display_name, line_text)`
   — this **centralizes** the log `gina_say` does today; keep the exact
   `"Gina: …"` strings so `test/scripts/gina.json` passes unchanged
   (`gina_say` becomes a thin wrapper or is deleted). Extra logged Vania
   lines are harmless — the harness asserts expected lines in order, not
   exhaustively.
4. Play the chunk if present (on the reserved dialogue channel once #33/#34
   land — same code, coordinate the PRs).
5. **Cue mode** if `dialog && dialog->cues.length > 0 &&
   spec->talk_shape_frames == MOUTH_SHAPE_COUNT`: set
   `talking_cues = &dialog->cues`, `cue_cursor = 0`, and set the active
   variant's TALKING animation `current_frame = MOUTH_X` — do **not** call
   `play_animation` (the animation stays non-playing; `animation_update`
   then never touches `current_frame`, and the global end-callback footgun
   of #35 is never armed). **Legacy mode** otherwise: `play_animation` as
   today.
6. Overlay: `subtitle_show(line_text, dialog ? &dialog->words : NULL,
   talking_duration)` when subtitles are enabled **or** `chunk == NULL`
   (Part 3).
7. `state = TALKING`, `started_talking_at = SDL_GetTicks()` as today.

`actor_update`, TALKING case: if `talking_cues`, each frame set
`animations[variant][TALKING]->current_frame =
lipsync_shape_at(talking_cues, now - started_talking_at, &cue_cursor)`
(writing to the *active* variant makes mid-line depth-band switches work for
free — next update re-stamps the new variant's sheet). On end-of-line: cue
mode resets `current_frame = MOUTH_X` and `talking_cues = NULL` (no
`stop_animation` — it was never playing); legacy mode stops as today.

Load-time validation: if `talk_shape_frames == MOUTH_SHAPE_COUNT`, every
variant's TALKING `.anim` must have exactly `MOUTH_SHAPE_COUNT` frames —
`SDL_LogError` and fail `actor_load_media` otherwise (this is the `.anim` ↔
spec frame-count contract, enforced loudly).

## Part 3 — text overlays: subtitles & read-along highlighting *(shipped)*

*Implementation notes: `subtitle_show` takes an explicit `force` flag (true
when the line has no audio chunk) instead of checking the chunk itself; the
engine clears the overlay on scene/adventure switches so a line never lingers
across scenes; the bundled face is Atkinson Hyperlegible Regular
(`assets/fonts/`, OFL). Everything else landed as specified below.*

### Text rendering (trade-offs)

- **Pre-rendered PNG per line** (the current localized-image pattern):
  zero new dependency, but ~1 image per line per locale (~40 and growing),
  a regeneration pipeline coupling art to script edits, and no path to any
  future dynamic text. Good fit for *buttons*, poor fit for *dialogue*.
- **Bitmap font atlas + custom blitter**: no dependency but the most code,
  and glyph coverage becomes our problem per locale.
- **SDL2_ttf** *(chosen)*: one well-trodden dependency that exists on every
  target (pkg-config natively; `-sUSE_SDL_TTF=2` Emscripten port; nothing
  special for the offscreen terminal/test builds), and
  `TTF_RenderUTF8_Blended_Wrapped` does layout + wrapping + UTF-8 for us.
  Any future on-screen text (#47's hub labels, #50) rides the same
  integration.

Integration mirrors the existing SDL2_image/SDL2_mixer pattern: pkg-config
`SDL2_ttf` in `CFLAGS`/`LDFLAGS`, a forwarding header
`include/SDL2_ttf/SDL_ttf.h`, an Emscripten shim
`src/emscripten/compat/SDL2_ttf/SDL_ttf.h`, `-sUSE_SDL_TTF=2` in `EM_PORTS`.

**Font**: bundle one OFL-licensed face with full Latin coverage —
suggestion: *Atkinson Hyperlegible* (highly legible, includes Italian
diacritics) — at top-level **`assets/fonts/`** (engine-level, deliberately
outside any adventure's `assets_root`; include the OFL license file). Load
it by plain relative path (works natively from the repo root and matches a
new `--preload-file assets/fonts` in `EM_PRELOAD`); iOS bundling joins #31's
follow-up. **[cut]** one font, one size (28 pt), no styling options.

### `src/subtitle.{c,h}` (new; add to `GAME_SRCS`)

```c
bool subtitle_init(void);                 // TTF_Init + load the font
// Replaces any current line. `words` may be NULL/empty → no highlight.
void subtitle_show(const char *text, const WordTimings *words,
                   Uint32 duration_ms);
void subtitle_clear(void);
void subtitle_render(SDL_Renderer *renderer); // no-op when nothing active
void subtitle_deinit(void);
bool subtitles_enabled(void);             // the global setting
```

- **Per-word layout** (needed for the highlight, and used even without one so
  there is a single code path): `subtitle_show` splits `text` on spaces,
  measures each word with `TTF_SizeUTF8` (plus one measured space width),
  wraps greedily at 700 px, centres each line horizontally, and stacks lines
  bottom-anchored (`y = WINDOW_HEIGHT - block_h - 24`). Each word is rendered
  **twice** into small cached textures at show time —
  `TTF_RenderUTF8_Blended` in white and in near-black (`{32,32,32}`) — ≤ 64
  tiny textures, freed on the next `subtitle_show`/`subtitle_clear`. Stores
  `shown_at = SDL_GetTicks()` and an expiry of `duration_ms + 300` grace.
  If `words` is present, its entries pair with the split words **by index**;
  on a count mismatch (stale sidecar) log once and disable the highlight for
  the line.
- **Highlight (R7):** each frame `subtitle_render` computes
  `elapsed = now - shown_at` and `active = lipsync_word_at(words, elapsed,
  &cursor)`. Draw order, all in **screen space**: the blended black backing
  rect (`SDL_SetRenderDrawBlendMode(BLEND)`, alpha ≈ 160, 12 px padding);
  then for the active word (if any) a filled warm-yellow rect
  (`{255, 204, 51}`, alpha 230, 4 px padding, the word's measured box);
  then every word — the active one with its near-black texture (dark on
  yellow), the rest with their white textures. The effect is a karaoke box
  bouncing word to word, pausing in the voice's pauses (the estimation
  algorithm guarantees that, since word windows only cover speech spans).
  Called at the very end of `game_render`, **after** the camera offset is
  reset (`DEPTH_AND_CAMERA.md`) — the overlay is UI like the hub button.
- **Setting:** on by default (R6 — it is a feature, not just a fallback);
  plumbing copies the locale pattern (`locale.c`): `--subtitles=0|1` argv
  flag, `$VANIA_SUBTITLES`, and the web shell forwards `?subtitles=` via
  `Module.arguments` exactly as it forwards `?lang=`. Flipping the default
  later is a one-line change.
- **Fallback tier (R5/R6):** `actor_talk` calls `subtitle_show`
  unconditionally when the line has no audio chunk — which finally makes
  Gina's placeholder-audio lines *visible in game*, not just in the log
  (without word timings her highlight simply doesn't render until her real
  recordings land). Note **[cut]**: a silent-but-present WAV (Gina today) is
  *not* detected as "no audio"; with subtitles on by default her lines show
  regardless.

## Phases

```
Phase 1: offline pipeline — gen_lipsync.py, .txt transcripts, committed .cues + .words (code-only, no engine change)
Phase 2: runtime playback — lipsync.c, ChunkData sidecars, cue-driven TALKING, remapped fox .anim (code-only)
Phase 3: read-along text  — SDL2_ttf, subtitle.c with word highlight, settings, no-audio fallback (code-only + font asset)
Phase 4: viseme art       — real 7-frame sheets for fox & hen (needs-art); Gina per-line audio (needs-audio)
```

Phase 2 depends on 1 (real cues to play); Phase 3 depends only on Phase 2's
`ChunkData.text` loading; Phase 4 is pure assets. Acceptance per phase:

1. `.cues`/`.txt`/`.words` exist for every Vania it_IT dialogue WAV; the
   script is idempotent; files are committed; `make web` picks them up with
   no Makefile change (verify in `build/web/catalog.html`'s asset listing);
   spot-check one `.words` file against the audio by ear.
2. All four builds green; `make test` passes byte-identical log
   expectations; with the remapped 7-frame `talking.anim` the fox's mouth
   visibly closes in pauses (manual playtest + unit tests on
   `lipsync_shape_at` / `lipsync_word_at` and both parsers: valid file,
   out-of-order times, bad letter, over-cap, CRLF, missing trailing
   newline, `start > end`).
3. All four builds green with SDL2_ttf linked; by default the browser
   playtest screenshots show wrapped Italian text with accents intact and
   the yellow highlight on different words in successive screenshots;
   `?subtitles=0` renders nothing; a text-only `actor_talk(NULL, "…")` line
   enters TALKING for the estimated duration and forces the overlay; a
   deliberate word-count mismatch disables only the highlight.
4. Real sheets replace the remapped `.anim`s (art brief = the shape table
   above); Gina's lines move fully to per-line WAV + sidecars and
   `gina_say`'s literals are deleted.

## Relevant code

- `src/actor.{c,h}` — `actor_talk` rework, cue-driven TALKING frames,
  `display_name` / `talk_shape_frames` spec fields.
- `src/lipsync.{c,h}` *(new)* — cue and word-timing parsing and lookup.
- `src/subtitle.{c,h}` *(new)* — overlay layout, word highlight, rendering.
- `src/sound.h` / `src/scene.c` — `ChunkData` sidecars in
  `load_scene_chunks` / `free_scene_chunks`.
- `src/adventures/*/…` — `fox_talk`/`hen_talk`/`gina_say` call-site
  migration; Gina strings → `.txt` sidecars.
- `tools/gen_lipsync.py` *(new)* — Rhubarb driver, TSV → `.cues`, word-timing
  estimation → `.words`.
- `Makefile` — `GAME_SRCS`, SDL2_ttf flags, `--preload-file assets/fonts`.
- `src/emscripten/shell.html` — forward `?subtitles=`.
- `DIALOGS.md` — stays the human-readable script; per-line `.txt` files
  become the data.
