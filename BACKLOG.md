# Backlog

Future improvements, mostly surfaced while play-testing with a toddler. Items are in
**priority order** — the top is what to do next. The leading number is just a position
for quick reference, **not a stable ID**, so items refer to each other by **title** (a
number would go stale on every reprioritization). Conventions: a PR that addresses an
item **removes it in that same PR** — or, if only part shipped, leaves a note on what's
done and what's left; shipped items are *not* archived here, the git history is the
record. Items that need art or audio assets authored separately are flagged.

## 1. Sync the Xcode project with the new layout
*Required for the iOS/Mac build (there is no Xcode on the build machine).*

The Adventure + Actor refactor (PR #10) and the per-adventure directory move
changed the source/asset layout, so `sdlexample.xcodeproj` needs to match before
the iOS/Mac target compiles:
- Add the common engine files now in `src/`: `actor.{c,h}`, `adventure.h`.
- Add the adventure module group `src/adventures/vania_fox_the_slide/`
  (`vania_fox_the_slide.{c,h}`, `fox.{c,h}`, and the scene files moved there) —
  the previously-referenced `src/fox.{c,h}`, `src/intro.c`, etc. moved here.
- Repoint the **asset folder references** to
  `src/adventures/vania_fox_the_slide/assets/*` (iOS bundles assets flat, so this
  is build-config only — runtime is unaffected).
- Ensure every `.c` is in the target's **Compile Sources** build phase.

The Makefile native/web/terminal builds already reflect the new layout.

## 2. Map input to logical render coordinates
*Code-only; real bug, surfaced by a `game.c` review.*

Hit-testing assumes window pixels equal logical pixels, but they don't: the window
is created `RESIZABLE | ALLOW_HIGHDPI` (`src/main.c`) and the renderer uses
`SDL_RenderSetLogicalSize(renderer, 800, 600)`, while every hit-test reads raw
`event->button.x/y` / `event->motion.x/y` and compares against logical-space rects
(`HUB_BUTTON` in `game.c`; the scene hotspots/POIs in `hub.c`, `playground.c`,
`playground_entrance.c`, `pool.c`, …, and `debug.c`). So on a resized or
Retina/HiDPI window — and the scaled web canvas — clicks land at the wrong spot
(e.g. the top-right back button misses).

- Add one helper that converts an event's coordinates to logical space via
  `SDL_RenderWindowToLogical(renderer, wx, wy, &lx, &ly)`, and use it everywhere a
  scene/engine reads pointer coordinates. The renderer must be reachable where input
  is processed (today `game_process_input`/scene `process_input` don't receive it —
  thread it through, or expose the active renderer).
- Fix the engine `HUB_BUTTON` test and the per-scene hotspot tests through the same
  helper so there's a single conversion point. (`actor.c` movement/click handling
  likely has the same assumption — audit it too.)

> **Note (attempted 2026-06):** a central `SDL_RenderWindowToLogical` conversion in
> `game_process_input` was tried and **backed out** — it regressed the web build. On
> Emscripten, SDL already reports mouse coordinates in *logical* space (which is why
> hit-testing works on web today without any conversion), so converting again
> double-scales them (badly under a HiDPI canvas). So this is **not** a uniform
> window→logical fix: it must be per-backend (convert on native desktop where events
> are in window pixels, skip on Emscripten) and needs verification on a real resized /
> HiDPI desktop window and an actual iPad — neither of which the CI smoke covers. The
> headless web smoke (`scratchpad/gina_smoke.js`) runs with `deviceScaleFactor: 2` and
> is a good regression guard for the "don't break web" half.

## 3. Reserve a mixer channel for dialogue
*Code-only; surfaced by a `main.c` review.*

Audio is the entire UX in this no-text game, but voice and SFX share one
unreserved channel pool: `actor_talk` (`src/actor.c`) and every sound effect play
with `Mix_PlayChannel(-1, …)` (first free channel), and the mixer opens with the
default 8 channels. So a toddler click-spamming SFX can steal the channel mid-line
and cut the fox/hen off — bad for a pre-reader who relies on the voice.

- In SDL init (`init_window`, `src/main.c`, mirrored in `src/main_terminal.c`):
  `Mix_AllocateChannels(16)` then `Mix_ReserveChannels(1)` to reserve channel 0.
