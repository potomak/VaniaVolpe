#ifndef terminal_h
#define terminal_h

#include <SDL2/SDL.h>
#include <stdbool.h>

// Initialize the libcaca display and dither object.
// Must be called after SDL_Init and after the renderer is created.
// Returns false if libcaca could not open a display.
bool terminal_init(SDL_Renderer *renderer);

// Read pixels from the SDL renderer and display them via libcaca.
// Call this instead of SDL_RenderPresent each frame.
void terminal_present(SDL_Renderer *renderer);

// Drain all pending libcaca events and inject equivalent SDL events into
// the SDL event queue via SDL_PushEvent. Call this before SDL_PollEvent.
void terminal_poll_events(void);

// Free all libcaca resources.
void terminal_deinit(void);

#endif /* terminal_h */
