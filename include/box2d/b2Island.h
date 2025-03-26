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

#ifndef B2_ISLAND_H
#define B2_ISLAND_H

#include <box2d/b2Math.h>

struct b2StackAllocator;
struct b2Contact;
struct b2Body;
struct b2Joint;
struct b2TimeStep;

struct b2Island
{
	b2StackAllocator* m_allocator;

	b2Body** m_bodies;
	b2Contact** m_contacts;
	b2Joint** m_joints;

	int32 m_bodyCount;
	int32 m_jointCount;
	int32 m_contactCount;

	int32 m_bodyCapacity;
	int32 m_contactCapacity;
	int32 m_jointCapacity;

	float64 m_positionError;
};

void b2Island_ctor(b2Island *island, int32 bodyCapacity, int32 contactCapacity, int32 jointCapacity, b2StackAllocator* allocator);
void b2Island_dtor(b2Island *island);

void b2Island_Clear(b2Island *island);

void b2Island_Solve(b2Island *island, const b2TimeStep* step, const b2Vec2& gravity);


void b2Island_UpdateSleep(b2Island *island, float64 dt);

static inline void b2Island_AddBody(b2Island *island, b2Body* body)
{
	b2Assert(island->m_bodyCount < island->m_bodyCapacity);
	island->m_bodies[island->m_bodyCount++] = body;
}

static inline void b2Island_AddContact(b2Island *island, b2Contact* contact)
{
	b2Assert(island->m_contactCount < island->m_contactCapacity);
	island->m_contacts[island->m_contactCount++] = contact;
}

static inline void b2Island_AddJoint(b2Island *island, b2Joint* joint)
{
	b2Assert(island->m_jointCount < island->m_jointCapacity);
	island->m_joints[island->m_jointCount++] = joint;
}

#endif
