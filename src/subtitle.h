//
//  subtitle.h
//  On-screen dialogue text with a karaoke-style read-along highlight
//  (SPEECH.md Part 3): while a line is spoken its text is shown at the bottom
//  of the screen, and the currently spoken word — from the line's .words
//  sidecar — is highlighted so a child can follow along.
//
//  Subtitles are on by default (they are an accessibility and learn-to-read
//  feature, not only a fallback) and can be disabled with --subtitles=0 /
//  $VANIA_SUBTITLES=0 / web ?subtitles=0. Lines with no audio always show.
//  The overlay is screen-space UI, drawn after any camera offset is reset.
//

#ifndef subtitle_h
#define subtitle_h

#include <SDL2/SDL.h>
#include <stdbool.h>

#include "lipsync.h"

// Read the setting (argv flag, then $VANIA_SUBTITLES; default on), init
// SDL2_ttf and load the bundled font. A missing font logs an error and
// leaves subtitles off; the game keeps running.
bool subtitle_init(int argc, char **argv, SDL_Renderer *renderer);

// Show `text` for duration_ms (+ a short grace), replacing any current line.
// `words` may be NULL/empty — the text shows without a highlight. `force`
// shows the line even when subtitles are disabled (lines with no audio).
void subtitle_show(const char *text, const WordTimings *words,
                   Uint32 duration_ms, bool force);

void subtitle_clear(void);

// Draw the active line (no-op when nothing is active or it expired).
void subtitle_render(SDL_Renderer *renderer);

void subtitle_deinit(void);

// The global setting (true unless disabled via flag/env).
bool subtitles_enabled(void);

#endif /* subtitle_h */
