//
//  sound.h
//  sdlexample
//
//  Created by Giovanni Cappellotto on 1/22/25.
//

#ifndef sound_h
#define sound_h

#import <SDL2_mixer/SDL_mixer.h>

#include <stdbool.h>

#include "lipsync.h"

typedef struct chunk_data {
  Mix_Chunk *chunk;
  const char *filename;
  const char *directory;
  // The WAV is optional: a not-yet-recorded dialogue line (SCENES.md
  // milestone 4) has its text but no audio, so a missing file loads as a
  // text-only line (chunk == NULL) instead of failing. Plain SFX/music leave
  // this false and a missing WAV is still a hard error.
  bool optional_audio;
  // Dialogue sidecars (see SPEECH.md), loaded next to the WAV by
  // load_scene_chunks; absent for plain SFX chunks. `text` is the spoken
  // line from <name>.txt; `cues`/`words` drive the talking animation and
  // the read-along highlight.
  char *text;
  MouthCues cues;
  WordTimings words;
} ChunkData;

Uint32 get_chunk_time_ms(Mix_Chunk *chunk);

#endif /* sound_h */
