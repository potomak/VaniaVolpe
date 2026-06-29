# Backlog

Future improvements, mostly surfaced while play-testing with a toddler. Each task
is independent; some need art/audio assets that have to be authored separately —
those are flagged. Tasks are lettered for easy reference; A and B already shipped.

## Open

### Task C — Tap-the-fox → contextual voice hint
*Needs new Italian `.wav` voice lines.*

When the player taps the **fox** itself (a common toddler instinct), have her say
what to do next, by world state, plus a nudge that you move by tapping where you
want her to go. Hints are voice-only (the game has no on-screen text).

- **C1 (code):** add `bool fox_contains_point(Fox*, SDL_Point)` from the current
  animation frame's bounds around `current_position`; hit-test the fox at the top
  of each scene's `process_input` (before movement handling).
- **C2 (code + audio):** on a fox tap, a per-scene `give_hint()` picks a line by
  world state and plays it via `fox_talk()` (`src/fox.c`):
  - *playground_entrance:* key not revealed → "dig with the shovel"; key revealed
    but no key → "go get the key"; has key → "use the key on the gate".
  - *playground:* no acorns → "shake the acorns down"; has acorns → "give them to
    the squirrel"; has peg → "fix the slide"; slide fixed → "let's slide!".
- **Assets:** one Italian `.wav` per state above, placed under
  `playground/dialog/` and `playground_entrance/dialog/`.

### Task D — Drag-to-move the fox
*Optional polish, code-only.*

Today a press triggers movement to the press point on `MOUSEBUTTONDOWN`, so a drag
gesture just moves the fox to where the drag started. Make dragging intuitive: if
a press starts on the fox, treat the `MOUSEBUTTONUP` release point as the
destination (drag-to-move). Both scenes' `process_input`. Layer on after C.

### Task E — Climb-stairs & slide-down animations
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

### Task F — Walk around non-walkable areas
*Future; see [`MOVEMENT.md`](MOVEMENT.md).*

Movement is a straight line with no collision/pathfinding, so the fox can walk
through non-walkable areas (e.g. the sandbox). Replace straight-line movement with
obstacle-avoiding routing — a corner waypoint around the rectangle for the simple
single-obstacle case, or a navigation grid / navmesh + A\* for the general case.
Keep the engine simple until this becomes a felt gameplay problem.

### Task G — Sync the Xcode project with the new layout
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

### Task H — Adventure hub UI (Rollout Step 2)
*Foundation landed in PR #10.*

Add a hub scene — the app shell that lists the registered adventures and launches
the selected one, returning to it on completion instead of starting straight into
a single adventure. Then author a **second adventure** as a module, introducing
**Gina Gallina** as a second `Actor` spec, to exercise the module seams and the
actor abstraction. The engine is already adventure-agnostic (Adventure registry +
generic Actor), so this is mostly new content plus a small menu UI.

### Task I — Lazy-load adventures (on demand, not all up front)
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

### Task J — Internationalization follow-ups
*The locale-aware asset layer shipped; these extend it.*

Localization now works via `assets/common/` + `assets/<locale>/` and a strict
`asset_path` (locale → common), with the locale chosen at startup
(`--locale=` / `$VANIA_LOCALE` / `$LANG`; web: browser language or `?lang=`).
Remaining work:
- **In-hub language picker** + runtime switch — the hub lists languages and
  re-loads media on change (ties into Task I: a locale switch is a media reload).
- **iOS bundling** — drop the flat-filename shortcut in `asset_path` and bundle
  `common/` + `<locale>/` as Xcode folder references so the same two-step lookup
  works on iOS (extends Task G). Note the iOS branch currently returns the bare
  filename and ignores `asset_root`, so per-adventure `asset_set_root()` is a no-op
  there; the fix must prepend the root (use `SDL_GetBasePath()` for a portable base)
  and run the locale→common lookup. Consider a bare-language fallback (`it` as well
  as `it_IT`), since iOS locale codes are often just the language.
- **Per-locale web bundles** — preload only the chosen language so a browser
  downloads one locale, not all of them (pairs with Task I lazy-loading).
