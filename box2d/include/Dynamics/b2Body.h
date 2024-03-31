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

#ifndef B2_BODY_H
#define B2_BODY_H

#include "../Common/b2Math.h"
#include "../Dynamics/Joints/b2Joint.h"
#include "../Collision/b2Shape.h"

#include <cstring>
#include <memory>

class b2Joint;
class b2Contact;
struct b2World;
struct b2JointNode;
struct b2ContactNode;

struct b2BodyDef
{
	void* userData;
	b2ShapeDef* shapes[b2_maxShapesPerBody];
	b2Vec2 position;
	float64 rotation;
	b2Vec2 linearVelocity;
	float64 angularVelocity;
	float64 linearDamping;
	float64 angularDamping;
	bool allowSleep;
	bool isSleeping;
	bool preventRotation;
};

static void b2BodyDef_ctor(b2BodyDef *def)
{
	def->userData = NULL;
	memset(def->shapes, 0, sizeof(def->shapes));
	b2Vec2_Set(&def->position, 0.0, 0.0);
	def->rotation = 0.0;
	b2Vec2_Set(&def->linearVelocity, 0.0, 0.0);
	def->angularVelocity = 0.0;
	def->linearDamping = 0.0;
	def->angularDamping = 0.0;
	def->allowSleep = true;
	def->isSleeping = false;
	def->preventRotation = false;
}

// A rigid body. Internal computation are done in terms
// of the center of mass position. The center of mass may
// be offset from the body's origin.
struct b2Body
{
	// Is this body static (immovable)?
	bool IsStatic() const;

	// Is this body frozen?
	bool IsFrozen() const;

	// Is this body sleeping (not simulating).
	bool IsSleeping() const;

	// You can disable sleeping on this particular body.
	void AllowSleeping(bool flag);

	// Wake up this body so it will begin simulating.
	void WakeUp();

	// Get the list of all shapes attached to this body.
	b2Shape* GetShapeList();

	// Get the list of all contacts attached to this body.
	b2ContactNode* GetContactList();

	// Get the list of all joints attached to this body.
	b2JointNode* GetJointList();

	// Get the next body in the world's body list.
	b2Body* GetNext();

	void* GetUserData();

	//--------------- Internals Below -------------------

	// m_flags
	enum
	{
		e_staticFlag		= 0x0001,
		e_frozenFlag		= 0x0002,
		e_islandFlag		= 0x0004,
		e_sleepFlag			= 0x0008,
		e_allowSleepFlag	= 0x0010,
		e_destroyFlag		= 0x0020,
	};

	b2Body(const b2BodyDef* bd, b2World* world);
	~b2Body();

	void SynchronizeShapes();
	void QuickSyncShapes();

	// This is used to prevent connected bodies from colliding.
	// It may lie, depending on the collideConnected flag.
	bool IsConnected(const b2Body* other) const;

	// This is called when the child shape has no proxy.
	void Freeze();

	uint32 m_flags;

	b2Vec2 m_position;	// center of mass position
	float64 m_rotation;
	b2Mat22 m_R;

	// Conservative advancement data.
	b2Vec2 m_position0;
	float64 m_rotation0;

	b2Vec2 m_linearVelocity;
	float64 m_angularVelocity;

	b2Vec2 m_force;
	float64 m_torque;

	b2Vec2 m_center;	// local vector from client origin to center of mass

	b2World* m_world;
	b2Body* m_prev;
	b2Body* m_next;

	b2Shape* m_shapeList;
	int32 m_shapeCount;

	b2JointNode* m_jointList;
	b2ContactNode* m_contactList;

	float64 m_mass, m_invMass;
	float64 m_I, m_invI;

	float64 m_linearDamping;
	float64 m_angularDamping;

	float64 m_sleepTime;

	void* m_userData;
};

inline void b2BodyDef_AddShape(b2BodyDef *def, b2ShapeDef* shape)
{
	for (int32 i = 0; i < b2_maxShapesPerBody; ++i)
	{
		if (def->shapes[i] == NULL)
		{
			def->shapes[i] = shape;
			break;
		}
	}
}

// Get the position of the body's origin. The body's origin does not
// necessarily coincide with the center of mass. It depends on how the
// shapes are created.
inline b2Vec2 b2Body_GetOriginPosition(const b2Body *body)
{
	return body->m_position - b2Mul(body->m_R, body->m_center);
}

inline float64 b2Body_GetRotation(const b2Body *body)
{
	return body->m_rotation;
}

inline bool b2Body::IsStatic() const
{
	return (m_flags & e_staticFlag) == e_staticFlag;
}

inline bool b2Body::IsFrozen() const
{
	return (m_flags & e_frozenFlag) == e_frozenFlag;
}

inline bool b2Body::IsSleeping() const
{
	return (m_flags & e_sleepFlag) == e_sleepFlag;
}

inline void b2Body::AllowSleeping(bool flag)
{
	if (flag)
	{
		m_flags |= e_allowSleepFlag;
	}
	else
	{
		m_flags &= ~e_allowSleepFlag;
		WakeUp();
	}
}

inline void b2Body::WakeUp()
{
	m_flags &= ~e_sleepFlag;
	m_sleepTime = 0.0;
}

inline b2Shape* b2Body::GetShapeList()
{
	return m_shapeList;
}

inline b2ContactNode* b2Body::GetContactList()
{
	return m_contactList;
}

inline b2JointNode* b2Body::GetJointList()
{
	return m_jointList;
}

inline b2Body* b2Body::GetNext()
{
	return m_next;
}

inline void* b2Body::GetUserData()
{
	return m_userData;
}

inline bool b2Body::IsConnected(const b2Body* other) const
{
	for (b2JointNode* jn = m_jointList; jn; jn = jn->next)
	{
		if (jn->other == other)
			return jn->joint->m_collideConnected == false;
	}

	return false;
}

#endif
