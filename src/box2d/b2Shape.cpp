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

#include <box2d/b2Math.h>
#include <box2d/b2BroadPhase.h>
#include <box2d/b2Shape.h>
#include <box2d/b2Body.h>
#include <box2d/b2World.h>
#include <box2d/b2BlockAllocator.h>

// Polygon mass, centroid, and inertia.
// Let rho be the polygon density in mass per unit area.
// Then:
// mass = rho * int(dA)
// centroid.x = (1/mass) * rho * int(x * dA)
// centroid.y = (1/mass) * rho * int(y * dA)
// I = rho * int((x*x + y*y) * dA)
//
// We can compute these integrals by summing all the integrals
// for each triangle of the polygon. To evaluate the integral
// for a single triangle, we make a change of variables to
// the (u,v) coordinates of the triangle:
// x = x0 + e1x * u + e2x * v
// y = y0 + e1y * u + e2y * v
// where 0 <= u && 0 <= v && u + v <= 1.
//
// We integrate u from [0,1-v] and then v from [0,1].
// We also need to use the Jacobian of the transformation:
// D = cross(e1, e2)
//
// Simplification: triangle centroid = (1/3) * (p1 + p2 + p3)
//
// The rest of the derivation is handled by computer algebra.
static void PolyMass(b2MassData* massData, const b2Vec2* vs, int32 count, float64 rho)
{
	b2Vec2 center; b2Vec2_Set(&center, 0.0, 0.0);
	float64 area = 0.0;
	float64 I = 0.0;

	// pRef is the reference point for forming triangles.
	// It's location doesn't change the result (except for rounding error).
	b2Vec2 pRef;
	b2Vec2_Set(&pRef, 0.0, 0.0);
#if 0
	// This code would put the reference point inside the polygon.
	for (int32 i = 0; i < count; ++i)
	{
		pRef += vs[i];
	}
	pRef *= 1.0 / count;
#endif

	const float64 inv3 = 1.0 / 3.0;

	for (int32 i = 0; i < count; ++i)
	{
		// Triangle vertices.
		b2Vec2 p1 = pRef;
		b2Vec2 p2 = vs[i];
		b2Vec2 p3 = i + 1 < count ? vs[i+1] : vs[0];

		b2Vec2 e1 = p2 - p1;
		b2Vec2 e2 = p3 - p1;

		float64 D = b2Cross(e1, e2);

		float64 triangleArea = 0.5 * D;
		area += triangleArea;

		// Area weighted centroid
		center += triangleArea * inv3 * (p1 + p2 + p3);

		float64 px = p1.x, py = p1.y;
		float64 ex1 = e1.x, ey1 = e1.y;
		float64 ex2 = e2.x, ey2 = e2.y;

		float64 intx2 = inv3 * (0.25 * (ex1*ex1 + ex2*ex1 + ex2*ex2) + (px*ex1 + px*ex2)) + 0.5*px*px;
		float64 inty2 = inv3 * (0.25 * (ey1*ey1 + ey2*ey1 + ey2*ey2) + (py*ey1 + py*ey2)) + 0.5*py*py;

		I += D * (intx2 + inty2);
	}

	// Total mass
	massData->mass = rho * area;

	// Center of mass
	center *= 1.0 / area;
	massData->center = center;

	// Inertia tensor relative to the center.
	I = rho * (I - area * b2Dot(center, center));
	massData->I = I;
}

static b2Vec2 PolyCentroid(const b2Vec2* vs, int32 count)
{
	b2Vec2 c; b2Vec2_Set(&c, 0.0, 0.0);
	float64 area = 0.0;

	// pRef is the reference point for forming triangles.
	// It's location doesn't change the result (except for rounding error).
	b2Vec2 pRef;
	b2Vec2_Set(&pRef, 0.0, 0.0);
#if 0
	// This code would put the reference point inside the polygon.
	for (int32 i = 0; i < count; ++i)
	{
		pRef += vs[i];
	}
	pRef *= 1.0 / count;
#endif

	const float64 inv3 = 1.0 / 3.0;

	for (int32 i = 0; i < count; ++i)
	{
		// Triangle vertices.
		b2Vec2 p1 = pRef;
		b2Vec2 p2 = vs[i];
		b2Vec2 p3 = i + 1 < count ? vs[i+1] : vs[0];

		b2Vec2 e1 = p2 - p1;
		b2Vec2 e2 = p3 - p1;

		float64 D = b2Cross(e1, e2);

		float64 triangleArea = 0.5 * D;
		area += triangleArea;

		// Area weighted centroid
		c += triangleArea * inv3 * (p1 + p2 + p3);
	}

	// Centroid
	c *= 1.0 / area;
	return c;
}