- **Real translations** — replace the generated `en_US` placeholders (English
  voice lines + intro/outro/button art); add further locales as needed.
- **On-screen text** — if any is ever added, a per-locale key→string table is the
  analog of this asset layer (the Vania script is in the adventure's `DIALOGS.md`).

### Task K — Reserve a mixer channel for dialogue
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

### Task L — main.c app-lifecycle hardening
*Code-only; small, low-risk correctness fixes from the same review.*

A cluster of fixes in `src/main.c` (mirror to `src/main_terminal.c` where the same
code exists):

- **L1 — delta-time first frame:** `last_frame_time` starts at `0`, so frame 1 gets
  a delta of "time since SDL init" (often 0.1–2 s) and actors teleport. Set
  `last_frame_time = SDL_GetTicks()` right before entering the loop (as
  `main_terminal.c` already does), read `SDL_GetTicks()` once per `update()`, and
  store ticks as `Uint32` (wraps cleanly) instead of `int`.
- **L2 — shutdown order + leak:** tear down in reverse-init order
  (`Mix_CloseAudio()` → `Mix_Quit()` → `IMG_Quit()` → renderer/window →
  `SDL_Quit()`); today `destroy_window()` calls `SDL_Quit()` *before*
  `destroy_image()`/`destroy_sound()`, and `Mix_CloseAudio()` (matching
  `Mix_OpenAudio`) is missing entirely.
- **L3 — diagnostics + renderer fallback:** include `SDL_GetError()` /
  `IMG_GetError()` / `Mix_GetError()` in every failure message (most are generic,
  e.g. "Error creating SDL Window."), and fall back to `SDL_RENDERER_SOFTWARE` when
  `ACCELERATED | PRESENTVSYNC` fails, to avoid a black screen on software/web
  contexts.
- **L4 — small consistency:** `init_window` is declared `int` but returns
  `true`/`false` — return `bool`; unify logging on `SDL_Log*` (drop the mixed
  `fprintf(stderr, …)`) so messages reach the browser console / logcat / stderr.
- **L5 — registration single source of truth + drop the scene hack:** the hub
  `content[]` (2 adventures) and engine `all[]` (3, incl. the hub) duplicate the
  adventure list in `SDL_main`, so a new adventure must be added in both places (and
  can be forgotten). Derive both from one list. And make scene/adventure activation
  run the first scene's enter callback itself, so the `// Hack to execute lifecycle
  callbacks for the first scene` (`set_active_scene(hub.entry_scene)`) is no longer
  needed in `main` — otherwise every adventure author copies the hack.

*Evaluated and deferred (from the same review, intentionally not promoted):*
- **Bundle the globals (`window`/`renderer`/`last_frame_time`) into an `AppContext`:**
  low ROI now — `renderer` is already threaded as a parameter and there are only
  three globals (duplicated across the two `main*.c`). Reconsider when a third
  backend lands (ties to the terminal split).
- **Drop the iOS `SDL_main` / `SDL_UIKitRunApp` block:** intentional and correct;
  the iOS/Mac project isn't even synced yet (Task G) — leave as-is.
- **Emscripten "tap to start" + `Mix_ResumeAudio` overlay:** the browser audio
  unlock (AudioContext resume on first gesture) is already handled in
  `src/emscripten/shell.html`; keep as a watch-item, not a new task.

### Task M — Map input to logical render coordinates
*Code-only; real bug, surfaced by a `game.c` review.*

Hit-testing assumes window pixels equal logical pixels, but they don't: the window
is created `RESIZABLE | ALLOW_HIGHDPI` (`src/main.c`) and the renderer uses
`SDL_RenderSetLogicalSize(renderer, 800, 600)`, while every hit-test reads raw
`event->button.x/y` / `event->motion.x/y` and compares against logical-space rects
(`HUB_BUTTON` in `game.c:102`; the scene hotspots/POIs in `hub.c`, `playground.c`,
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

### Task N — Unify adventure/scene transitions and harden the scene router
*Code-only; correctness + maintainability, from the `game.c` review.*

- **Activation-order bug:** `set_active_scene` (`game.c:63`) calls the new scene's
  `on_scene_active()` *before* assigning `game.current_scene = scene`, so any
  `on_scene_active` that reads `current_scene` sees the *old* scene. Set the field
  first, then call `on_scene_active()`.
- **One transition function:** `set_current_adventure` and `switch_to_adventure`
  differ only by whether they run scene inactive/active callbacks, which is exactly
  why `main.c` needs the `set_active_scene(hub.entry_scene)` hack at startup. Merge
  them into a single `adventure_switch_to()` that always does old-scene-inactive →
  set new adventure/scene → adventure `on_enter` → new-scene-active. Removes the
  hack (supersedes the `set_active_scene` half of Task L5).
- **Bounds/NULL-safe router:** `scene_instance` (`game.c:58`) returns a `Scene` by
  value with no bounds check and dereferences `game.current_adventure` unconditionally.
  Return a `const Scene *`, assert `current_adventure != NULL` and
  `0 <= scene < scenes_count` (needs a scene count on `Adventure`), so a bad index or
  pre-init call fails loudly instead of corrupting/crashing.
- **Pass the hub explicitly** instead of inferring it as `registered[0]` in
  `register_adventures`, and either copy the registry array the engine keeps or
  document that the caller must keep it alive for the whole run (today it survives
  only because `main` passes a `static` array).

### Task O — Toddler-friendly hub button and touch input
*Code-only; UX for the target age, some overlap with iOS Task G.*

- The back-to-hub `HUB_BUTTON` is 40×40 logical px — below Apple's 44pt minimum and
  small for a 2-year-old. Bump to ≥64×64 with a comfortable margin (a
  `#define HUB_BUTTON_SIZE` rather than the inline literal).
- Ignore key auto-repeat on the debug toggle (`if (event->key.repeat) break;` in
  `game_process_input`) so holding **D** doesn't strobe debug mode at frame rate.
- Handle `SDL_FINGERDOWN`/touch explicitly for iPad (toddlers tap with fingers).
  Mouse events are synthesized from touch on web/mobile so it works today, but
  explicit touch (and ignoring the synthesized duplicate) is cleaner — pairs with
  the iOS sync (Task G) and the coordinate fix (Task M).

*Deferred from the same review:* rename `is_running`/`is_debugging` and move `Game`
into an `AppContext` — cosmetic, folded into the already-deferred `AppContext` note
under Task L; not worth a standalone change now.

### Task P — asset_path(): caller-owned buffer
*Code-only; latent footgun, from the `asset.c` review.*

`asset_path` returns a pointer into a single function-`static resolved_path[1024]`
(`asset.c:51`), so two live results clobber each other — e.g.
`load_two(asset_path(a), asset_path(b))` would resolve both to the second path.
Every call site today consumes the result immediately in one load
(`IMG_Load`/`Mix_LoadWAV`/`Mix_LoadMUS`/`load_animation_data`), so it's latent — but
with two adventures whose assets share filenames across different roots (`walking.wav`
in both, `pool/float.png` vs `tree/float.png`, many `background.png`), it's the
classic "second texture loads the wrong file" bug waiting to happen.

- Add a caller-owned variant — `bool asset_resolve(Asset, char *buf, size_t n)`
  returning whether the localized layer was used (vs the common fallback) — and let
  the caller own the storage. Keep `asset_path()` as a thin, documented wrapper for
  the existing single-use sites, or migrate them.
- While changing the API, document the lifetime contract of `asset_set_root` /
  `asset_set_locale`: they store the pointer, not a copy (`asset.c:23`/`27`), so the
  string must outlive the asset system. True today (string literals + `detect_locale`
  statics); either state the contract or `strdup`/free on set.

### Task Q — Portable, single-hit asset resolution
*Code-only; portability + diagnostics, from the `asset.c` review.*

- Replace the `access(F_OK)` pre-check (`asset.c:57`) with a real open probe
  (`SDL_RWFromFile`) — or, better, try-load-then-fallback (attempt the locale path,
  and only on failure try `common`). This drops the POSIX `<unistd.h>` dependency
  (Windows has no `access`), avoids the extra virtual-FS stat on Emscripten, and
  closes the TOCTOU window between the check and the real `IMG_Load`/`Mix_LoadWAV`.
- Check `snprintf` truncation in `build_path` (`asset.c:42`) instead of truncating
  silently, and name the `1024` buffer (`#define ASSET_PATH_MAX`) — Windows
  `PATH_MAX` is 260, Linux 4096.
- Log once when *neither* the locale nor the common file exists, so a typo'd filename
  surfaces here instead of as a cryptic downstream "IMG_Load: Couldn't open …".

### Task R — Per-actor walk-completion callback
*Code-only; real design bug, from the `actor.c` review.*

`on_end_walking` is a single file-`static` callback (`actor.c:17`) shared by every
`Actor`, set globally in `actor_walk_to` (`actor.c:195`). With the engine now generic
over multiple actors (fox + hen), a second `actor_walk_to` overwrites the first's
callback, so when actor A arrives it can fire actor B's `on_end`. Not triggered today
(only one actor is active at a time — one per adventure, one adventure active), but it
violates the multi-actor contract the abstraction promises.

- Move the callback (and an optional `void *userdata`) onto the `Actor`; have
  `actor_walk_to(actor, target, cb, userdata)` store it per-instance.
- Clear it *before* invoking it on arrival (`actor.c:131`) so a callback that starts a
  new walk isn't immediately clobbered.
- Same pattern lives in `image.c` (`on_animation_playback_end`, set by
  `play_animation`) — give animations a per-instance end callback too, or document why
  the global is safe.

### Task S — Actor audio safety (channels & lifetime)
*Code-only; includes two real crashes, from the `actor.c` review.*

- **Halt-all bug:** on arrival `actor_update` calls
  `Mix_HaltChannel(actor->move_sound_channel)` (`actor.c:130`), but that channel is
  `-1` when the actor has no move sound or when `Mix_PlayChannel` failed (e.g. all
  channels busy under click-spam) — and `Mix_HaltChannel(-1)` halts **every** channel,
  killing dialogue and SFX. Guard with `if (actor->move_sound_channel >= 0)`.
- **Use-after-free:** `actor_free` (`actor.c:183`) calls `Mix_FreeChunk(move_sound)`
  without halting its channel first — a use-after-free if it's still playing (notably
  on Emscripten's audio thread). Halt the channel, then free.
- **NULL dialog crash:** `actor_talk(actor, NULL)` (`actor.c:215`) flows into
  `get_chunk_time_ms`, which dereferences `chunk->alen` → crash. Guard `dialog != NULL`.
- **Voice channel tracking:** store the dialogue channel (`actor->voice_channel`) and
  play voice on the channel reserved by Task K, so a new line/click can interrupt the
  current one instead of overlapping or being dropped.
- **Tidy:** use `Mix_VolumeChunk(actor->move_sound, vol)` instead of the deprecated
  `move_sound->volume =` (`actor.c:102`; assert 0–128), and replace the stray
  `fprintf(stdout, "Sound duration: …")` (`actor.c:221`) with `SDL_Log` (or drop it).

### Task T — Actor state-machine hardening
*Code-only; small correctness/robustness nits, from the `actor.c` review.*

- **Divide-by-zero:** tapping exactly on the actor makes `dist == 0` in
  `actor_walk_to` (`actor.c:211`), so `direction` becomes NaN. It self-corrects next
  frame (the `close_enough` test catches `dx==dy==0`), but relying on NaN is fragile —
  guard `dist == 0` (skip the walk / zero the direction). Name the `2 px` arrival
  window `ACTOR_ARRIVE_EPSILON`, and use `dx*dx` instead of `powf(dx, 2)` for
  consistency with the overshoot test.
- **Time base:** `actor_update` reads `float ticks = SDL_GetTicks()` (`actor.c:111`)
  and compares against `Uint32` talk timing — use `Uint32` throughout so it stays
  precise and wraps cleanly (same fix family as Task L1).
- **Consistency:** `actor_play_state` sets `state` even when the animation is NULL
  (`actor.c:230`), unlike `actor_talk`/`actor_walk_to` which refuse when busy — pick
  one policy (e.g. return `bool` success).
- **Compiler help:** drop the `case ACTOR_STATE_COUNT` sentinel from the `actor_update`
  switch (`actor.c:117`) and build with `-Wswitch` so a newly added `ActorState` that
  isn't handled is flagged.
- **Allocation safety:** `make_actor` (`actor.c:47`) `malloc`s with no NULL check and
  no cleanup if a later animation load fails — add the check. And add a one-line
  comment that an actor's animations are per-instance (cloned in `make_actor`), so the
  next contributor knows `actor_face` flipping "all animations" won't turn both actors.

### Task U — Decouple animation update from render (+ per-animation frame rate)
*Code-only; from the `image.c` review.*

`render_animation` (`image.c:173`) advances the current frame, recomputes
`loop_count`, and calls `stop_animation` — which fires the end callback — all inside
the render path. So animation speed tracks *render* frequency (render twice → talk
plays twice as fast), and an end callback (which can switch scene/adventure) fires
mid-render.

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
  Task P); and consider a `play_animation` restart option (today a second
  `play_animation` on an already-playing animation is a silent no-op).

