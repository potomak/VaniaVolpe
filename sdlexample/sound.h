//
//  sound.h
//  sdlexample
//
//  Created by Giovanni Cappellotto on 1/22/25.
//

#ifndef sound_h
#define sound_h

typedef struct chunk_data {
  Mix_Chunk *chunk;
  const char *path;
} ChunkData;

Uint32 get_chunk_time_ms(Mix_Chunk *chunk);

#endif /* sound_h */
