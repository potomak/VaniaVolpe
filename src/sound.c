//
//  sound.c
//  sdlexample
//
//  Created by Giovanni Cappellotto on 1/22/25.
//

#include <SDL2/SDL.h>
#include <SDL2_mixer/SDL_mixer.h>

#include "sound.h"

// Compute the length of a chunk in milliseconds
//
// Source:
// https://discourse.libsdl.org/t/time-length-of-sdl-mixer-chunks/12852/2
Uint32 get_chunk_time_ms(Mix_Chunk *chunk) {
  Uint32 points = 0;
  Uint32 frames = 0;
  int freq = 0;
  Uint16 fmt = 0;
  int chans = 0;
  // Chunks are converted to audio device format...
  if (!Mix_QuerySpec(&freq, &fmt, &chans)) {
    // never called Mix_OpenAudio()?!
    return 0;
  }

  /* bytes / samplesize == sample points */
  points = (chunk->alen / ((fmt & 0xFF) / 8));

  /* sample points / channels == sample frames */
  frames = (points / chans);

  /* (sample frames * 1000) / frequency == play length in ms */
  return (frames * 1000) / freq;
}
