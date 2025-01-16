//
//  intro.h
//  sdlexample
//
//  Created by Giovanni Cappellotto on 1/15/25.
//

#ifndef intro_h
#define intro_h

void intro_init(void);

bool intro_load_media(SDL_Renderer *renderer);

void intro_process_input(void);

void intro_update(void);

void intro_render(SDL_Renderer *renderer);

void intro_deinit(void);

#endif /* intro_h */