- Play dialogue on the reserved channel in `actor_talk` (`src/actor.c`) —
  `Mix_PlayChannel(0, dialog, 0)` — so SFX (still `-1`, i.e. channels ≥ 1) can't
  evict it. Optionally halt channel 0 before a new line so dialogue can't overlap
  itself.
- Drive-by: name the `Mix_VolumeMusic(30)` magic number with a constant in
  `constants.h`. (`AUDIO_CHUNK_SIZE` is already `#define`d to 2048 — fine.)

## 4. Actor voice-channel interrupt
*Code-only; from the `actor.c` review.*

*Partly shipped in #28* — the crash/correctness guards are done: the halt-all bug
(`Mix_HaltChannel` guarded `>= 0`), the free-while-playing use-after-free, the NULL
dialog guard, `Mix_VolumeChunk`, and the dropped debug print.

*Remaining:* store the dialogue channel (`actor->voice_channel`) and play voice on the
channel reserved by *Reserve a mixer channel for dialogue*, so a new line/click can
interrupt the current one instead of overlapping or being dropped (do the two together).
And assert the move-sound volume is in 0–128 in the spec loader.

## 5. Per-instance animation end callback
*Code-only; from the `actor.c` / `image.c` review.*

*Partly shipped in #28* — the actor half is done: `on_end_walking` is now a
per-instance `Actor.on_end_walking` field, cleared before firing, so two actors' walk
callbacks can't clobber each other.

*Remaining:* the same global-callback pattern still lives in `image.c`
(`on_animation_playback_end`, a file-`static` set by `play_animation` and fired by
`stop_animation`). It's actively used by object animations (`playground_entrance.c`
plays the gate/shovel with non-NULL callbacks), and the actor's own
`play_animation(..., NULL)` calls reset it to NULL — so a walk starting mid object
animation drops that animation's callback. Give `AnimationData` a per-instance end
callback (+ optional `void *userdata`), set by `play_animation`, cleared before firing.

## 6. Decouple animation update from render (+ per-animation frame rate)
*Code-only; from the `image.c` review.*

`render_animation` (`image.c`) advances the current frame, recomputes `loop_count`,
and calls `stop_animation` — which fires the end callback — all inside the render
path. So animation speed tracks *render* frequency (render twice → talk plays twice
as fast), and an end callback (which can switch scene/adventure) fires mid-render.

- Extract `animation_update(animation, now_ms)` that advances frames and handles
  `ONE_SHOT` completion/stop, and call it from `actor_update` (and any other animation
  owner). Keep `render_animation` pure — just blit the current clip.
- Make the frame duration a per-animation field (default 83 ms; note 83 ms is
  ~12.05 FPS and drifts ~1 frame / 20 s) carried on `ActorAnimSpec`/`AnimationData`,
  so different actors can animate at different speeds.
- While here: `render_animation` should early-out when `image.texture == NULL` (failed
  load) instead of passing it to `SDL_RenderCopyEx`; call
  `SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND)` after creation (defensive —
  alpha PNGs like the hen rely on blend, though `SDL_CreateTextureFromSurface` usually
  sets it); NULL-check the two `make_animation_data` mallocs; add a `#define` for the
  cyan color key in `image.h`; document that `ImageData.filename/directory` are
  *borrowed* (owned by the `ActorSpec` literals — ties to the lifetime contract in
  *asset_path(): caller-owned buffer*); and consider a `play_animation` restart option
  (today a second `play_animation` on an already-playing animation is a silent no-op).

## 7. Unify adventure/scene transitions and harden the scene router
*Code-only; correctness + maintainability, from the `game.c` review.*

- **Activation-order bug:** `set_active_scene` (`game.c`) calls the new scene's
  `on_scene_active()` *before* assigning `game.current_scene = scene`, so any
  `on_scene_active` that reads `current_scene` sees the *old* scene. Set the field
  first, then call `on_scene_active()`.
- **One transition function:** `set_current_adventure` and `switch_to_adventure`
  differ only by whether they run scene inactive/active callbacks, which is exactly
  why `main.c` needs the `set_active_scene(hub.entry_scene)` hack at startup. Merge
  them into a single `adventure_switch_to()` that always does old-scene-inactive →
  set new adventure/scene → adventure `on_enter` → new-scene-active. Removes the hack
  (supersedes the single-source-adventure-list / scene-hack part of *main.c
  app-lifecycle hardening*).
