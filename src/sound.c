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
  if (chunk == NULL) {
    return 0;
  }

  Uint32 points = 0;
  Uint32 frames = 0;
  int freq = 0;
  Uint16 fmt = 0;
  int chans = 0;
  // Chunks are converted to audio device format...
  if (!Mix_QuerySpec(&freq, &fmt, &chans)) {
    // Audio device not open (e.g. Mix_OpenAudio failed). Log it so a zero
    // duration reads as "no audio" rather than "talk animation is broken".
    SDL_Log("get_chunk_time_ms: Mix_QuerySpec failed: %s", Mix_GetError());
    return 0;
  }

  int sample_bytes = (fmt & 0xFF) / 8;
  if (sample_bytes <= 0 || chans <= 0 || freq <= 0) {
    return 0;
  }

  /* bytes / samplesize == sample points */
  points = (chunk->alen / sample_bytes);

  /* sample points / channels == sample frames */
  frames = (points / chans);

  /* (sample frames * 1000) / frequency == play length in ms.
   * Use 64-bit math: frames * 1000 overflows Uint32 past ~89 s at 48 kHz. */
  return (Uint32)((Uint64)frames * 1000 / freq);
}
