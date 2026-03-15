#ifndef GEN_H
#define GEN_H

#include "graph.h"
#include <box2d/b2World.h>

#ifdef __cplusplus
extern "C" {
#endif

void gen_block(b2World *world, struct block *block);

#ifdef __cplusplus
}
#endif

#endif
