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
#include <box2d/b2Math.h>
#include <string.h>

// Notes:
// - we use bound arrays instead of linked lists for cache coherence.
// - we use quantized integral values for fast compares.
// - we use short indices rather than pointers to save memory.
// - we use a stabbing count for fast overlap queries (less than order N).
// - we also use a time stamp on each proxy to speed up the registration of
//   overlap query results.
// - where possible, we compare bound indices instead of values to reduce
//   cache misses (TODO_ERIN).
// - no broadphase is perfect and neither is this one: it is not great for huge
//   worlds (use a multi-SAP instead), it is not great for large objects.

struct b2BoundValues {
  uint16 lowerValues[2];
  uint16 upperValues[2];
};

static int32 BinarySearch(b2Bound *bounds, int32 count, uint16 value) {
  int32 low = 0;
  int32 high = count - 1;
  while (low <= high) {
    int32 mid = (low + high) >> 1;
    if (bounds[mid].value > value) {
      high = mid - 1;
    } else if (bounds[mid].value < value) {
      low = mid + 1;
    } else {
      return (uint16)mid;
    }
  }

  return low;
}

void b2BroadPhase_ctor(b2BroadPhase *broad_phase, const b2AABB &worldAABB,
                       b2PairCallback *callback) {
  b2PairManager_ctor(&broad_phase->m_pairManager);
  b2PairManager_Initialize(&broad_phase->m_pairManager, broad_phase, callback);

  broad_phase->m_worldAABB = worldAABB;
  broad_phase->m_proxyCount = 0;

  b2Vec2 d = worldAABB.maxVertex - worldAABB.minVertex;
  broad_phase->m_quantizationFactor.x = USHRT_MAX / d.x;
  broad_phase->m_quantizationFactor.y = USHRT_MAX / d.y;

  for (uint16 i = 0; i < b2_maxProxies - 1; ++i) {
    b2Proxy_SetNext(&broad_phase->m_proxyPool[i], i + 1);
    broad_phase->m_proxyPool[i].timeStamp = 0;
    broad_phase->m_proxyPool[i].overlapCount = b2_invalid;
    broad_phase->m_proxyPool[i].userData = NULL;
  }
  b2Proxy_SetNext(&broad_phase->m_proxyPool[b2_maxProxies - 1], b2_nullProxy);
  broad_phase->m_proxyPool[b2_maxProxies - 1].timeStamp = 0;
  broad_phase->m_proxyPool[b2_maxProxies - 1].overlapCount = b2_invalid;
  broad_phase->m_proxyPool[b2_maxProxies - 1].userData = NULL;
  broad_phase->m_freeProxy = 0;

  broad_phase->m_timeStamp = 1;
  broad_phase->m_queryResultCount = 0;
}

bool b2BroadPhase_TestOverlap(b2BroadPhase *broad_phase, const b2BoundValues &b,
                              b2Proxy *p) {
  for (int32 axis = 0; axis < 2; ++axis) {
    b2Bound *bounds = broad_phase->m_bounds[axis];

    if (b.lowerValues[axis] > bounds[p->upperBounds[axis]].value)
      return false;

    if (b.upperValues[axis] < bounds[p->lowerBounds[axis]].value)
      return false;
  }

  return true;
}

