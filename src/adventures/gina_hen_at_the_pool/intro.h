//
//  intro.h
//  Gina's title screen.
//

#ifndef gina_intro_h
#define gina_intro_h

#include "scene.h"

// Prefixed because scene objects are global symbols shared by every adventure,
// and Vania's title screen already owns `intro_scene`.
extern Scene gina_intro_scene;

#endif /* gina_intro_h */
