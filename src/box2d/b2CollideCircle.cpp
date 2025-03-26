/*
* Copyright (c) 2007 Erin Catto http://www.gphysics.com
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
#include <box2d/b2Collision.h>
#include <box2d/b2Shape.h>

void b2CollideCircle(b2Manifold* manifold, b2CircleShape* circle1, b2CircleShape* circle2, bool conservative)
{
	manifold->pointCount = 0;

	b2Vec2 d = circle2->m_shape.m_position - circle1->m_shape.m_position;
	float64 distSqr = b2Dot(d, d);
	float64 radiusSum = circle1->m_radius + circle2->m_radius;
	if (distSqr > radiusSum * radiusSum && conservative == false)
	{
		return;
	}

	float64 separation;
	if (distSqr < MIN_VALUE)
	{
		separation = -radiusSum;
		b2Vec2_Set(&manifold->normal, 0.0, 1.0);
	}
	else
	{
		float64 dist = sqrt(distSqr);
		separation = dist - radiusSum;
		float64 a = 1.0 / dist;
		manifold->normal.x = a * d.x;
		manifold->normal.y = a * d.y;
	}

	manifold->pointCount = 1;
	manifold->points[0].id.key = 0;
	manifold->points[0].separation = separation;
	manifold->points[0].position = circle2->m_shape.m_position - circle2->m_radius * manifold->normal;
}

void b2CollidePolyAndCircle(b2Manifold* manifold, const b2PolyShape* poly, const b2CircleShape* circle, UNUSED bool conservative)
{
	manifold->pointCount = 0;

	// Compute circle position in the frame of the polygon.
	b2Vec2 xLocal = b2MulT(poly->m_shape.m_R, circle->m_shape.m_position - poly->m_shape.m_position);

	// Find the min separating edge.
	int32 normalIndex = 0;
	float64 separation = -DBL_MAX;
	const float64 radius = circle->m_radius;
	for (int32 i = 0; i < poly->m_vertexCount; ++i)
	{
		float64 s = b2Dot(poly->m_normals[i], xLocal - poly->m_vertices[i]);
		if (s > radius)
		{
			// Early out.
			return;
		}

		if (s > separation)
		{
			separation = s;
			normalIndex = i;
		}
	}

	// If the center is inside the polygon ...
	if (separation < MIN_VALUE)
	{
		manifold->pointCount = 1;
		manifold->normal = b2Mul(poly->m_shape.m_R, poly->m_normals[normalIndex]);
		manifold->points[0].id.features.incidentEdge = (uint8)normalIndex;
		manifold->points[0].id.features.incidentVertex = b2_nullFeature;
		manifold->points[0].id.features.referenceFace = b2_nullFeature;
		manifold->points[0].id.features.flip = 0;
		manifold->points[0].position = circle->m_shape.m_position - radius * manifold->normal;
		manifold->points[0].separation = separation - radius;
		return;
	}

	// Project the circle center onto the edge segment.
	int32 vertIndex1 = normalIndex;
	int32 vertIndex2 = vertIndex1 + 1 < poly->m_vertexCount ? vertIndex1 + 1 : 0;
	b2Vec2 e = poly->m_vertices[vertIndex2] - poly->m_vertices[vertIndex1];
	float64 length = b2Vec2_Length(&e);
	e.x /= length;
	e.y /= length;

	// If the edge length is zero ...
	if (length < MIN_VALUE)
	{
		b2Vec2 d = xLocal - poly->m_vertices[vertIndex1];
		float64 dist = b2Vec2_Length(&d);
		d.x /= dist;
		d.y /= dist;
		if (dist > radius)
		{
			return;
		}

		manifold->pointCount = 1;
		manifold->normal = b2Mul(poly->m_shape.m_R, d);
		manifold->points[0].id.features.incidentEdge = b2_nullFeature;
		manifold->points[0].id.features.incidentVertex = (uint8)vertIndex1;
		manifold->points[0].id.features.referenceFace = b2_nullFeature;
		manifold->points[0].id.features.flip = 0;
		manifold->points[0].position = circle->m_shape.m_position - radius * manifold->normal;
		manifold->points[0].separation = dist - radius;
		return;
	}

	// Project the center onto the edge.
	float64 u = b2Dot(xLocal - poly->m_vertices[vertIndex1], e);
	manifold->points[0].id.features.incidentEdge = b2_nullFeature;
	manifold->points[0].id.features.incidentVertex = b2_nullFeature;
	manifold->points[0].id.features.referenceFace = b2_nullFeature;
	manifold->points[0].id.features.flip = 0;
	b2Vec2 p;
	if (u <= 0.0)
	{
		p = poly->m_vertices[vertIndex1];
		manifold->points[0].id.features.incidentVertex = (uint8)vertIndex1;
	}
	else if (u >= length)
	{
		p = poly->m_vertices[vertIndex2];
		manifold->points[0].id.features.incidentVertex = (uint8)vertIndex2;
	}
	else
	{
		p = poly->m_vertices[vertIndex1] + u * e;
		manifold->points[0].id.features.incidentEdge = (uint8)vertIndex1;
	}

	b2Vec2 d = xLocal - p;
	float64 dist = b2Vec2_Length(&d);
	d.x /= dist;
	d.y /= dist;
	if (dist > radius)
	{
		return;
	}

	manifold->pointCount = 1;
	manifold->normal = b2Mul(poly->m_shape.m_R, d);
	manifold->points[0].position = circle->m_shape.m_position - radius * manifold->normal;
	manifold->points[0].separation = dist - radius;
}
