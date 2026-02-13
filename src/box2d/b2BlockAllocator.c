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

#include <box2d/b2BlockAllocator.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

int32 b2BlockAllocator_s_blockSizes[b2_blockSizes] = {
    16,  // 0
    32,  // 1
    64,  // 2
    96,  // 3
    128, // 4
    160, // 5
    192, // 6
    224, // 7
    256, // 8
    320, // 9
    384, // 10
    448, // 11
    512, // 12
    640, // 13
};
uint8 b2BlockAllocator_s_blockSizeLookup[b2_maxBlockSize + 1];
bool b2BlockAllocator_s_blockSizeLookupInitialized;

struct b2Chunk {
  int32 blockSize;
  b2Block *blocks;
};

struct b2Block {
  b2Block *next;
};

void b2BlockAllocator_ctor(b2BlockAllocator *allocator) {
  allocator->m_chunkSpace = b2_chunkArrayIncrement;
  allocator->m_chunkCount = 0;
  allocator->m_chunks =
      (b2Chunk *)b2Alloc(allocator->m_chunkSpace * sizeof(b2Chunk));

  memset(allocator->m_chunks, 0, allocator->m_chunkSpace * sizeof(b2Chunk));
  memset(allocator->m_freeLists, 0, sizeof(allocator->m_freeLists));

  if (b2BlockAllocator_s_blockSizeLookupInitialized == false) {
    int32 j = 0;
    for (int32 i = 1; i <= b2_maxBlockSize; ++i) {
      if (i <= b2BlockAllocator_s_blockSizes[j]) {
        b2BlockAllocator_s_blockSizeLookup[i] = (uint8)j;
      } else {
        ++j;
        b2BlockAllocator_s_blockSizeLookup[i] = (uint8)j;
      }
    }

    b2BlockAllocator_s_blockSizeLookupInitialized = true;
  }
}

void b2BlockAllocator_dtor(b2BlockAllocator *allocator) {
  for (int32 i = 0; i < allocator->m_chunkCount; ++i) {
    b2Free(allocator->m_chunks[i].blocks);
  }

  b2Free(allocator->m_chunks);
}

void *b2BlockAllocator_Allocate(b2BlockAllocator *allocator, int32 size) {
  if (size == 0)
    return NULL;

  int32 index = b2BlockAllocator_s_blockSizeLookup[size];

  if (allocator->m_freeLists[index]) {
    b2Block *block = allocator->m_freeLists[index];
    allocator->m_freeLists[index] = block->next;
    return block;
  } else {
    if (allocator->m_chunkCount == allocator->m_chunkSpace) {
      b2Chunk *oldChunks = allocator->m_chunks;
      allocator->m_chunkSpace += b2_chunkArrayIncrement;
      allocator->m_chunks =
          (b2Chunk *)b2Alloc(allocator->m_chunkSpace * sizeof(b2Chunk));
      memcpy(allocator->m_chunks, oldChunks,
             allocator->m_chunkCount * sizeof(b2Chunk));
      memset(allocator->m_chunks + allocator->m_chunkCount, 0,
             b2_chunkArrayIncrement * sizeof(b2Chunk));
      b2Free(oldChunks);
    }

    b2Chunk *chunk = allocator->m_chunks + allocator->m_chunkCount;
    chunk->blocks = (b2Block *)b2Alloc(b2_chunkSize);
#if defined(_DEBUG)
    memset(chunk->blocks, 0xcd, b2_chunkSize);
#endif
    int32 blockSize = b2BlockAllocator_s_blockSizes[index];
    chunk->blockSize = blockSize;
    int32 blockCount = b2_chunkSize / blockSize;
    for (int32 i = 0; i < blockCount - 1; ++i) {
      b2Block *block = (b2Block *)((int8 *)chunk->blocks + blockSize * i);
      b2Block *next = (b2Block *)((int8 *)chunk->blocks + blockSize * (i + 1));
      block->next = next;
    }
    b2Block *last =
        (b2Block *)((int8 *)chunk->blocks + blockSize * (blockCount - 1));
    last->next = NULL;

    allocator->m_freeLists[index] = chunk->blocks->next;
    ++allocator->m_chunkCount;

    return chunk->blocks;
  }
}

void b2BlockAllocator_Free(b2BlockAllocator *allocator, void *p, int32 size) {
  if (size == 0) {
    return;
  }

  int32 index = b2BlockAllocator_s_blockSizeLookup[size];

  b2Block *block = (b2Block *)p;
  block->next = allocator->m_freeLists[index];
  allocator->m_freeLists[index] = block;
}
