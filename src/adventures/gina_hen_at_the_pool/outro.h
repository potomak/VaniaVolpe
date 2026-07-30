//
//  outro.h
//  Gina's end card.
//

#ifndef gina_outro_h
#define gina_outro_h

#include "scene.h"

// Prefixed because scene objects are global symbols shared by every adventure,
// and Vania's end card already owns `outro_scene`.
extern Scene gina_outro_scene;

#endif /* gina_outro_h */
