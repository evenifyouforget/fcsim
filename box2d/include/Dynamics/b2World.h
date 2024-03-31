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

#include "../Common/b2Math.h"
#include "../Common/b2BlockAllocator.h"
#include "../Common/b2StackAllocator.h"
#include "b2ContactManager.h"
#include "b2WorldCallbacks.h"

struct b2AABB;
struct b2BodyDef;
struct b2JointDef;
class b2Body;
class b2Joint;
class b2Shape;
class b2Contact;
class b2BroadPhase;

struct b2TimeStep
{
	float64 dt;			// time step
	float64 inv_dt;		// inverse time step (0 if dt == 0).
	int32 iterations;
};

struct b2World
{
	// You can use these to iterate over all the bodies, joints, and contacts.
	b2Body* GetBodyList();
	b2Joint* GetJointList();
	b2Contact* GetContactList();

	//--------------- Internals Below -------------------

	void CleanBodyList();

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

	b2WorldListener* m_listener;
	b2CollisionFilter* m_filter;

	int32 m_positionIterationCount;
};

void b2World_ctor(b2World *world, const b2AABB& worldAABB, const b2Vec2& gravity, bool doSleep);

void b2World_dtor(b2World *world);
	
// Register a world listener to receive important events that can
// help prevent your code from crashing.
void b2World_SetListener(b2World *world, b2WorldListener* listener);

// Register a collision filter to provide specific control over collision.
// Otherwise the default filter is used (b2CollisionFilter).
void b2World_SetFilter(b2World *world, b2CollisionFilter* filter);

// Create and destroy rigid bodies. Destruction is deferred until the
// the next call to Step. This is done so that bodies may be destroyed
// while you iterate through the contact list.
b2Body* b2World_CreateBody(b2World *world, const b2BodyDef* def);
void b2World_DestroyBody(b2World *world, b2Body* body);

b2Joint* b2World_CreateJoint(b2World *world, const b2JointDef* def);

void b2World_DestroyJoint(b2World *world, b2Joint* joint);

void b2World_Step(b2World *world, float64 timeStep, int32 iterations);

inline b2Body* b2World::GetBodyList()
{
	return m_bodyList;
}

inline b2Joint* b2World::GetJointList()
{
	return m_jointList;
}

inline b2Contact* b2World::GetContactList()
{
	return m_contactList;
}

extern int32 b2World_s_enablePositionCorrection;
extern int32 b2World_s_enableWarmStarting;

#endif