void b2ShapeDef_ctor(b2ShapeDef *def)
{
	def->type = e_unknownShape;
	def->userData = NULL;
	b2Vec2_Set(&def->localPosition, 0.0, 0.0);
	def->localRotation = 0.0;
	def->friction = 0.2;
	def->restitution = 0.0;
	def->density = 0.0;
}

void b2ShapeDef_ComputeMass(const b2ShapeDef *shapeDef, b2MassData* massData)
{
	if (shapeDef->density == 0.0)
	{
		massData->mass = 0.0;
		b2Vec2_Set(&massData->center, 0.0, 0.0);
		massData->I = 0.0;
	}

	switch (shapeDef->type)
	{
	case e_circleShape:
		{
			b2CircleDef* circle = (b2CircleDef*)shapeDef;
			massData->mass = shapeDef->density * b2_pi * circle->radius * circle->radius;
			b2Vec2_Set(&massData->center, 0.0, 0.0);
			massData->I = 0.5 * (massData->mass) * circle->radius * circle->radius;
		}
		break;

	case e_boxShape:
		{
			b2BoxDef* box = (b2BoxDef*)shapeDef;
			massData->mass = 4.0 * shapeDef->density * box->extents.x * box->extents.y;
			b2Vec2_Set(&massData->center, 0.0, 0.0);
			massData->I = massData->mass / 3.0 * b2Dot(box->extents, box->extents);
		}
		break;

	case e_polyShape:
		{
			b2PolyDef* poly = (b2PolyDef*)shapeDef;
			PolyMass(massData, poly->vertices, poly->vertexCount, shapeDef->density);
		}
		break;

	default:
		massData->mass = 0.0;
		b2Vec2_Set(&massData->center, 0.0, 0.0);
		massData->I = 0.0;
		break;
	}
}

void b2BoxDef_ctor(b2BoxDef *boxDef)
{
	b2ShapeDef_ctor(&boxDef->m_shapeDef);
	boxDef->m_shapeDef.type = e_boxShape;
	b2Vec2_Set(&boxDef->extents, 1.0, 1.0);
} 

static void b2CircleShape_ctor(b2CircleShape *circleShape, const b2ShapeDef* def, b2Body* body, const b2Vec2& localCenter);

extern "C" {
void b2PolyShape_ctor(b2PolyShape *polyShape,
			     const b2ShapeDef* def, b2Body* body,
			     const b2Vec2 newOrigin);
}

b2Shape* b2Shape_Create(const b2ShapeDef* def,
			b2Body* body, b2Vec2 center)
{
	switch (def->type)
	{
	case e_circleShape:
		{
			b2CircleShape* mem = (b2CircleShape *)b2BlockAllocator_Allocate(&body->m_world->m_blockAllocator, sizeof(b2CircleShape));
			b2CircleShape_ctor(mem, def, body, center);
			return (b2Shape *)mem;
		}

	case e_boxShape:
	case e_polyShape:
		{
			b2PolyShape* mem = (b2PolyShape *)b2BlockAllocator_Allocate(&body->m_world->m_blockAllocator, sizeof(b2PolyShape));
			b2PolyShape_ctor(mem, def, body, center);
			return (b2Shape *)mem;
		}
	default:break;
	}

	return NULL;
}

static void b2Shape_dtor(b2Shape *shape);

void b2Shape_Destroy(b2Shape* shape)
{
	b2BlockAllocator& allocator = shape->m_body->m_world->m_blockAllocator;
	b2Shape_dtor(shape);

	switch (shape->m_type)
	{
	case e_circleShape:
		b2BlockAllocator_Free(&allocator, shape, sizeof(b2CircleShape));
		break;

	case e_polyShape:
		b2BlockAllocator_Free(&allocator, shape, sizeof(b2PolyShape));
		break;

	default:break;
	}
}

