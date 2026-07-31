// Scripted play-test for "Vania Volpe - Lo Scivolo".

#ifndef test_play_vania_h
#define test_play_vania_h

// Play the full Vania adventure via scripted input and assert the expected
// dialogue appeared. Assumes the harness has started the game and left it at
// the hub — and leaves it back there, since the adventure ends by returning to
// the menu. Returns the number of failed checks (0 = pass).
int play_vania(void);

#endif /* test_play_vania_h */
