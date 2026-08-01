//
//  confirm.h
//  A modal yes/no overlay, for the one question the engine asks: are you sure
//  you want to leave?
//
//  The back-to-hub button sits in a corner an adventure never uses, which is
//  also a corner a toddler's palm finds by accident — and leaving throws away
//  whatever they had solved. So the button opens this instead of leaving.
//
//  Deliberately wordless: the audience cannot read. A tick and a cross, both
//  large and well apart. The art is engine-owned (assets/ui), shared by every
//  adventure and listed in the repo-level manifest like any other asset still
//  to be drawn.
//

#ifndef confirm_h
#define confirm_h

#include <SDL2/SDL.h>
#include <stdbool.h>

// Load and free the button art. Called once around the game's lifetime, beside
// the subtitle font's — this is engine UI, so it outlives any one adventure.
bool confirm_load_media(SDL_Renderer *renderer);
void confirm_free_media(void);

// Ask, and call `on_confirm` if the answer is yes. Opening while already open
// replaces the pending question, which cannot happen today (only the hub
// button asks) but keeps the state single-valued.
void confirm_open(void (*on_confirm)(void));

// Dismiss without answering. Called when the ground shifts under the question
// — an adventure change means whatever it was about is gone.
void confirm_close(void);

bool confirm_is_open(void);

// Handle one event while open. Returns true when the overlay consumed it,
// which is every mouse event: a modal that let taps through to the scene
// underneath would not be modal. Answers on release, like every other tap
// (see ARCHITECTURE.md).
bool confirm_process_input(const SDL_Event *event);

// Draw the overlay, in screen space, over everything else.
void confirm_render(SDL_Renderer *renderer);

#endif /* confirm_h */
