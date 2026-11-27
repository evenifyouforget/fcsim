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

#include <box2d/b2BlockAllocator.h>
#include <box2d/b2CircleContact.h>

static void b2CircleContact_Evaluate(b2Contact *contact) {
  b2CircleContact *circ_contact = (b2CircleContact *)contact;
  b2CollideCircle(&circ_contact->m_manifold, (b2CircleShape *)contact->m_shape1,
                  (b2CircleShape *)contact->m_shape2, false);

  if (circ_contact->m_manifold.pointCount > 0) {
    contact->m_manifoldCount = 1;
  } else {
    contact->m_manifoldCount = 0;
  }
}

static b2Manifold *b2CircleContact_GetManifolds(b2Contact *contact) {
  b2CircleContact *circ_contact = (b2CircleContact *)contact;
  return &circ_contact->m_manifold;
}

static void b2CircleContact_ctor(b2CircleContact *circ_contact, b2Shape *s1,
                                 b2Shape *s2) {
  b2Contact_ctor(&circ_contact->contact, s1, s2);
  circ_contact->contact.Evaluate = b2CircleContact_Evaluate;
  circ_contact->contact.GetManifolds = b2CircleContact_GetManifolds;
  circ_contact->m_manifold.pointCount = 0;
  circ_contact->m_manifold.points[0].normalImpulse = 0.0;
  circ_contact->m_manifold.points[0].tangentImpulse = 0.0;
}

b2Contact *b2CircleContact_Create(b2Shape *shape1, b2Shape *shape2,
                                  b2BlockAllocator *allocator) {
  b2CircleContact *circ_contact = (b2CircleContact *)b2BlockAllocator_Allocate(
      allocator, sizeof(b2CircleContact));
  b2CircleContact_ctor(circ_contact, shape1, shape2);
  return &circ_contact->contact;
}

void b2CircleContact_Destroy(b2Contact *contact, b2BlockAllocator *allocator) {
  b2BlockAllocator_Free(allocator, contact, sizeof(b2CircleContact));
}
