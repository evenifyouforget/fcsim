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
#include <box2d/b2ContactManager.h>
#include <box2d/b2World.h>
#include <box2d/b2Body.h>

static void b2ContactManager_DestroyContact(b2ContactManager *manager, b2Contact* c);

// This is a callback from the broadphase when two AABB proxies begin
// to overlap. We create a b2Contact to manage the narrow phase.
void* b2ContactManager_PairAdded(b2PairCallback *callback, void* proxyUserData1, void* proxyUserData2)
{
	b2ContactManager *manager = (b2ContactManager *)callback;
	b2Shape* shape1 = (b2Shape*)proxyUserData1;
	b2Shape* shape2 = (b2Shape*)proxyUserData2;

	b2Body* body1 = shape1->m_body;
	b2Body* body2 = shape2->m_body;

	if (b2Body_IsStatic(body1) && b2Body_IsStatic(body2))
	{
		return &manager->m_nullContact;
	}

	if (shape1->m_body == shape2->m_body)
	{
		return &manager->m_nullContact;
	}

	if (b2Body_IsConnected(body2, body1))
	{
		return &manager->m_nullContact;
	}

	if (manager->m_world->m_filter != NULL && manager->m_world->m_filter(shape1, shape2) == false)
	{
		return &manager->m_nullContact;
	}

	// Ensure that body2 is dynamic (body1 is static or dynamic).
	if (body2->m_invMass == 0.0)
	{
		b2Swap(shape1, shape2);
		b2Swap(body1, body2);
	}

	// Call the factory.
	b2Contact* contact = b2Contact_Create(shape1, shape2, &manager->m_world->m_blockAllocator);

	if (contact == NULL)
	{
		return &manager->m_nullContact;
	}
	else
	{
		// Insert into the world.
		contact->m_prev = NULL;
		contact->m_next = manager->m_world->m_contactList;
		if (manager->m_world->m_contactList != NULL)
		{
			manager->m_world->m_contactList->m_prev = contact;
		}
		manager->m_world->m_contactList = contact;
		++manager->m_world->m_contactCount;
	}

	return contact;
}

// This is a callback from the broadphase when two AABB proxies cease
// to overlap. We destroy the b2Contact.
void b2ContactManager_PairRemoved(b2PairCallback *callback, UNUSED void* proxyUserData1, UNUSED void* proxyUserData2, void* pairUserData)
{
	b2ContactManager *manager = (b2ContactManager *)callback;
	NOT_USED(proxyUserData1);
	NOT_USED(proxyUserData2);

	if (pairUserData == NULL)
	{
		return;
	}

	b2Contact* c = (b2Contact*)pairUserData;
	if (c != &manager->m_nullContact.contact)
	{
		if (manager->m_destroyImmediate == true)
		{
			b2ContactManager_DestroyContact(manager, c);
			c = NULL;
		}
		else
		{
			c->m_flags |= b2Contact_e_destroyFlag;
		}
	}
}

static void b2ContactManager_DestroyContact(b2ContactManager *manager, b2Contact* c)
{
	// Remove from the world.
	if (c->m_prev)
	{
		c->m_prev->m_next = c->m_next;
	}

	if (c->m_next)
	{
		c->m_next->m_prev = c->m_prev;
	}

	if (c == manager->m_world->m_contactList)
	{
		manager->m_world->m_contactList = c->m_next;
	}

	// If there are contact points, then disconnect from the island graph.
	if (c->m_manifoldCount > 0)
	{
		b2Body* body1 = c->m_shape1->m_body;
		b2Body* body2 = c->m_shape2->m_body;

		// Wake up touching bodies.
		b2Body_WakeUp(body1);
		b2Body_WakeUp(body2);

		// Remove from body 1
		if (c->m_node1.prev)
		{
			c->m_node1.prev->next = c->m_node1.next;
		}

		if (c->m_node1.next)
		{
			c->m_node1.next->prev = c->m_node1.prev;
		}

		if (&c->m_node1 == body1->m_contactList)
		{
			body1->m_contactList = c->m_node1.next;
		}

		c->m_node1.prev = NULL;
		c->m_node1.next = NULL;

		// Remove from body 2
		if (c->m_node2.prev)
		{
			c->m_node2.prev->next = c->m_node2.next;
		}

		if (c->m_node2.next)
		{
			c->m_node2.next->prev = c->m_node2.prev;
		}

		if (&c->m_node2 == body2->m_contactList)
		{
			body2->m_contactList = c->m_node2.next;
		}

		c->m_node2.prev = NULL;
		c->m_node2.next = NULL;
	}

	// Call the factory.
	b2Contact_Destroy(c, &manager->m_world->m_blockAllocator);
	--manager->m_world->m_contactCount;
}

