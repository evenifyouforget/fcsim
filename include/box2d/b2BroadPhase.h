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

#ifndef B2_BROAD_PHASE_H
#define B2_BROAD_PHASE_H

/*
This broad phase uses the Sweep and Prune algorithm as described in:
Collision Detection in Interactive 3D Environments by Gino van den Bergen
Also, some ideas, such as using integral values for fast compares comes from
Bullet (http:/www.bulletphysics.com).
*/

#include <box2d/b2Collision.h>
#include <box2d/b2Math.h>
#include <box2d/b2PairManager.h>
#include <box2d/b2Settings.h>
#include <limits.h>

const uint16 b2_invalid = USHRT_MAX;
const uint16 b2_nullEdge = USHRT_MAX;

typedef struct b2BoundValues b2BoundValues;
struct b2BoundValues;

typedef struct b2Bound b2Bound;
struct b2Bound {
  uint16 value;
  uint16 proxyId;
  uint16 stabbingCount;
};

static inline bool b2Bound_IsLower(const b2Bound *bound) {
  return (bound->value & 1) == 0;
}

static inline bool b2Bound_IsUpper(const b2Bound *bound) {
  return (bound->value & 1) == 1;
}

typedef struct b2Proxy b2Proxy;
struct b2Proxy {
  uint16 lowerBounds[2], upperBounds[2];
  uint16 overlapCount;
  uint16 timeStamp;
  void *userData;
};

static inline uint16 b2Proxy_GetNext(const b2Proxy *proxy) {
  return proxy->lowerBounds[0];
}

static inline void b2Proxy_SetNext(b2Proxy *proxy, uint16 next) {
  proxy->lowerBounds[0] = next;
}

static inline bool b2Proxy_IsValid(const b2Proxy *proxy) {
  return proxy->overlapCount != b2_invalid;
}

typedef struct b2BroadPhase b2BroadPhase;
struct b2BroadPhase {
  b2PairManager m_pairManager;

  b2Proxy m_proxyPool[b2_maxProxies];
  uint16 m_freeProxy;

  b2Bound m_bounds[2][2 * b2_maxProxies];

  uint16 m_queryResults[b2_maxProxies];
  int32 m_queryResultCount;

  b2AABB m_worldAABB;
  b2Vec2 m_quantizationFactor;
  int32 m_proxyCount;
  uint16 m_timeStamp;
};

void b2BroadPhase_ctor(b2BroadPhase *broad_phase, const b2AABB &worldAABB,
                       b2PairCallback *callback);

// Use this to see if your proxy is in range. If it is not in range,
// it should be destroyed. Otherwise you may get O(m^2) pairs, where m
// is the number of proxies that are out of range.
static inline bool b2BroadPhase_InRange(const b2BroadPhase *broad_phase,
                                        const b2AABB &aabb) {
  b2Vec2 d = b2Max(aabb.minVertex - broad_phase->m_worldAABB.maxVertex,
                   broad_phase->m_worldAABB.minVertex - aabb.maxVertex);
  return b2Max(d.x, d.y) < 0.0;
}

static inline b2Proxy *b2BroadPhase_GetProxy(b2BroadPhase *broad_phase,
                                             int32 proxyId) {
  if (proxyId == b2_nullProxy ||
      b2Proxy_IsValid(&broad_phase->m_proxyPool[proxyId]) == false) {
    return NULL;
  }

  return broad_phase->m_proxyPool + proxyId;
}

// Create and destroy proxies. These call Flush first.
uint16 b2BroadPhase_CreateProxy(b2BroadPhase *broad_phase, const b2AABB &aabb,
                                void *userData);

void b2BroadPhase_DestroyProxy(b2BroadPhase *broad_phase, int32 proxyId);

// Call MoveProxy as many times as you like, then when you are done
// call Commit to finalized the proxy pairs (for your time step).
void b2BroadPhase_MoveProxy(b2BroadPhase *broad_phase, int32 proxyId,
                            const b2AABB &aabb);

void b2BroadPhase_Commit(b2BroadPhase *broad_phase);

#endif
