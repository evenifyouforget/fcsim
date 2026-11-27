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

#include <box2d/b2StackAllocator.h>
#include <stddef.h>

void b2StackAllocator_ctor(struct b2StackAllocator *allocator) {
  allocator->m_index = 0;
  allocator->m_allocation = 0;
  allocator->m_entryCount = 0;
}

void *b2StackAllocator_Allocate(struct b2StackAllocator *allocator,
                                int32 size) {
  b2StackEntry *entry = allocator->m_entries + allocator->m_entryCount;
  entry->size = size;
  if (allocator->m_index + size > b2_stackSize) {
    entry->data = (char *)b2Alloc(size);
    entry->usedMalloc = true;
  } else {
    entry->data = allocator->m_data + allocator->m_index;
    entry->usedMalloc = false;
    allocator->m_index += size;
  }

  allocator->m_allocation += size;
  ++allocator->m_entryCount;

  return entry->data;
}

void b2StackAllocator_Free(struct b2StackAllocator *allocator, void *p) {
  b2StackEntry *entry = allocator->m_entries + allocator->m_entryCount - 1;
  if (entry->usedMalloc) {
    b2Free(p);
  } else {
    allocator->m_index -= entry->size;
  }
  allocator->m_allocation -= entry->size;
  --allocator->m_entryCount;

  p = NULL;
}
