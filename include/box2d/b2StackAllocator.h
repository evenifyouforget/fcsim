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

#ifndef B2_STACK_ALLOCATOR_H
#define B2_STACK_ALLOCATOR_H

#include <box2d/b2Settings.h>

#define b2_stackSize 100 * 1024 // 100k
#define b2_maxStackEntries 32

typedef struct b2StackEntry b2StackEntry;
struct b2StackEntry {
  char *data;
  int32 size;
  bool usedMalloc;
};

// This is a stack allocator used for fast per step allocations.
// You must nest allocate/free pairs. The code will assert
// if you try to interleave multiple allocate/free pairs.
typedef struct b2StackAllocator b2StackAllocator;
struct b2StackAllocator {
  char m_data[b2_stackSize];
  int32 m_index;

  int32 m_allocation;

  struct b2StackEntry m_entries[b2_maxStackEntries];
  int32 m_entryCount;
};

#ifdef __cplusplus
extern "C" {
#endif

void b2StackAllocator_ctor(b2StackAllocator *allocator);

void *b2StackAllocator_Allocate(struct b2StackAllocator *allocator, int32 size);

void b2StackAllocator_Free(struct b2StackAllocator *allocator, void *p);

#ifdef __cplusplus
}
#endif

#endif