static void b2Shape_ctor(b2Shape *shape, const b2ShapeDef* def, b2Body* body)
{
	shape->m_userData = def->userData;
	shape->m_friction = def->friction;
	shape->m_restitution = def->restitution;
	shape->m_body = body;

	shape->m_proxyId = b2_nullProxy;
}

static void b2Shape_dtor(b2Shape *shape)
{
	if (shape->m_proxyId != b2_nullProxy)
	{
		b2BroadPhase_DestroyProxy(shape->m_body->m_world->m_broadPhase, shape->m_proxyId);
	}
}

void b2Shape_DestroyProxy(b2Shape *shape)
{
	if (shape->m_proxyId != b2_nullProxy)
	{
		b2BroadPhase_DestroyProxy(shape->m_body->m_world->m_broadPhase, shape->m_proxyId);
		shape->m_proxyId = b2_nullProxy;
	}
}

static void b2CircleShape_ctor(b2CircleShape *circleShape, const b2ShapeDef* def, b2Body* body, const b2Vec2& localCenter)
{
	b2Shape_ctor(&circleShape->m_shape, def, body);
	circleShape->m_shape.TestPoint = b2CircleShape_TestPoint;
	circleShape->m_shape.ResetProxy = b2CircleShape_ResetProxy;
	circleShape->m_shape.Synchronize = b2CircleShape_Synchronize;
	circleShape->m_shape.Support = b2CircleShape_Support;

	const b2CircleDef* circle = (const b2CircleDef*)def;

	circleShape->m_localPosition = def->localPosition - localCenter;
	circleShape->m_shape.m_type = e_circleShape;
	circleShape->m_radius = circle->radius;

	circleShape->m_shape.m_R = circleShape->m_shape.m_body->m_R;
	b2Vec2 r = b2Mul(circleShape->m_shape.m_body->m_R, circleShape->m_localPosition);
	circleShape->m_shape.m_position = circleShape->m_shape.m_body->m_position + r;

	b2AABB aabb;
	b2Vec2_Set(&aabb.minVertex, circleShape->m_shape.m_position.x - circleShape->m_radius, circleShape->m_shape.m_position.y - circleShape->m_radius);
	b2Vec2_Set(&aabb.maxVertex, circleShape->m_shape.m_position.x + circleShape->m_radius, circleShape->m_shape.m_position.y + circleShape->m_radius);

	b2BroadPhase* broadPhase = circleShape->m_shape.m_body->m_world->m_broadPhase;
	if (b2BroadPhase_InRange(broadPhase, aabb))
	{
		circleShape->m_shape.m_proxyId = b2BroadPhase_CreateProxy(broadPhase, aabb, circleShape);
	}
	else
	{
		circleShape->m_shape.m_proxyId = b2_nullProxy;
	}

	if (circleShape->m_shape.m_proxyId == b2_nullProxy)
	{
		b2Body_Freeze(circleShape->m_shape.m_body);
	}
}

void b2CircleShape_Synchronize(b2Shape *shape,
			       b2Vec2 position1, const b2Mat22 *R1,
			       b2Vec2 position2, const b2Mat22 *R2)
{
	b2CircleShape *circleShape = (b2CircleShape *)shape;
	shape->m_R = *R2;
	shape->m_position = position2 + b2Mul(shape->m_R, circleShape->m_localPosition);

	if (shape->m_proxyId == b2_nullProxy)
	{
		return;
	}

	// Compute an AABB that covers the swept shape (may miss some rotation effect).
	b2Vec2 p1 = position1 + b2Mul(*R1, circleShape->m_localPosition);
	b2Vec2 lower = b2Min(p1, shape->m_position);
	b2Vec2 upper = b2Max(p1, shape->m_position);

	b2AABB aabb;
	b2Vec2_Set(&aabb.minVertex, lower.x - circleShape->m_radius, lower.y - circleShape->m_radius);
	b2Vec2_Set(&aabb.maxVertex, upper.x + circleShape->m_radius, upper.y + circleShape->m_radius);

	b2BroadPhase* broadPhase = shape->m_body->m_world->m_broadPhase;
	if (b2BroadPhase_InRange(broadPhase, aabb))
	{
		b2BroadPhase_MoveProxy(broadPhase, shape->m_proxyId, aabb);
	}
	else
	{
		b2Body_Freeze(shape->m_body);
	}
}