// Destroy any contacts marked for deferred destruction.
void b2ContactManager_CleanContactList(b2ContactManager *manager)
{
	b2Contact* c = manager->m_world->m_contactList;
	while (c != NULL)
	{
		b2Contact* c0 = c;
		c = c->m_next;

		if (c0->m_flags & b2Contact_e_destroyFlag)
		{
			b2ContactManager_DestroyContact(manager, c0);
			c0 = NULL;
		}
	}
}

// This is the top level collision call for the time step. Here
// all the narrow phase collision is processed for the world
// contact list.
void b2ContactManager_Collide(b2ContactManager *manager)
{
	for (b2Contact* c = manager->m_world->m_contactList; c; c = c->m_next)
	{
		if (b2Body_IsSleeping(c->m_shape1->m_body) &&
			b2Body_IsSleeping(c->m_shape2->m_body))
		{
			continue;
		}

		int32 oldCount = c->m_manifoldCount;
		c->Evaluate(c);

		int32 newCount = c->m_manifoldCount;

		if (oldCount == 0 && newCount > 0)
		{
			// Connect to island graph.
			b2Body* body1 = c->m_shape1->m_body;
			b2Body* body2 = c->m_shape2->m_body;

			// Connect to body 1
			c->m_node1.contact = c;
			c->m_node1.other = body2;

			c->m_node1.prev = NULL;
			c->m_node1.next = body1->m_contactList;
			if (c->m_node1.next != NULL)
			{
				c->m_node1.next->prev = &c->m_node1;
			}
			body1->m_contactList = &c->m_node1;

			// Connect to body 2
			c->m_node2.contact = c;
			c->m_node2.other = body1;

			c->m_node2.prev = NULL;
			c->m_node2.next = body2->m_contactList;
			if (c->m_node2.next != NULL)
			{
				c->m_node2.next->prev = &c->m_node2;
			}
			body2->m_contactList = &c->m_node2;
		}
		else if (oldCount > 0 && newCount == 0)
		{
			// Disconnect from island graph.
			b2Body* body1 = c->m_shape1->m_body;
			b2Body* body2 = c->m_shape2->m_body;

			// Remove from body 1
			if (c->m_node1.prev)
			{
				c->m_node1.prev->next = c->m_node1.next;
			}

			if (c->m_node1.next)
			{
				c->m_node1.next->prev = c->m_node1.prev;
			}

			if (&c->m_node1 == body1->m_contactList)
			{
				body1->m_contactList = c->m_node1.next;
			}

			c->m_node1.prev = NULL;
			c->m_node1.next = NULL;

			// Remove from body 2
			if (c->m_node2.prev)
			{
				c->m_node2.prev->next = c->m_node2.next;
			}

			if (c->m_node2.next)
			{
				c->m_node2.next->prev = c->m_node2.prev;
			}

			if (&c->m_node2 == body2->m_contactList)
			{
				body2->m_contactList = c->m_node2.next;
			}

			c->m_node2.prev = NULL;
			c->m_node2.next = NULL;
		}
	}
}