static void b2BroadPhase_ComputeBounds(b2BroadPhase *broad_phase,
                                       uint16 *lowerValues, uint16 *upperValues,
                                       const b2AABB &aabb) {
  b2Vec2 minVertex = b2Clamp(aabb.minVertex, broad_phase->m_worldAABB.minVertex,
                             broad_phase->m_worldAABB.maxVertex);
  b2Vec2 maxVertex = b2Clamp(aabb.maxVertex, broad_phase->m_worldAABB.minVertex,
                             broad_phase->m_worldAABB.maxVertex);

  // Bump lower bounds downs and upper bounds up. This ensures correct sorting
  // of lower/upper bounds that would have equal values.
  // TODO_ERIN implement fast float to uint16 conversion.
  lowerValues[0] =
      (uint16)(broad_phase->m_quantizationFactor.x *
               (minVertex.x - broad_phase->m_worldAABB.minVertex.x)) &
      (USHRT_MAX - 1);
  upperValues[0] =
      (uint16)(broad_phase->m_quantizationFactor.x *
               (maxVertex.x - broad_phase->m_worldAABB.minVertex.x)) |
      1;

  lowerValues[1] =
      (uint16)(broad_phase->m_quantizationFactor.y *
               (minVertex.y - broad_phase->m_worldAABB.minVertex.y)) &
      (USHRT_MAX - 1);
  upperValues[1] =
      (uint16)(broad_phase->m_quantizationFactor.y *
               (maxVertex.y - broad_phase->m_worldAABB.minVertex.y)) |
      1;
}

static void b2BroadPhase_IncrementTimeStamp(b2BroadPhase *broad_phase) {
  if (broad_phase->m_timeStamp == USHRT_MAX) {
    for (uint16 i = 0; i < b2_maxProxies; ++i) {
      broad_phase->m_proxyPool[i].timeStamp = 0;
    }
    broad_phase->m_timeStamp = 1;
  } else {
    ++broad_phase->m_timeStamp;
  }
}

void b2BroadPhase_IncrementOverlapCount(b2BroadPhase *broad_phase,
                                        int32 proxyId) {
  b2Proxy *proxy = broad_phase->m_proxyPool + proxyId;
  if (proxy->timeStamp < broad_phase->m_timeStamp) {
    proxy->timeStamp = broad_phase->m_timeStamp;
    proxy->overlapCount = 1;
  } else {
    proxy->overlapCount = 2;
    broad_phase->m_queryResults[broad_phase->m_queryResultCount] =
        (uint16)proxyId;
    ++broad_phase->m_queryResultCount;
  }
}

static void b2BroadPhase_Query(b2BroadPhase *broad_phase, int32 *lowerQueryOut,
                               int32 *upperQueryOut, uint16 lowerValue,
                               uint16 upperValue, b2Bound *bounds,
                               int32 boundCount, int32 axis) {
  int32 lowerQuery = BinarySearch(bounds, boundCount, lowerValue);
  int32 upperQuery = BinarySearch(bounds, boundCount, upperValue);

  // Easy case: lowerQuery <= lowerIndex(i) < upperQuery
  // Solution: search query range for min bounds.
  for (int32 i = lowerQuery; i < upperQuery; ++i) {
    if (b2Bound_IsLower(&bounds[i])) {
      b2BroadPhase_IncrementOverlapCount(broad_phase, bounds[i].proxyId);
    }
  }

  // Hard case: lowerIndex(i) < lowerQuery < upperIndex(i)
  // Solution: use the stabbing count to search down the bound array.
  if (lowerQuery > 0) {
    int32 i = lowerQuery - 1;
    int32 s = bounds[i].stabbingCount;

    // Find the s overlaps.
    while (s) {
      if (b2Bound_IsLower(&bounds[i])) {
        b2Proxy *proxy = broad_phase->m_proxyPool + bounds[i].proxyId;
        if (lowerQuery <= proxy->upperBounds[axis]) {
          b2BroadPhase_IncrementOverlapCount(broad_phase, bounds[i].proxyId);
          --s;
        }
      }
      --i;
    }
  }

  *lowerQueryOut = lowerQuery;
  *upperQueryOut = upperQuery;
}

