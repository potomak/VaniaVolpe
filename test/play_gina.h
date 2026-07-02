// Scripted play-test for "Gina la Gallina in Piscina".

#ifndef test_play_gina_h
#define test_play_gina_h

// Play the full Gina adventure via scripted input and assert the expected
// dialogue appeared (and that a real frame rendered). Assumes the harness has
// started the game and left it at the hub. Returns the number of failed checks
// (0 = pass).
int play_gina(void);

#endif /* test_play_gina_h */