### Task V — Harden the `.anim` CSV parser
*Code-only; prevents overflow on malformed clip files, from the `image.c` review.*

`load_animation_data` (`image.c:95`) trusts its input completely:
- `num_i` is unbounded (`image.c:126`) — a 6+ digit field overflows the 6-byte `num`
  buffer (stack smash). Bound it.
- `rect_i` is never checked `< 4` (`image.c:114`) — a line with >4 fields writes past
  `rect[4]`.
- `row` is never checked `< animation->frames` (`image.c:117`) — more lines than the
  spec's frame count overflow the heap `sprite_clips[]`. (This check also enforces the
  `.anim` ↔ `ActorAnimSpec` frame-count contract — a real mismatch risk when editing a
  `.anim` without bumping `frames`.)
- A final line with no trailing newline is dropped (a rect is only committed on `\n`) —
  flush at EOF.
- CRLF leaves `\r` in the buffer (`atoi` tolerates it by luck); strip it. And `atoi`
  silently yields 0 on garbage.

Add the three bounds checks + EOF flush, or replace the hand-rolled scan with
`sscanf("%d,%d,%d,%d", …)` / `strtol` per line (handles whitespace and CRLF, no manual
buffer) — the files are tiny.

### Task W — Fix get_chunk_time_ms overflow and robustness
*Code-only; from the `sound.c` review.*

