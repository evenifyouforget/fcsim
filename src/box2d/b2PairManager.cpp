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

#include <box2d/b2BroadPhase.h>
#include <box2d/b2PairManager.h>

// Thomas Wang's hash, see: http://www.concentric.net/~Ttwang/tech/inthash.htm
// This assumes proxyId1 and proxyId2 are 16-bit.
inline uint32 Hash(uint32 proxyId1, uint32 proxyId2) {
  uint32 key = (proxyId2 << 16) | proxyId1;
  key = ~key + (key << 15);
  key = key ^ (key >> 12);
  key = key + (key << 2);
  key = key ^ (key >> 4);
  key = key * 2057;
  key = key ^ (key >> 16);
  return key;
}

inline bool Equals(const b2Pair &pair, int32 proxyId1, int32 proxyId2) {
  return pair.proxyId1 == proxyId1 && pair.proxyId2 == proxyId2;
}

void b2PairManager_ctor(b2PairManager *manager) {
  for (int32 i = 0; i < b2_tableCapacity; ++i) {
    manager->m_hashTable[i] = b2_nullPair;
  }
  manager->m_freePair = 0;
  for (int32 i = 0; i < b2_maxPairs; ++i) {
    manager->m_pairs[i].proxyId1 = b2_nullProxy;
    manager->m_pairs[i].proxyId2 = b2_nullProxy;
    manager->m_pairs[i].userData = NULL;
    manager->m_pairs[i].status = 0;
    manager->m_pairs[i].next = uint16(i + 1);
  }
  manager->m_pairs[b2_maxPairs - 1].next = b2_nullPair;
  manager->m_pairCount = 0;

  manager->m_pairBufferCount = 0;
}

void b2PairManager_Initialize(b2PairManager *manager, b2BroadPhase *broadPhase,
                              b2PairCallback *callback) {
  manager->m_broadPhase = broadPhase;
  manager->m_callback = callback;
}

static b2Pair *b2PairManager_FindHash(b2PairManager *manager, int32 proxyId1,
                                      int32 proxyId2, uint32 hash) {
  int32 index = manager->m_hashTable[hash];

  while (index != b2_nullPair &&
         Equals(manager->m_pairs[index], proxyId1, proxyId2) == false) {
    index = manager->m_pairs[index].next;
  }

  if (index == b2_nullPair) {
    return NULL;
  }

  return manager->m_pairs + index;
}

static b2Pair *b2PairManager_Find(b2PairManager *manager, int32 proxyId1,
                                  int32 proxyId2) {
  if (proxyId1 > proxyId2)
    b2Swap(proxyId1, proxyId2);

  int32 hash = Hash(proxyId1, proxyId2) & b2_tableMask;

  return b2PairManager_FindHash(manager, proxyId1, proxyId2, hash);
}

// Returns existing pair or creates a new one.
static b2Pair *b2PairManager_AddPair(b2PairManager *manager, int32 proxyId1,
                                     int32 proxyId2) {
  if (proxyId1 > proxyId2)
    b2Swap(proxyId1, proxyId2);

  int32 hash = Hash(proxyId1, proxyId2) & b2_tableMask;

  b2Pair *pair = b2PairManager_FindHash(manager, proxyId1, proxyId2, hash);
  if (pair != NULL) {
    return pair;
  }

  uint16 pairIndex = manager->m_freePair;
  pair = manager->m_pairs + pairIndex;
  manager->m_freePair = pair->next;

  pair->proxyId1 = (uint16)proxyId1;
  pair->proxyId2 = (uint16)proxyId2;
  pair->status = 0;
  pair->userData = NULL;
  pair->next = manager->m_hashTable[hash];

  manager->m_hashTable[hash] = pairIndex;

  ++manager->m_pairCount;

  return pair;
}

// Removes a pair. The pair must exist.
static void *b2PairManager_RemovePair(b2PairManager *manager, int32 proxyId1,
                                      int32 proxyId2) {
  if (proxyId1 > proxyId2)
    b2Swap(proxyId1, proxyId2);

  int32 hash = Hash(proxyId1, proxyId2) & b2_tableMask;

  uint16 *node = &manager->m_hashTable[hash];
  while (*node != b2_nullPair) {
    if (Equals(manager->m_pairs[*node], proxyId1, proxyId2)) {
      uint16 index = *node;
      *node = manager->m_pairs[*node].next;

      b2Pair *pair = manager->m_pairs + index;
      void *userData = pair->userData;

      // Scrub
      pair->next = manager->m_freePair;
      pair->proxyId1 = b2_nullProxy;
      pair->proxyId2 = b2_nullProxy;
      pair->userData = NULL;
      pair->status = 0;

      manager->m_freePair = index;
      --manager->m_pairCount;
      return userData;
    } else {
      node = &manager->m_pairs[*node].next;
    }
  }

  return NULL;
}

