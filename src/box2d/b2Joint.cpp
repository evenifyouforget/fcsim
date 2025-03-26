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

#include <box2d/b2Joint.h>
#include <box2d/b2RevoluteJoint.h>
#include <box2d/b2Body.h>
#include <box2d/b2World.h>
#include <box2d/b2BlockAllocator.h>
#include <box2d/b2BroadPhase.h>

b2Joint* b2Joint_Create(const b2JointDef* def, b2BlockAllocator* allocator)
{
	b2Joint* joint = NULL;

	switch (def->type)
	{
	case e_revoluteJoint:
		{
			b2RevoluteJoint* revoluteJoint = (b2RevoluteJoint *)b2BlockAllocator_Allocate(allocator, sizeof(b2RevoluteJoint));
			b2RevoluteJoint_ctor(revoluteJoint, (b2RevoluteJointDef*)def);
			joint = &revoluteJoint->m_joint;
		}
		break;

	default:break;
	}

	return joint;
}

void b2Joint_Destroy(b2Joint* joint, b2BlockAllocator* allocator)
{
	switch (joint->m_type)
	{
	case e_revoluteJoint:
		b2BlockAllocator_Free(allocator, joint, sizeof(b2RevoluteJoint));
		break;

	default:break;
	}
}

void b2Joint_ctor(b2Joint *joint, const b2JointDef* def)
{
	joint->m_type = def->type;
	joint->m_prev = NULL;
	joint->m_next = NULL;
	joint->m_body1 = def->body1;
	joint->m_body2 = def->body2;
	joint->m_collideConnected = def->collideConnected;
	joint->m_islandFlag = false;
	joint->m_userData = def->userData;
}