uint16 b2BroadPhase_CreateProxy(b2BroadPhase *broad_phase, const b2AABB &aabb,
                                void *userData) {
  uint16 proxyId = broad_phase->m_freeProxy;
  b2Proxy *proxy = broad_phase->m_proxyPool + proxyId;
  broad_phase->m_freeProxy = b2Proxy_GetNext(proxy);

  proxy->overlapCount = 0;
  proxy->userData = userData;

  int32 boundCount = 2 * broad_phase->m_proxyCount;

  uint16 lowerValues[2], upperValues[2];
  b2BroadPhase_ComputeBounds(broad_phase, lowerValues, upperValues, aabb);

  for (int32 axis = 0; axis < 2; ++axis) {
    b2Bound *bounds = broad_phase->m_bounds[axis];
    int32 lowerIndex, upperIndex;
    b2BroadPhase_Query(broad_phase, &lowerIndex, &upperIndex, lowerValues[axis],
                       upperValues[axis], bounds, boundCount, axis);

    memmove(bounds + upperIndex + 2, bounds + upperIndex,
            (boundCount - upperIndex) * sizeof(b2Bound));
    memmove(bounds + lowerIndex + 1, bounds + lowerIndex,
            (upperIndex - lowerIndex) * sizeof(b2Bound));

    // The upper index has increased because of the lower bound insertion.
    ++upperIndex;

    // Copy in the new bounds.
    bounds[lowerIndex].value = lowerValues[axis];
    bounds[lowerIndex].proxyId = proxyId;
    bounds[upperIndex].value = upperValues[axis];
    bounds[upperIndex].proxyId = proxyId;

    bounds[lowerIndex].stabbingCount =
        lowerIndex == 0 ? 0 : bounds[lowerIndex - 1].stabbingCount;
    bounds[upperIndex].stabbingCount = bounds[upperIndex - 1].stabbingCount;

    // Adjust the stabbing count between the new bounds.
    for (int32 index = lowerIndex; index < upperIndex; ++index) {
      ++bounds[index].stabbingCount;
    }

    // Adjust the all the affected bound indices.
    for (int32 index = lowerIndex; index < boundCount + 2; ++index) {
      b2Proxy *proxy = broad_phase->m_proxyPool + bounds[index].proxyId;
      if (b2Bound_IsLower(&bounds[index])) {
        proxy->lowerBounds[axis] = (uint16)index;
      } else {
        proxy->upperBounds[axis] = (uint16)index;
      }
    }
  }

  ++broad_phase->m_proxyCount;

  // Create pairs if the AABB is in range.
  for (int32 i = 0; i < broad_phase->m_queryResultCount; ++i) {
    b2PairManager_AddBufferedPair(&broad_phase->m_pairManager, proxyId,
                                  broad_phase->m_queryResults[i]);
  }

  b2PairManager_Commit(&broad_phase->m_pairManager);

  // Prepare for next query.
  broad_phase->m_queryResultCount = 0;
  b2BroadPhase_IncrementTimeStamp(broad_phase);

  return proxyId;
}

