//
//  image.h
//  sdlexample
//
//  Created by Giovanni Cappellotto on 1/15/25.
//

#ifndef image_h
#define image_h

typedef struct image_data {
  SDL_Texture *texture;
  int width;
  int height;
} ImageData;

void free_image_texture(ImageData *image);

bool load_from_file(const char *path, SDL_Renderer *renderer, ImageData *image);

#endif /* image_h */
