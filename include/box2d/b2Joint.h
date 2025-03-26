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

#ifndef JOINT_H
#define JOINT_H

#include <box2d/b2Vec.h>
#include <stddef.h>

typedef struct b2Body b2Body;
struct b2Body;

typedef struct b2Joint b2Joint;
struct b2Joint;

typedef struct b2TimeStep b2TimeStep;
struct b2TimeStep;

typedef struct b2BlockAllocator b2BlockAllocator;
struct b2BlockAllocator;

enum b2JointType
{
	e_unknownJoint,
	e_revoluteJoint,
	e_prismaticJoint,
	e_distanceJoint,
	e_pulleyJoint,
	e_mouseJoint,
	e_gearJoint
};
typedef enum b2JointType b2JointType;

enum b2LimitState
{
	e_inactiveLimit,
	e_atLowerLimit,
	e_atUpperLimit,
	e_equalLimits
};
typedef enum b2LimitState b2LimitState;

typedef struct b2JointNode b2JointNode;
struct b2JointNode
{
	b2Body* other;
	b2Joint* joint;
	b2JointNode* prev;
	b2JointNode* next;
};

typedef struct b2JointDef b2JointDef;
struct b2JointDef
{
	b2JointType type;
	void* userData;
	b2Body* body1;
	b2Body* body2;
	bool collideConnected;
};

inline void b2JointDef_ctor(b2JointDef *joint_def)
{
	joint_def->type = e_unknownJoint;
	joint_def->userData = NULL;
	joint_def->body1 = NULL;
	joint_def->body2 = NULL;
	joint_def->collideConnected = false;
}

typedef struct b2Joint b2Joint;
struct b2Joint
{
	b2Vec2 (*GetAnchor1)(b2Joint *joint);
	b2Vec2 (*GetAnchor2)(b2Joint *joint);

	b2Vec2 (*GetReactionForce)(b2Joint *joint, float64 invTimeStep);
	float64 (*GetReactionTorque)(b2Joint *joint, float64 invTimeStep);

	void (*PrepareVelocitySolver)(b2Joint *joint);
	void (*SolveVelocityConstraints)(b2Joint *joint, const b2TimeStep* step);

	bool (*SolvePositionConstraints)(b2Joint *joint);

	b2JointType m_type;
	b2Joint* m_prev;
	b2Joint* m_next;
	b2JointNode m_node1;
	b2JointNode m_node2;
	b2Body* m_body1;
	b2Body* m_body2;

	bool m_islandFlag;
	bool m_collideConnected;

	void* m_userData;
};

#ifdef __cplusplus
extern "C" {
#endif

b2Joint* b2Joint_Create(const b2JointDef* def, b2BlockAllocator* allocator);

void b2Joint_Destroy(b2Joint* joint, b2BlockAllocator* allocator);

void b2Joint_ctor(b2Joint *joint, const b2JointDef* def);

#ifdef __cplusplus
}
#endif

#endif