`get_chunk_time_ms` (`src/sound.c`) drives talk-animation length from the actual WAV
duration — exactly right for lip-syncing the Italian voiceover — but the final
`(frames * 1000) / freq` is all `Uint32`. For a long clip (e.g. ~89 s+ at 48 kHz),
`frames * 1000` exceeds 2³² and wraps, so the talk animation ends mid-narration. Today's
fox/hen lines are 2–3 s so it's latent, but a story scene with a longer narration would
hit it every time.

- Do the millisecond math in 64-bit: `(Uint64)frames * 1000 / freq`.
- Guard `chunk == NULL` (return 0) so a stray `actor_talk(actor, NULL)` can't deref
  `chunk->alen` (defense in depth; the call-site guard is Task S), and guard against
  `freq`/`chans` being 0.
- When `Mix_QuerySpec` fails (audio device not open) it returns 0, which makes the talk
  animation end instantly — `SDL_Log` it so that reads as "audio not open" instead of
  "talk animation is broken".

### Task X — Scene asset lifecycle: pass `Scene *`, unwind partial loads, null freed chunks
*Code-only; a leak + a dangling-pointer bug, from the `scene.c` review.*

`load_scene_images`/`load_scene_chunks`/`free_scene_images`/`free_scene_chunks`
(`scene.c:77`–`114`) all take `Scene` **by value**. It works only because `images`/
`chunks` are pointers (the writes go through the copied pointer), but it's misleading,
copies the whole struct per call, and means `free_scene_chunks` can't null the freed
`chunk` pointers — so they dangle (`scene.c:112`).

