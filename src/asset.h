//
//  asset.h
//  sdlexample
//
//  Created by Giovanni Cappellotto on 1/26/25.
//

#ifndef asset_h
#define asset_h

typedef struct asset {
  const char *filename;
  const char *directory;
} Asset;

const char *asset_path(Asset asset);

#endif /* asset_h */
