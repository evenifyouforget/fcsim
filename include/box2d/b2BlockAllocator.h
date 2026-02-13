/*
 * Copyright (c) 2006-2007 Erin Catto http://www.gphysics.com
 *
 * This software is provided 'as-is', without any express or implied
 * warranty.  In no event will the authors be held liable for any damages
 * arising from the use of this software.
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 * 1. The origin of this software must not be misrepresented; you must not
 * claim that you wrote the original software. If you use this software
 * in a product, an acknowledgment in the product documentation would be
 * appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 * misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 */

#ifndef B2_BLOCK_ALLOCATOR_H
#define B2_BLOCK_ALLOCATOR_H

#include <box2d/b2Settings.h>

#define b2_chunkSize 4096
#define b2_maxBlockSize 640
#define b2_blockSizes 14
#define b2_chunkArrayIncrement 128

typedef struct b2Block b2Block;
struct b2Block;
typedef struct b2Chunk b2Chunk;
struct b2Chunk;

// This is a small block allocator used for allocating small
// objects that persist for more than one time step.
// See: http://www.codeproject.com/useritems/Small_Block_Allocator.asp
typedef struct b2BlockAllocator b2BlockAllocator;
struct b2BlockAllocator {
  b2Chunk *m_chunks;
  int32 m_chunkCount;
  int32 m_chunkSpace;

  b2Block *m_freeLists[b2_blockSizes];
};

#ifdef __cplusplus
extern "C" {
#endif

void b2BlockAllocator_ctor(b2BlockAllocator *allocator);

void b2BlockAllocator_dtor(b2BlockAllocator *allocator);

void *b2BlockAllocator_Allocate(b2BlockAllocator *allocator, int32 size);

void b2BlockAllocator_Free(b2BlockAllocator *allocator, void *p, int32 size);

extern int32 b2BlockAllocator_s_blockSizes[b2_blockSizes];
extern uint8 b2BlockAllocator_s_blockSizeLookup[b2_maxBlockSize + 1];
extern bool b2BlockAllocator_s_blockSizeLookupInitialized;

#ifdef __cplusplus
}
#endif

#endif
