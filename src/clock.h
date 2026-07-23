//
//  clock.h
//  Simulation clock (see #155). The engine reads "now" through clock_now_ms()
//  instead of calling SDL_GetTicks() directly, so a test can drive the whole
//  simulation off a deterministic virtual clock rather than the wall clock.
//
//  In the normal (real) mode clock_now_ms() *is* SDL_GetTicks(), so production
//  behaviour is unchanged. The headless test switches to virtual mode and
//  advances the clock a fixed step per frame (test/harness.c), making the
//  scripted playthrough reproducible regardless of machine speed or load —
//  which is what removes the timing flakiness.
//

#ifndef clock_h
#define clock_h

#include <SDL2/SDL.h>
#include <stdbool.h>

// Current simulation time in milliseconds: SDL_GetTicks() in real mode, the
// virtual counter in virtual mode.
Uint32 clock_now_ms(void);

// Switch to virtual mode (reset to 0) or back to real mode. Tests call this;
// production never does, so clock_now_ms() stays SDL_GetTicks().
void clock_set_virtual(bool on);

// Advance the virtual clock by ms (a no-op in real mode). The test frame step
// calls this once per frame with its fixed timestep.
void clock_advance(Uint32 ms);

// True while in virtual mode — the harness uses a fixed timestep then.
bool clock_is_virtual(void);

#endif /* clock_h */
