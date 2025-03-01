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

#ifndef B2_WORLD_H
#define B2_WORLD_H

#include <box2d/b2Vec.h>
#include <box2d/b2BlockAllocator.h>
#include <box2d/b2StackAllocator.h>
#include <box2d/b2ContactManager.h>

typedef struct b2AABB b2AABB;
typedef struct b2BodyDef b2BodyDef;
typedef struct b2JointDef b2JointDef;
typedef struct b2Body b2Body;
typedef struct b2Joint b2Joint;
typedef struct b2Shape b2Shape;
typedef struct b2Contact b2Contact;
typedef struct b2BroadPhase b2BroadPhase;
struct b2AABB;
struct b2BodyDef;
struct b2JointDef;
struct b2Body;
struct b2Joint;
struct b2Shape;
struct b2Contact;
struct b2BroadPhase;

typedef bool (*b2CollisionFilter)(b2Shape* shape1, b2Shape* shape2);

typedef struct b2TimeStep b2TimeStep;
struct b2TimeStep
{
	float64 dt;			// time step
	float64 inv_dt;		// inverse time step (0 if dt == 0).
	int32 iterations;
};

typedef struct b2World b2World;
struct b2World
{
	b2BlockAllocator m_blockAllocator;
	b2StackAllocator m_stackAllocator;

	b2BroadPhase* m_broadPhase;
	b2ContactManager m_contactManager;

	b2Body* m_bodyList;
	b2Contact* m_contactList;
	b2Joint* m_jointList;

	int32 m_bodyCount;
	int32 m_contactCount;
	int32 m_jointCount;

	// These bodies will be destroyed at the next time step.
	b2Body* m_bodyDestroyList;

	b2Vec2 m_gravity;
	bool m_allowSleep;

	b2Body* m_groundBody;

	b2CollisionFilter m_filter;
};

#ifdef __cplusplus
extern "C" {
#endif

void b2World_ctor(b2World *world, const b2AABB* worldAABB, b2Vec2 gravity, bool doSleep);

void b2World_dtor(b2World *world);

// Register a collision filter to provide specific control over collision.
// Otherwise the default filter is used (b2CollisionFilter).
void b2World_SetFilter(b2World *world, b2CollisionFilter filter);

// Create and destroy rigid bodies. Destruction is deferred until the
// the next call to Step. This is done so that bodies may be destroyed
// while you iterate through the contact list.
b2Body* b2World_CreateBody(b2World *world, const b2BodyDef* def);
void b2World_DestroyBody(b2World *world, b2Body* body);

b2Joint* b2World_CreateJoint(b2World *world, const b2JointDef* def);

void b2World_DestroyJoint(b2World *world, b2Joint* joint);

void b2World_Step(b2World *world, float64 timeStep, int32 iterations);

static inline b2Joint* b2World_GetJointList(b2World *world)
{
	return world->m_jointList;
}

extern int32 b2World_s_enablePositionCorrection;
extern int32 b2World_s_enableWarmStarting;

#ifdef __cplusplus
}
#endif

#endif
