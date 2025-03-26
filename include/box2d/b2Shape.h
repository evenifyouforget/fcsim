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

#ifndef B2_SHAPE_H
#define B2_SHAPE_H

#include <box2d/b2Vec.h>
#include <box2d/b2Collision.h>

typedef struct b2Body b2Body;
typedef struct b2BroadPhase b2BroadPhase;
struct b2Body;
struct b2BroadPhase;

typedef struct b2MassData b2MassData;
struct b2MassData
{
	float64 mass;
	b2Vec2 center;
	float64 I;
};

enum b2ShapeType
{
	e_unknownShape = -1,
	e_circleShape,
	e_boxShape,
	e_polyShape,
	e_meshShape,
	e_shapeTypeCount,
};
typedef enum b2ShapeType b2ShapeType;

typedef struct b2ShapeDef b2ShapeDef;
struct b2ShapeDef
{
	b2ShapeType type;
	void* userData;
	b2Vec2 localPosition;
	float64 localRotation;
	float64 friction;
	float64 restitution;
	float64 density;
};

typedef struct b2CircleDef b2CircleDef;
struct b2CircleDef
{
	b2ShapeDef m_shapeDef;

	float64 radius;
};

typedef struct b2BoxDef b2BoxDef;
struct b2BoxDef
{
	b2ShapeDef m_shapeDef;

	b2Vec2 extents;
};

// Convex polygon, vertices must be in CCW order.
typedef struct b2PolyDef b2PolyDef;
struct b2PolyDef
{
	b2ShapeDef m_shapeDef;

	b2Vec2 vertices[b2_maxPolyVertices];
	int32 vertexCount;
};

// Shapes are created automatically when a body is created.
// Client code does not normally interact with shapes.
typedef struct b2Shape b2Shape;
struct b2Shape
{
	bool (*TestPoint)(b2Shape *shape, b2Vec2 p);

	// Remove and then add proxy from the broad-phase.
	// This is used to refresh the collision filters.
	void (*ResetProxy)(b2Shape *shape, b2BroadPhase* broadPhase);

	void (*Synchronize)(b2Shape *shape,
			    b2Vec2 position1, const b2Mat22* R1,
			    b2Vec2 position2, const b2Mat22* R2);

	b2Vec2 (*Support)(const b2Shape *shape, b2Vec2 d);

	b2Shape* m_next;

	b2Mat22 m_R;
	b2Vec2 m_position;

	b2ShapeType m_type;

	void* m_userData;

	b2Body* m_body;

	float64 m_friction;
	float64 m_restitution;

	uint16 m_proxyId;
};

typedef struct b2CircleShape b2CircleShape;
struct b2CircleShape
{
	b2Shape m_shape;

	// Local position in parent body
	b2Vec2 m_localPosition;
	float64 m_radius;
};

// A convex polygon. The position of the polygon (m_position) is the
// position of the centroid. The vertices of the incoming polygon are pre-rotated
// according to the local rotation. The vertices are also shifted to be centered
// on the centroid. Since the local rotation is absorbed into the vertex
// coordinates, the polygon rotation is equal to the body rotation. However,
// the polygon position is centered on the polygon centroid. This simplifies
// some collision algorithms.
typedef struct b2PolyShape b2PolyShape;
struct b2PolyShape
{
	b2Shape m_shape;

	// Local position of the shape centroid in parent body frame.
	b2Vec2 m_localCentroid;

	// Local position oriented bounding box. The OBB center is relative to
	// shape centroid.
	b2OBB m_localOBB;
	b2Vec2 m_vertices[b2_maxPolyVertices];
	b2Vec2 m_coreVertices[b2_maxPolyVertices];
	int32 m_vertexCount;
	b2Vec2 m_normals[b2_maxPolyVertices];
};

#ifdef __cplusplus
extern "C" {
#endif

void b2ShapeDef_ctor(b2ShapeDef *def);

void b2BoxDef_ctor(b2BoxDef *boxDef);

void b2ShapeDef_ComputeMass(const b2ShapeDef *shapeDef, b2MassData* massData);

bool b2CircleShape_TestPoint(b2Shape *shape, b2Vec2 p);

void b2CircleShape_ResetProxy(b2Shape *shape, b2BroadPhase* broadPhase);

void b2CircleShape_Synchronize(b2Shape *shape,
			       b2Vec2 position1, const b2Mat22* R1,
			       b2Vec2 position2, const b2Mat22* R2);

b2Vec2 b2CircleShape_Support(const b2Shape *shape, b2Vec2 d);

bool b2PolyShape_TestPoint(b2Shape *shape, b2Vec2 p);

void b2PolyShape_ResetProxy(b2Shape *shape, b2BroadPhase* broadPhase);

void b2PolyShape_Synchronize(b2Shape *shape,
			     b2Vec2 position1, const b2Mat22* R1,
			     b2Vec2 position2, const b2Mat22* R2);

b2Vec2 b2PolyShape_Support(const b2Shape *shape, b2Vec2 d);

static inline void* b2Shape_GetUserData(b2Shape *shape)
{
	return shape->m_userData;
}

b2Shape* b2Shape_Create(const b2ShapeDef* def,
			b2Body* body, b2Vec2 newOrigin);

void b2Shape_Destroy(b2Shape* shape);

void b2Shape_DestroyProxy(b2Shape *shape);

#ifdef __cplusplus
}
#endif

#endif