- Take `Scene *scene` in all four; after `Mix_FreeChunk(scene->chunks[i].chunk)` set
  `scene->chunks[i].chunk = NULL` (the image side already nulls the texture in
  `free_image_texture`).
- **Unwind partial loads:** if image/chunk *i* fails to load, items `0..i-1` stay
  loaded and the caller gets `false` with no handle to free them. Free what was loaded
  before returning `false` (or have the caller call the matching `free_scene_*`).
- Minor pathing hardening in `nearest_walkable_point` (`scene.c:21`): the `long`
  distance math is fine at 800×600 but use a fixed-width `int64_t` (or clamp) to keep
  static analyzers quiet; and document that `non_walkable` is a *single* exclusion
  zone — if a scene (e.g. Gina's pool) needs more, take `const SDL_Rect *blocked, int
  count` (a stepping stone toward the full pathfinding in Task F).

## Suggested sequencing

1. **G** — required to keep the iOS/Mac build green after the Step-1 refactor.
2. **M** — fix the logical-coordinate input bug first; it's a live bug on
   resized/Retina/web windows and underpins every hit-test (incl. `actor_walk_to`
   targets).
3. **S** — actor audio safety: two real crashes (halt-all-channels, free-while-playing)
   plus the NULL-dialog guard; pairs with **K**.
4. **K** — reserve a dialogue channel; small, high-value (stops SFX cutting voice
   lines), no assets needed.
5. **R** — per-actor walk callback; do with the engine-correctness cluster before a
   second on-screen actor makes the shared callback bite.
6. **U** — split animation update from render (fixes render-rate-dependent speed and
   callbacks firing mid-render); engine correctness, pairs with **R**.
7. **N** — unify the adventure/scene transitions (fixes the activation-order bug and
   drops the main.c hack); pairs well with **L**.
8. **P** — give `asset_path` a caller-owned buffer before the asset set grows and a
   shared filename across two roots causes a silent wrong-asset load.
9. **X** — pass `Scene *` + unwind partial scene loads + null freed chunks; fixes a
   leak and dangling pointers, easy alongside **N**.
10. **C** — biggest remaining UX win; write C1 + the state machine, fill in lines
   as the `.wav`s arrive.
11. **E** — engine wiring can be written behind the new states; lands with the art.
12. **D** — optional polish.
13. **L** — main.c lifecycle hardening; low-risk, land anytime (good alongside other
   main.c work).
14. **T** — actor state-machine hardening (divide-by-zero, `Uint32` time, `-Wswitch`,
   malloc check); low-risk, bundle with **L**/**Q**.
15. **V** — harden the `.anim` CSV parser (overflow guards + EOF flush); cheap, bundle
   with the **T** hardening pass.
16. **W** — 64-bit fix for `get_chunk_time_ms` (talk duration wraps past ~89 s);
   tiny, do with the **T**/**V** hardening pass.
17. **Q** — portable single-hit asset resolution; do with **L** or before a
   Windows/iOS port (drops `access()`/`<unistd.h>`, adds missing-asset logging).
18. **O** — toddler input polish (bigger button, key-repeat, touch); cheap, do
   alongside **M**/**G**.
19. **F** — only if the walk-through becomes a real problem.
20. **H** — the hub + a second adventure, when ready to grow the collection.
21. **I** — lazy-load adventures once the collection is large enough that loading
   everything up front is a felt cost (memory/startup, or web download size).
22. **J** — i18n follow-ups (language picker, iOS bundling, per-locale web bundles,
   real translations) as more languages are actually shipped.

## Asset checklist (blocks C2 and E; to be provided)

- Italian voice `.wav` per hint state (Task C2) → `playground*/dialog/`.
- `fox/climbing.png` + `climbing.anim`, `fox/sliding.png` + `sliding.anim` (Task E).

## Shipped

- **A** — enlarged the shovel hotspot (#5).
- **B** — clicks outside the walkable area now walk the fox to the nearest
  reachable point instead of being ignored (#5).
- Web build target + GitHub Pages deployment; custom minimal HTML shell; iOS
  Safari audio fix.
- Multi-adventure foundation (#10, Rollout Step 1): engine/content split via an
  `Adventure` registry, a generic `Actor` (the fox is now a spec), and the
  `vania_fox_the_slide` adventure module. Follow-ups: Task G, Task H.
