//
//  image.h
//  sdlexample
//
//  Created by Giovanni Cappellotto on 1/15/25.
//

#ifndef image_h
#define image_h

void free_image_texture(SDL_Texture **texture, int *width, int *height);

bool load_from_file(const char *path, SDL_Renderer *renderer,
                    SDL_Texture **texture, int *width, int *height);

#endif /* image_h */