- **Bounds/NULL-safe router:** `scene_instance` (`game.c`) returns a `Scene` by
  value with no bounds check and dereferences `game.current_adventure` unconditionally.
  Return a `const Scene *`, assert `current_adventure != NULL` and
  `0 <= scene < scenes_count` (needs a scene count on `Adventure`), so a bad index or
  pre-init call fails loudly instead of corrupting/crashing.
- **Pass the hub explicitly** instead of inferring it as `registered[0]` in
  `register_adventures`, and either copy the registry array the engine keeps or
  document that the caller must keep it alive for the whole run (today it survives
  only because `main` passes a `static` array).

## 8. asset_path(): caller-owned buffer
*Code-only; latent footgun, from the `asset.c` review.*

`asset_path` returns a pointer into a single function-`static resolved_path[1024]`,
so two live results clobber each other — e.g. `load_two(asset_path(a), asset_path(b))`
would resolve both to the second path. Every call site today consumes the result
immediately in one load (`IMG_Load`/`Mix_LoadWAV`/`Mix_LoadMUS`/`load_animation_data`),
so it's latent — but with two adventures whose assets share filenames across different
roots (`walking.wav` in both, `pool/float.png` vs `tree/float.png`, many
`background.png`), it's the classic "second texture loads the wrong file" bug waiting
to happen.

- Add a caller-owned variant — `bool asset_resolve(Asset, char *buf, size_t n)`
  returning whether the localized layer was used (vs the common fallback) — and let
  the caller own the storage. Keep `asset_path()` as a thin, documented wrapper for
  the existing single-use sites, or migrate them.
- While changing the API, document the lifetime contract of `asset_set_root` /
  `asset_set_locale`: they store the pointer, not a copy, so the string must outlive
  the asset system. True today (string literals + `detect_locale` statics); either
  state the contract or `strdup`/free on set.

## 9. Scene asset lifecycle: pass `Scene *`, unwind partial loads, null freed chunks
*Code-only; a leak + a dangling-pointer bug, from the `scene.c` review.*

`load_scene_images`/`load_scene_chunks`/`free_scene_images`/`free_scene_chunks`
(`scene.c`) all take `Scene` **by value**. It works only because `images`/`chunks`
are pointers (the writes go through the copied pointer), but it's misleading, copies
the whole struct per call, and means `free_scene_chunks` can't null the freed `chunk`
pointers — so they dangle.

- Take `Scene *scene` in all four; after `Mix_FreeChunk(scene->chunks[i].chunk)` set
  `scene->chunks[i].chunk = NULL` (the image side already nulls the texture in
  `free_image_texture`).
- **Unwind partial loads:** if image/chunk *i* fails to load, items `0..i-1` stay
  loaded and the caller gets `false` with no handle to free them. Free what was loaded
  before returning `false` (or have the caller call the matching `free_scene_*`).