b2Vec2 b2CircleShape_Support(const b2Shape *shape, b2Vec2 d)
{
	b2CircleShape *circleShape = (b2CircleShape *)shape;
	b2Vec2 u = d;
	float64 len = b2Vec2_Length(&u);
	u.x /= len;
	u.y /= len;
	return shape->m_position + circleShape->m_radius * u;
}

bool b2CircleShape_TestPoint(b2Shape *shape, b2Vec2 p)
{
	b2CircleShape *circleShape = (b2CircleShape *)shape;
	b2Vec2 d = p - shape->m_position;
	return b2Dot(d, d) <= circleShape->m_radius * circleShape->m_radius;
}

void b2CircleShape_ResetProxy(b2Shape *shape, b2BroadPhase* broadPhase)
{
	b2CircleShape *circleShape = (b2CircleShape *)shape;

	if (shape->m_proxyId == b2_nullProxy)
	{
		return;
	}

	b2BroadPhase_GetProxy(broadPhase, shape->m_proxyId);

	b2BroadPhase_DestroyProxy(broadPhase, shape->m_proxyId);

	b2AABB aabb;
	b2Vec2_Set(&aabb.minVertex, shape->m_position.x - circleShape->m_radius, shape->m_position.y - circleShape->m_radius);
	b2Vec2_Set(&aabb.maxVertex, shape->m_position.x + circleShape->m_radius, shape->m_position.y + circleShape->m_radius);

	if (b2BroadPhase_InRange(broadPhase, aabb))
	{
		shape->m_proxyId = b2BroadPhase_CreateProxy(broadPhase, aabb, shape);
	}
	else
	{
		shape->m_proxyId = b2_nullProxy;
	}

	if (shape->m_proxyId == b2_nullProxy)
	{
		b2Body_Freeze(shape->m_body);
	}
}




