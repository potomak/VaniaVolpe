//
//  debug.h
//  sdlexample
//
//  Created by Giovanni Cappellotto on 1/16/25.
//

#ifndef debug_h
#define debug_h

#include <SDL2/SDL.h>
#include <stdbool.h>

// Handle a debug-layer event. Returns true when the event was consumed and
// must not reach the scene (walk-paint mode swallows all mouse input so
// painting doesn't walk the actor; see TOOLS.md).
bool debug_process_input(SDL_Event *event);

void debug_render(SDL_Renderer *renderer);

#endif /* debug_h */
