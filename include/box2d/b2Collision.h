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

#ifndef B2_COLLISION_H
#define B2_COLLISION_H

#include <box2d/b2Vec.h>
#include <limits.h>

typedef struct b2Shape b2Shape;
typedef struct b2CircleShape b2CircleShape;
typedef struct b2PolyShape b2PolyShape;
struct b2Shape;
struct b2CircleShape;
struct b2PolyShape;

// We use contact ids to facilitate warm starting.
static const uint8 b2_nullFeature = UCHAR_MAX;

typedef union b2ContactID b2ContactID;
union b2ContactID {
  struct Features {
    uint8 referenceFace;
    uint8 incidentEdge;
    uint8 incidentVertex;
    uint8 flip;
  } features;
  uint32 key;
};

typedef struct b2ContactPoint b2ContactPoint;
struct b2ContactPoint {
  b2Vec2 position;
  float64 separation;
  float64 normalImpulse;
  float64 tangentImpulse;
  b2ContactID id;
};

// A manifold for two touching convex shapes.
typedef struct b2Manifold b2Manifold;
struct b2Manifold {
  b2ContactPoint points[b2_maxManifoldPoints];
  b2Vec2 normal;
  int32 pointCount;
};

typedef struct b2AABB b2AABB;
struct b2AABB {
  b2Vec2 minVertex, maxVertex;
};

typedef struct b2OBB b2OBB;
struct b2OBB {
  b2Mat22 R;
  b2Vec2 center;
  b2Vec2 extents;
};

#ifdef __cplusplus
extern "C" {
#endif

void b2CollideCircle(b2Manifold *manifold, b2CircleShape *circle1,
                     b2CircleShape *circle2, bool conservative);
void b2CollidePolyAndCircle(b2Manifold *manifold, const b2PolyShape *poly,
                            const b2CircleShape *circle, bool conservative);
void b2CollidePoly(b2Manifold *manifold, const b2PolyShape *poly1,
                   const b2PolyShape *poly2, bool conservative);

#ifdef __cplusplus
}
#endif

#endif