void b2BroadPhase_DestroyProxy(b2BroadPhase *broad_phase, int32 proxyId) {
  b2Proxy *proxy = broad_phase->m_proxyPool + proxyId;

  int32 boundCount = 2 * broad_phase->m_proxyCount;

  for (int32 axis = 0; axis < 2; ++axis) {
    b2Bound *bounds = broad_phase->m_bounds[axis];

    int32 lowerIndex = proxy->lowerBounds[axis];
    int32 upperIndex = proxy->upperBounds[axis];
    uint16 lowerValue = bounds[lowerIndex].value;
    uint16 upperValue = bounds[upperIndex].value;

    memmove(bounds + lowerIndex, bounds + lowerIndex + 1,
            (upperIndex - lowerIndex - 1) * sizeof(b2Bound));
    memmove(bounds + upperIndex - 1, bounds + upperIndex + 1,
            (boundCount - upperIndex - 1) * sizeof(b2Bound));

    // Fix bound indices.
    for (int32 index = lowerIndex; index < boundCount - 2; ++index) {
      b2Proxy *proxy = broad_phase->m_proxyPool + bounds[index].proxyId;
      if (b2Bound_IsLower(&bounds[index])) {
        proxy->lowerBounds[axis] = (uint16)index;
      } else {
        proxy->upperBounds[axis] = (uint16)index;
      }
    }

    // Fix stabbing count.
    for (int32 index = lowerIndex; index < upperIndex - 1; ++index) {
      --bounds[index].stabbingCount;
    }

    // Query for pairs to be removed. lowerIndex and upperIndex are not needed.
    b2BroadPhase_Query(broad_phase, &lowerIndex, &upperIndex, lowerValue,
                       upperValue, bounds, boundCount - 2, axis);
  }

  for (int32 i = 0; i < broad_phase->m_queryResultCount; ++i) {
    b2PairManager_RemoveBufferedPair(&broad_phase->m_pairManager, proxyId,
                                     broad_phase->m_queryResults[i]);
  }

  b2PairManager_Commit(&broad_phase->m_pairManager);

  // Prepare for next query.
  broad_phase->m_queryResultCount = 0;
  b2BroadPhase_IncrementTimeStamp(broad_phase);

  // Return the proxy to the pool.
  proxy->userData = NULL;
  proxy->overlapCount = b2_invalid;
  proxy->lowerBounds[0] = b2_invalid;
  proxy->lowerBounds[1] = b2_invalid;
  proxy->upperBounds[0] = b2_invalid;
  proxy->upperBounds[1] = b2_invalid;

  b2Proxy_SetNext(proxy, broad_phase->m_freeProxy);
  broad_phase->m_freeProxy = (uint16)proxyId;
  --broad_phase->m_proxyCount;
}

static bool b2AABB_IsValid(const b2AABB *aabb) {
  b2Vec2 d = aabb->maxVertex - aabb->minVertex;
  bool valid = d.x >= 0.0 && d.y >= 0;
  valid = valid && b2Vec2_IsValid(&aabb->minVertex) &&
          b2Vec2_IsValid(&aabb->maxVertex);
  return valid;
}