void b2PolyShape_ctor(b2PolyShape *polyShape,
			     const b2ShapeDef* def, b2Body* body,
			     const b2Vec2 newOrigin)
{
	b2Shape_ctor(&polyShape->m_shape, def, body);
	polyShape->m_shape.TestPoint = b2PolyShape_TestPoint;
	polyShape->m_shape.ResetProxy = b2PolyShape_ResetProxy;
	polyShape->m_shape.Synchronize = b2PolyShape_Synchronize;
	polyShape->m_shape.Support = b2PolyShape_Support;

	polyShape->m_shape.m_type = e_polyShape;
	b2Mat22 localR;
	b2Mat22_SetAngle(&localR, def->localRotation);

	// Get the vertices transformed into the body frame.
	if (def->type == e_boxShape)
	{
		polyShape->m_localCentroid = def->localPosition - newOrigin;

		const b2BoxDef* box = (const b2BoxDef*)def;
		polyShape->m_vertexCount = 4;
		b2Vec2 h = box->extents;
		b2Vec2 hc = h;
		hc.x = b2Max(0.0, h.x - 2.0 * b2_linearSlop);
		hc.y = b2Max(0.0, h.y - 2.0 * b2_linearSlop);
		polyShape->m_vertices[0] = b2Mul(localR, b2Vec2_Make(h.x, h.y));
		polyShape->m_vertices[1] = b2Mul(localR, b2Vec2_Make(-h.x, h.y));
		polyShape->m_vertices[2] = b2Mul(localR, b2Vec2_Make(-h.x, -h.y));
		polyShape->m_vertices[3] = b2Mul(localR, b2Vec2_Make(h.x, -h.y));

		polyShape->m_coreVertices[0] = b2Mul(localR, b2Vec2_Make(hc.x, hc.y));
		polyShape->m_coreVertices[1] = b2Mul(localR, b2Vec2_Make(-hc.x, hc.y));
		polyShape->m_coreVertices[2] = b2Mul(localR, b2Vec2_Make(-hc.x, -hc.y));
		polyShape->m_coreVertices[3] = b2Mul(localR, b2Vec2_Make(hc.x, -hc.y));
	}
	else
	{
		const b2PolyDef* poly = (const b2PolyDef*)def;
		polyShape->m_vertexCount = poly->vertexCount;
		b2Vec2 centroid = PolyCentroid(poly->vertices, poly->vertexCount);
		polyShape->m_localCentroid = def->localPosition + b2Mul(localR, centroid) - newOrigin;
		for (int32 i = 0; i < polyShape->m_vertexCount; ++i)
		{
			polyShape->m_vertices[i] = b2Mul(localR, poly->vertices[i] - centroid);

			b2Vec2 u = polyShape->m_vertices[i];
			float64 length = b2Vec2_Length(&u);
			if (length > MIN_VALUE)
			{
				u *= 1.0 / length;
			}

			polyShape->m_coreVertices[i] = polyShape->m_vertices[i] - 2.0 * b2_linearSlop * u;
		}
	}

	// Compute bounding box. TODO_ERIN optimize OBB
	b2Vec2 minVertex;
	b2Vec2_Set(&minVertex, DBL_MAX, DBL_MAX);
	b2Vec2 maxVertex;
	b2Vec2_Set(&maxVertex, -DBL_MAX, -DBL_MAX);
	for (int32 i = 0; i < polyShape->m_vertexCount; ++i)
	{
		b2Vec2 v = polyShape->m_vertices[i];
		minVertex = b2Min(minVertex, v);
		maxVertex = b2Max(maxVertex, v);
	}

	b2Mat22_SetIdentity(&polyShape->m_localOBB.R);
	polyShape->m_localOBB.center = 0.5 * (minVertex + maxVertex);
	polyShape->m_localOBB.extents = 0.5 * (maxVertex - minVertex);

	// Compute the edge normals and next index map.
	for (int32 i = 0; i < polyShape->m_vertexCount; ++i)
	{
		int32 i1 = i;
		int32 i2 = i + 1 < polyShape->m_vertexCount ? i + 1 : 0;
		b2Vec2 edge = polyShape->m_vertices[i2] - polyShape->m_vertices[i1];
		polyShape->m_normals[i] = b2Cross(edge, 1.0);
		b2Vec2_Normalize(&polyShape->m_normals[i]);
	}

	polyShape->m_shape.m_R = polyShape->m_shape.m_body->m_R;
	polyShape->m_shape.m_position = polyShape->m_shape.m_body->m_position + b2Mul(polyShape->m_shape.m_body->m_R, polyShape->m_localCentroid);

	b2Mat22 R = b2Mul(polyShape->m_shape.m_R, polyShape->m_localOBB.R);
	b2Mat22 absR = b2Abs(R);
	b2Vec2 h = b2Mul(absR, polyShape->m_localOBB.extents);
	b2Vec2 position = polyShape->m_shape.m_position + b2Mul(polyShape->m_shape.m_R, polyShape->m_localOBB.center);
	b2AABB aabb;
	aabb.minVertex = position - h;
	aabb.maxVertex = position + h;

	if (!polyShape->m_shape.m_body->m_world)
		return;

	b2BroadPhase* broadPhase = polyShape->m_shape.m_body->m_world->m_broadPhase;
	if (b2BroadPhase_InRange(broadPhase, aabb))
	{
		polyShape->m_shape.m_proxyId = b2BroadPhase_CreateProxy(broadPhase, aabb, polyShape);
	}
	else
	{
		polyShape->m_shape.m_proxyId = b2_nullProxy;
	}

	if (polyShape->m_shape.m_proxyId == b2_nullProxy)
	{
		b2Body_Freeze(polyShape->m_shape.m_body);
	}
}

