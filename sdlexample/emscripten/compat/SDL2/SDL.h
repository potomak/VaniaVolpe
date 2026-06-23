// Emscripten compatibility shim: the sources include <SDL2/SDL.h>, but the
// Emscripten SDL2 port exposes the unprefixed header.
#include <SDL.h>
