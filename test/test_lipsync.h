//
//  test_lipsync.h
//  Unit tests for the .cues / .words sidecar parsers and playback lookups
//  (src/lipsync.c). Pure functions — no window, renderer, or assets.
//

#ifndef test_lipsync_h
#define test_lipsync_h

// Runs every check; returns the number of failures (0 = all passed).
int test_lipsync(void);

#endif /* test_lipsync_h */