- Minor pathing hardening in `nearest_walkable_point` (`scene.c`): the `long`
  distance math is fine at 800×600 but use a fixed-width `int64_t` (or clamp) to keep
  static analyzers quiet; and document that `non_walkable` is a *single* exclusion
  zone — if a scene (e.g. Gina's pool) needs more, take `const SDL_Rect *blocked, int
  count` (a stepping stone toward the full pathfinding in *Walk around non-walkable
  areas*).

## 10. Tap-the-fox → contextual voice hint
*Needs new Italian `.wav` voice lines.*

When the player taps the **fox** itself (a common toddler instinct), have her say
what to do next, by world state, plus a nudge that you move by tapping where you
want her to go. Hints are voice-only (the game has no on-screen text).

- **Hit-test (code):** add `bool fox_contains_point(Fox*, SDL_Point)` from the current
  animation frame's bounds around `current_position`; hit-test the fox at the top
  of each scene's `process_input` (before movement handling).
- **Hints (code + audio):** on a fox tap, a per-scene `give_hint()` picks a line by
  world state and plays it via `fox_talk()` (`src/fox.c`):
  - *playground_entrance:* key not revealed → "dig with the shovel"; key revealed
    but no key → "go get the key"; has key → "use the key on the gate".
  - *playground:* no acorns → "shake the acorns down"; has acorns → "give them to
    the squirrel"; has peg → "fix the slide"; slide fixed → "let's slide!".
- **Assets:** one Italian `.wav` per state above, placed under
  `playground/dialog/` and `playground_entrance/dialog/`.

## 11. Climb-stairs & slide-down animations
*Needs new sprite sheets.*

The fox currently lacks dedicated animations for using the slide. Add two,
following the existing animation pattern (the ~10 steps mirroring
`walking`/`talking`/`sitting`/`waving` across `src/fox.h` and `fox.c`: new
struct field + `FoxState` + `make_fox` + `fox_load_media` + `fox_render` +
`fox_update` + `fox_face` + `fox_free` + a trigger helper).

- **Climb:** in the slide-fixed path of `maybe_use_slide` (`src/playground.c`),
  play a climb animation up the stairs before snapping to `START_SLIDE_POS`.
- **Slide:** during the sigmoid descent in `update()` the fox currently shows the
  *talking* animation; swap in a dedicated `sliding` animation for the descent
  (keep the audio).
- **Assets:** `fox/climbing.png` + `climbing.anim`, `fox/sliding.png` +
  `sliding.anim` (CSV `x,y,w,h` per frame, 12 FPS).

## 12. Drag-to-move the fox
*Optional polish, code-only.*

Today a press triggers movement to the press point on `MOUSEBUTTONDOWN`, so a drag
gesture just moves the fox to where the drag started. Make dragging intuitive: if
a press starts on the fox, treat the `MOUSEBUTTONUP` release point as the
destination (drag-to-move). Both scenes' `process_input`. Layer on after
*Tap-the-fox → contextual voice hint* (it reuses the same fox hit-test).

## 13. main.c app-lifecycle hardening
*Code-only; small, low-risk correctness fixes from a `main.c` review.*

A cluster of fixes in `src/main.c` (mirror to `src/main_terminal.c` where the same
code exists):

- **Delta-time first frame:** `last_frame_time` starts at `0`, so frame 1 gets a delta
  of "time since SDL init" (often 0.1–2 s) and actors teleport. Set
  `last_frame_time = SDL_GetTicks()` right before entering the loop (as
  `main_terminal.c` already does), read `SDL_GetTicks()` once per `update()`, and store
  ticks as `Uint32` (wraps cleanly) instead of `int`.
- **Shutdown order + leak:** tear down in reverse-init order
  (`Mix_CloseAudio()` → `Mix_Quit()` → `IMG_Quit()` → renderer/window → `SDL_Quit()`);
  today `destroy_window()` calls `SDL_Quit()` *before* `destroy_image()`/`destroy_sound()`,
  and `Mix_CloseAudio()` (matching `Mix_OpenAudio`) is missing entirely.
- **Diagnostics + renderer fallback:** include `SDL_GetError()` / `IMG_GetError()` /
  `Mix_GetError()` in every failure message (most are generic, e.g. "Error creating SDL
  Window."), and fall back to `SDL_RENDERER_SOFTWARE` when `ACCELERATED | PRESENTVSYNC`
  fails, to avoid a black screen on software/web contexts.
- **Small consistency:** `init_window` is declared `int` but returns `true`/`false` —
  return `bool`; unify logging on `SDL_Log*` (drop the mixed `fprintf(stderr, …)`) so
  messages reach the browser console / logcat / stderr.
- **Registration single source of truth + drop the scene hack:** the hub `content[]`
  (2 adventures) and engine `all[]` (3, incl. the hub) duplicate the adventure list in
  `SDL_main`, so a new adventure must be added in both places (and can be forgotten).
  Derive both from one list. And make scene/adventure activation run the first scene's
  enter callback itself, so the `// Hack to execute lifecycle callbacks for the first
  scene` (`set_active_scene(hub.entry_scene)`) is no longer needed in `main` (this
  overlaps the transition unification in *Unify adventure/scene transitions and harden
  the scene router*).

*Evaluated and deferred (from the same review, intentionally not promoted):*
- **Bundle the globals (`window`/`renderer`/`last_frame_time`) into an `AppContext`:**
  low ROI now — `renderer` is already threaded as a parameter and there are only
  three globals (duplicated across the two `main*.c`). Reconsider when a third
  backend lands (ties to the terminal split).
- **Drop the iOS `SDL_main` / `SDL_UIKitRunApp` block:** intentional and correct;
  the iOS/Mac project isn't even synced yet (*Sync the Xcode project*) — leave as-is.
- **Emscripten "tap to start" + `Mix_ResumeAudio` overlay:** the browser audio
  unlock (AudioContext resume on first gesture) is already handled in
  `src/emscripten/shell.html`; keep as a watch-item, not a new item.

## 14. Actor state-machine hardening
*Code-only; small correctness/robustness nits, from the `actor.c` review.*

*Partly shipped in #28* — the divide-by-zero is fixed: tapping on the actor made
`dist == 0` and `direction` NaN; `actor_walk_to` now measures distance first and
"arrives" within `ACTOR_ARRIVE_EPSILON` (a named constant), using `dx*dx` instead of
`powf`.

*Remaining:*

- **Time base:** `actor_update` reads `float ticks = SDL_GetTicks()` and compares
  against `Uint32` talk timing — use `Uint32` throughout so it stays precise and wraps
  cleanly (same fix family as the delta-time fix in *main.c app-lifecycle hardening*).
- **Consistency:** `actor_play_state` sets `state` even when the animation is NULL,
  unlike `actor_talk`/`actor_walk_to` which refuse when busy — pick one policy (e.g.
  return `bool` success).
- **Compiler help:** drop the `case ACTOR_STATE_COUNT` sentinel from the `actor_update`
  switch and build with `-Wswitch` so a newly added `ActorState` that isn't handled is
  flagged.
- **Allocation safety:** `make_actor` `malloc`s with no NULL check and no cleanup if a
  later animation load fails — add the check. And add a one-line comment that an actor's
  animations are per-instance (cloned in `make_actor`), so the next contributor knows
  `actor_face` flipping "all animations" won't turn both actors.

## 15. Harden the `.anim` CSV parser
*Code-only; prevents overflow on malformed clip files, from the `image.c` review.*

`load_animation_data` (`image.c`) trusts its input completely:
- `num_i` is unbounded — a 6+ digit field overflows the 6-byte `num` buffer (stack
  smash). Bound it.
- `rect_i` is never checked `< 4` — a line with >4 fields writes past `rect[4]`.
- `row` is never checked `< animation->frames` — more lines than the spec's frame count
  overflow the heap `sprite_clips[]`. (This check also enforces the `.anim` ↔
  `ActorAnimSpec` frame-count contract — a real mismatch risk when editing a `.anim`
  without bumping `frames`.)
- A final line with no trailing newline is dropped (a rect is only committed on `\n`) —
  flush at EOF.
- CRLF leaves `\r` in the buffer (`atoi` tolerates it by luck); strip it. And `atoi`
  silently yields 0 on garbage.

Add the three bounds checks + EOF flush, or replace the hand-rolled scan with
`sscanf("%d,%d,%d,%d", …)` / `strtol` per line (handles whitespace and CRLF, no manual
buffer) — the files are tiny.

## 16. Portable, single-hit asset resolution
*Code-only; portability + diagnostics, from the `asset.c` review.*

- Replace the `access(F_OK)` pre-check with a real open probe (`SDL_RWFromFile`) — or,
  better, try-load-then-fallback (attempt the locale path, and only on failure try
  `common`). This drops the POSIX `<unistd.h>` dependency (Windows has no `access`),
  avoids the extra virtual-FS stat on Emscripten, and closes the TOCTOU window between
  the check and the real `IMG_Load`/`Mix_LoadWAV`.
- Check `snprintf` truncation in `build_path` instead of truncating silently, and name
  the `1024` buffer (`#define ASSET_PATH_MAX`) — Windows `PATH_MAX` is 260, Linux 4096.
- Log once when *neither* the locale nor the common file exists, so a typo'd filename
  surfaces here instead of as a cryptic downstream "IMG_Load: Couldn't open …".

## 17. Toddler-friendly hub button and touch input
*Code-only; UX for the target age, some overlap with *Sync the Xcode project* (iOS).*

- The back-to-hub `HUB_BUTTON` is 40×40 logical px — below Apple's 44pt minimum and
  small for a 2-year-old. Bump to ≥64×64 with a comfortable margin (a
  `#define HUB_BUTTON_SIZE` rather than the inline literal).
- Ignore key auto-repeat on the debug toggle (`if (event->key.repeat) break;` in
  `game_process_input`) so holding **D** doesn't strobe debug mode at frame rate.
- Handle `SDL_FINGERDOWN`/touch explicitly for iPad (toddlers tap with fingers).
  Mouse events are synthesized from touch on web/mobile so it works today, but
  explicit touch (and ignoring the synthesized duplicate) is cleaner — pairs with
  the iOS sync (*Sync the Xcode project*) and *Map input to logical render coordinates*.

## 18. Walk around non-walkable areas
*Future; see [`MOVEMENT.md`](MOVEMENT.md).*

Movement is a straight line with no collision/pathfinding, so the fox can walk
through non-walkable areas (e.g. the sandbox). Replace straight-line movement with
obstacle-avoiding routing — a corner waypoint around the rectangle for the simple
single-obstacle case, or a navigation grid / navmesh + A\* for the general case.
Keep the engine simple until this becomes a felt gameplay problem.

## 19. Lazy-load adventures (on demand, not all up front)
*Performance, especially on mobile; enables per-adventure asset downloads.*

The engine currently initializes and loads media for **every** registered
adventure at startup: `game_init` / `game_load_media` (`src/game.c`) iterate all
adventures, calling `adventure_init` / `adventure_load_media` (`src/adventure.c`)
on each. So every adventure's assets are created up front — wasteful on memory and
startup time, and on the web the single `index.data` (all adventures' art/audio)
must finish downloading before the hub even appears.

Instead, load only what's needed:
- At startup, init + load **only the hub and the default active adventure**.
- Init + load each other adventure **on demand** — the first time it's selected
  (`switch_to_adventure`, `src/game.c`) — then keep it resident (or free it on
  exit if memory pressure warrants).
- **Web:** split assets per adventure so the user only downloads an adventure's
  assets after selecting it — e.g. per-adventure Emscripten data packages fetched
  on demand instead of one monolithic `--preload-file` `index.data`.

Care points: `game_deinit` frees *all* adventures today, so it must only free what
was actually loaded; guard against loading an adventure twice; and because a
scene's `process_input` can call `switch_to_adventure`, the on-demand load must
happen *before* activating the new adventure's entry scene. The per-adventure
methods (`adventure_init` / `adventure_load_media` / `adventure_deinit`, #16)
already encapsulate load/teardown, so this is mostly a scheduling change in
`game.c` plus a per-adventure "loaded" flag.

## 20. Internationalization follow-ups
*The locale-aware asset layer shipped; these extend it.*

Localization now works via `assets/common/` + `assets/<locale>/` and a strict
`asset_path` (locale → common), with the locale chosen at startup
(`--locale=` / `$VANIA_LOCALE` / `$LANG`; web: browser language or `?lang=`).
Remaining work:
- **In-hub language picker** + runtime switch — the hub lists languages and
  re-loads media on change (ties into *Lazy-load adventures*: a locale switch is a
  media reload).
- **iOS bundling** — drop the flat-filename shortcut in `asset_path` and bundle
  `common/` + `<locale>/` as Xcode folder references so the same two-step lookup
  works on iOS (extends *Sync the Xcode project*). Note the iOS branch currently
  returns the bare filename and ignores `asset_root`, so per-adventure
  `asset_set_root()` is a no-op there; the fix must prepend the root (use
  `SDL_GetBasePath()` for a portable base) and run the locale→common lookup. Consider
  a bare-language fallback (`it` as well as `it_IT`), since iOS locale codes are often
  just the language.
- **Per-locale web bundles** — preload only the chosen language so a browser
  downloads one locale, not all of them (pairs with *Lazy-load adventures*).
- **Real translations** — replace the generated `en_US` placeholders (English
  voice lines + intro/outro/button art); add further locales as needed.
- **On-screen text** — if any is ever added, a per-locale key→string table is the
  analog of this asset layer (the Vania script is in the adventure's `DIALOGS.md`).

## Asset checklist (to be provided)

- Italian voice `.wav` per hint state → `playground*/dialog/` (for *Tap-the-fox →
  contextual voice hint*).
- `fox/climbing.png` + `climbing.anim`, `fox/sliding.png` + `sliding.anim` (for
  *Climb-stairs & slide-down animations*).