void b2BroadPhase_MoveProxy(b2BroadPhase *broad_phase, int32 proxyId,
                            const b2AABB &aabb) {
  if (proxyId == b2_nullProxy || b2_maxProxies <= proxyId) {
    return;
  }

  if (b2AABB_IsValid(&aabb) == false) {
    return;
  }

  int32 boundCount = 2 * broad_phase->m_proxyCount;

  b2Proxy *proxy = broad_phase->m_proxyPool + proxyId;

  // Get new bound values
  b2BoundValues newValues;
  b2BroadPhase_ComputeBounds(broad_phase, newValues.lowerValues,
                             newValues.upperValues, aabb);

  // Get old bound values
  b2BoundValues oldValues;
  for (int32 axis = 0; axis < 2; ++axis) {
    oldValues.lowerValues[axis] =
        broad_phase->m_bounds[axis][proxy->lowerBounds[axis]].value;
    oldValues.upperValues[axis] =
        broad_phase->m_bounds[axis][proxy->upperBounds[axis]].value;
  }

  for (int32 axis = 0; axis < 2; ++axis) {
    b2Bound *bounds = broad_phase->m_bounds[axis];

    int32 lowerIndex = proxy->lowerBounds[axis];
    int32 upperIndex = proxy->upperBounds[axis];

    uint16 lowerValue = newValues.lowerValues[axis];
    uint16 upperValue = newValues.upperValues[axis];

    int32 deltaLower = lowerValue - bounds[lowerIndex].value;
    int32 deltaUpper = upperValue - bounds[upperIndex].value;

    bounds[lowerIndex].value = lowerValue;
    bounds[upperIndex].value = upperValue;

    //
    // Expanding adds overlaps
    //

    // Should we move the lower bound down?
    if (deltaLower < 0) {
      int32 index = lowerIndex;
      while (index > 0 && lowerValue < bounds[index - 1].value) {
        b2Bound *bound = bounds + index;
        b2Bound *prevBound = bound - 1;

        int32 prevProxyId = prevBound->proxyId;
        b2Proxy *prevProxy = broad_phase->m_proxyPool + prevBound->proxyId;

        ++prevBound->stabbingCount;

        if (b2Bound_IsUpper(prevBound) == true) {
          if (b2BroadPhase_TestOverlap(broad_phase, newValues, prevProxy)) {
            b2PairManager_AddBufferedPair(&broad_phase->m_pairManager, proxyId,
                                          prevProxyId);
          }

          ++prevProxy->upperBounds[axis];
          ++bound->stabbingCount;
        } else {
          ++prevProxy->lowerBounds[axis];
          --bound->stabbingCount;
        }

        --proxy->lowerBounds[axis];
        b2Swap(*bound, *prevBound);
        --index;
      }
    }

    // Should we move the upper bound up?
    if (deltaUpper > 0) {
      int32 index = upperIndex;
      while (index < boundCount - 1 && bounds[index + 1].value <= upperValue) {
        b2Bound *bound = bounds + index;
        b2Bound *nextBound = bound + 1;
        int32 nextProxyId = nextBound->proxyId;
        b2Proxy *nextProxy = broad_phase->m_proxyPool + nextProxyId;

        ++nextBound->stabbingCount;

        if (b2Bound_IsLower(nextBound) == true) {
          if (b2BroadPhase_TestOverlap(broad_phase, newValues, nextProxy)) {
            b2PairManager_AddBufferedPair(&broad_phase->m_pairManager, proxyId,
                                          nextProxyId);
          }

          --nextProxy->lowerBounds[axis];
          ++bound->stabbingCount;
        } else {
          --nextProxy->upperBounds[axis];
          --bound->stabbingCount;
        }

        ++proxy->upperBounds[axis];
        b2Swap(*bound, *nextBound);
        ++index;
      }
    }

    //
    // Shrinking removes overlaps
    //

    // Should we move the lower bound up?
    if (deltaLower > 0) {
      int32 index = lowerIndex;
      while (index < boundCount - 1 && bounds[index + 1].value <= lowerValue) {
        b2Bound *bound = bounds + index;
        b2Bound *nextBound = bound + 1;

        int32 nextProxyId = nextBound->proxyId;
        b2Proxy *nextProxy = broad_phase->m_proxyPool + nextProxyId;

        --nextBound->stabbingCount;

        if (b2Bound_IsUpper(nextBound)) {
          if (b2BroadPhase_TestOverlap(broad_phase, oldValues, nextProxy)) {
            b2PairManager_RemoveBufferedPair(&broad_phase->m_pairManager,
                                             proxyId, nextProxyId);
          }

          --nextProxy->upperBounds[axis];
          --bound->stabbingCount;
        } else {
          --nextProxy->lowerBounds[axis];
          ++bound->stabbingCount;
        }

        ++proxy->lowerBounds[axis];
        b2Swap(*bound, *nextBound);
        ++index;
      }
    }

    // Should we move the upper bound down?
    if (deltaUpper < 0) {
      int32 index = upperIndex;
      while (index > 0 && upperValue < bounds[index - 1].value) {
        b2Bound *bound = bounds + index;
        b2Bound *prevBound = bound - 1;

        int32 prevProxyId = prevBound->proxyId;
        b2Proxy *prevProxy = broad_phase->m_proxyPool + prevProxyId;

        --prevBound->stabbingCount;

        if (b2Bound_IsLower(prevBound) == true) {
          if (b2BroadPhase_TestOverlap(broad_phase, oldValues, prevProxy)) {
            b2PairManager_RemoveBufferedPair(&broad_phase->m_pairManager,
                                             proxyId, prevProxyId);
          }

          ++prevProxy->lowerBounds[axis];
          --bound->stabbingCount;
        } else {
          ++prevProxy->upperBounds[axis];
          ++bound->stabbingCount;
        }

        --proxy->upperBounds[axis];
        b2Swap(*bound, *prevBound);
        --index;
      }
    }
  }
}

void b2BroadPhase_Commit(b2BroadPhase *broad_phase) {
  b2PairManager_Commit(&broad_phase->m_pairManager);
}