/*
As proxies are created and moved, many pairs are created and destroyed. Even
worse, the same pair may be added and removed multiple times in a single time
step of the physics engine. To reduce traffic in the pair manager, we try to
avoid destroying pairs in the pair manager until the end of the physics step.
This is done by buffering all the RemovePair requests. AddPair requests are
processed immediately because we need the hash table entry for quick lookup.

All user user callbacks are delayed until the buffered pairs are confirmed in
Commit. This is very important because the user callbacks may be very expensive
and client logic may be harmed if pairs are added and removed within the same
time step.

Buffer a pair for addition.
We may add a pair that is not in the pair manager or pair buffer.
We may add a pair that is already in the pair manager and pair buffer.
If the added pair is not a new pair, then it must be in the pair buffer (because
RemovePair was called).
*/
void b2PairManager_AddBufferedPair(b2PairManager *manager, int32 id1,
                                   int32 id2) {
  b2Pair *pair = b2PairManager_AddPair(manager, id1, id2);

  // If this pair is not in the pair buffer ...
  if (b2Pair_IsBuffered(pair) == false) {
    // Add it to the pair buffer.
    b2Pair_SetBuffered(pair);
    manager->m_pairBuffer[manager->m_pairBufferCount].proxyId1 = pair->proxyId1;
    manager->m_pairBuffer[manager->m_pairBufferCount].proxyId2 = pair->proxyId2;
    ++manager->m_pairBufferCount;
  }

  // Confirm this pair for the subsequent call to Commit.
  b2Pair_ClearRemoved(pair);
}

// Buffer a pair for removal.
void b2PairManager_RemoveBufferedPair(b2PairManager *manager, int32 id1,
                                      int32 id2) {
  b2Pair *pair = b2PairManager_Find(manager, id1, id2);

  if (pair == NULL) {
    // The pair never existed. This is legal (due to collision filtering).
    return;
  }

  // If this pair is not in the pair buffer ...
  if (b2Pair_IsBuffered(pair) == false) {
    b2Pair_SetBuffered(pair);
    manager->m_pairBuffer[manager->m_pairBufferCount].proxyId1 = pair->proxyId1;
    manager->m_pairBuffer[manager->m_pairBufferCount].proxyId2 = pair->proxyId2;
    ++manager->m_pairBufferCount;
  }

  b2Pair_SetRemoved(pair);
}

void b2PairManager_Commit(b2PairManager *manager) {
  int32 removeCount = 0;

  b2Proxy *proxies = manager->m_broadPhase->m_proxyPool;

  for (int32 i = 0; i < manager->m_pairBufferCount; ++i) {
    b2Pair *pair =
        b2PairManager_Find(manager, manager->m_pairBuffer[i].proxyId1,
                           manager->m_pairBuffer[i].proxyId2);
    b2Pair_ClearBuffered(pair);

    b2Proxy *proxy1 = proxies + pair->proxyId1;
    b2Proxy *proxy2 = proxies + pair->proxyId2;

    if (b2Pair_IsRemoved(pair)) {
      // It is possible a pair was added then removed before a commit.
      // Therefore, we should be careful not to tell the user the pair was
      // removed when the the user didn't receive a matching add.
      if (b2Pair_IsFinal(pair) == true) {
        manager->m_callback->PairRemoved(manager->m_callback, proxy1->userData,
                                         proxy2->userData, pair->userData);
      }

      // Store the ids so we can actually remove the pair below.
      manager->m_pairBuffer[removeCount].proxyId1 = pair->proxyId1;
      manager->m_pairBuffer[removeCount].proxyId2 = pair->proxyId2;
      ++removeCount;
    } else {
      if (b2Pair_IsFinal(pair) == false) {
        pair->userData = manager->m_callback->PairAdded(
            manager->m_callback, proxy1->userData, proxy2->userData);
        b2Pair_SetFinal(pair);
      }
    }
  }

  for (int32 i = 0; i < removeCount; ++i) {
    b2PairManager_RemovePair(manager, manager->m_pairBuffer[i].proxyId1,
                             manager->m_pairBuffer[i].proxyId2);
  }

  manager->m_pairBufferCount = 0;
}