void b2PolyShape_Synchronize(b2Shape *shape,
			     b2Vec2 position1, const b2Mat22 *_R1,
			     b2Vec2 position2, const b2Mat22 *_R2)
{
	b2PolyShape *polyShape = (b2PolyShape *)shape;
	b2Mat22 R1 = *_R1;
	b2Mat22 R2 = *_R2;
	// The body transform is copied for convenience.
	shape->m_R = R2;
	shape->m_position = position2 + b2Mul(R2, polyShape->m_localCentroid);

	if (shape->m_proxyId == b2_nullProxy)
	{
		return;
	}

	b2AABB aabb1, aabb2;

	{
		b2Mat22 obbR = b2Mul(R1, polyShape->m_localOBB.R);
		b2Mat22 absR = b2Abs(obbR);
		b2Vec2 h = b2Mul(absR, polyShape->m_localOBB.extents);
		b2Vec2 center = position1 + b2Mul(R1, polyShape->m_localCentroid + polyShape->m_localOBB.center);
		aabb1.minVertex = center - h;
		aabb1.maxVertex = center + h;
	}

	{
		b2Mat22 obbR = b2Mul(R2, polyShape->m_localOBB.R);
		b2Mat22 absR = b2Abs(obbR);
		b2Vec2 h = b2Mul(absR, polyShape->m_localOBB.extents);
		b2Vec2 center = position2 + b2Mul(R2, polyShape->m_localCentroid + polyShape->m_localOBB.center);
		aabb2.minVertex = center - h;
		aabb2.maxVertex = center + h;
	}

	b2AABB aabb;
	aabb.minVertex = b2Min(aabb1.minVertex, aabb2.minVertex);
	aabb.maxVertex = b2Max(aabb1.maxVertex, aabb2.maxVertex);

	b2BroadPhase* broadPhase = shape->m_body->m_world->m_broadPhase;
	if (b2BroadPhase_InRange(broadPhase, aabb))
	{
		b2BroadPhase_MoveProxy(broadPhase, shape->m_proxyId, aabb);
	}
	else
	{
		b2Body_Freeze(shape->m_body);
	}
}

b2Vec2 b2PolyShape_Support(const b2Shape *shape, b2Vec2 d)
{
	b2PolyShape *polyShape = (b2PolyShape *)shape;
	b2Vec2 dLocal = b2MulT(shape->m_R, d);

	int32 bestIndex = 0;
	float64 bestValue = b2Dot(polyShape->m_coreVertices[0], dLocal);
	for (int32 i = 1; i < polyShape->m_vertexCount; ++i)
	{
		float64 value = b2Dot(polyShape->m_coreVertices[i], dLocal);
		if (value > bestValue)
		{
			bestIndex = i;
			bestValue = value;
		}
	}

	return shape->m_position + b2Mul(shape->m_R, polyShape->m_coreVertices[bestIndex]);
}

bool b2PolyShape_TestPoint(b2Shape *shape, b2Vec2 p)
{
	b2PolyShape *polyShape = (b2PolyShape *)shape;
	b2Vec2 pLocal = b2MulT(shape->m_R, p - shape->m_position);

	for (int32 i = 0; i < polyShape->m_vertexCount; ++i)
	{
		float64 dot = b2Dot(polyShape->m_normals[i], pLocal - polyShape->m_vertices[i]);
		if (dot > 0.0)
		{
			return false;
		}
	}

	return true;
}

void b2PolyShape_ResetProxy(b2Shape *shape, b2BroadPhase* broadPhase)
{
	b2PolyShape *polyShape = (b2PolyShape *)shape;
	if (shape->m_proxyId == b2_nullProxy)
	{
		return;
	}

	b2BroadPhase_GetProxy(broadPhase, shape->m_proxyId);

	b2BroadPhase_DestroyProxy(broadPhase, shape->m_proxyId);

	b2Mat22 R = b2Mul(shape->m_R, polyShape->m_localOBB.R);
	b2Mat22 absR = b2Abs(R);
	b2Vec2 h = b2Mul(absR, polyShape->m_localOBB.extents);
	b2Vec2 position = shape->m_position + b2Mul(shape->m_R, polyShape->m_localOBB.center);
	b2AABB aabb;
	aabb.minVertex = position - h;
	aabb.maxVertex = position + h;

	if (b2BroadPhase_InRange(broadPhase, aabb))
	{
		shape->m_proxyId = b2BroadPhase_CreateProxy(broadPhase, aabb, shape);
	}
	else
	{
		shape->m_proxyId = b2_nullProxy;
	}

	if (shape->m_proxyId == b2_nullProxy)
	{
		b2Body_Freeze(shape->m_body);
	}
}


