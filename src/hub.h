//
//  hub.h
//  A minimal adventure-selection menu, modelled as a one-scene Adventure. It is
//  registered as the first adventure and the app starts on it.
//

#ifndef hub_h
#define hub_h

#include "adventure.h"

extern Adventure hub;

// Provide the list of selectable (content) adventures the menu shows. Call once
// before register_adventures().
void hub_register(const Adventure **content, int count);

#endif /* hub_h */
