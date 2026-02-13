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
#include <box2d/b2Body.h>
#include <box2d/b2CMath.h>
#include <box2d/b2CircleContact.h>
#include <box2d/b2Collision.h>
#include <box2d/b2Contact.h>
#include <box2d/b2PolyAndCircleContact.h>
#include <box2d/b2PolyContact.h>
#include <box2d/b2Shape.h>
#include <box2d/b2World.h>

static b2ContactRegister s_registers[e_shapeTypeCount][e_shapeTypeCount];
static bool s_initialized = false;

static void AddType(b2ContactCreateFcn *createFcn,
                    b2ContactDestroyFcn *destoryFcn, b2ShapeType type1,
                    b2ShapeType type2);

static void InitializeRegisters() {
  AddType(b2CircleContact_Create, b2CircleContact_Destroy, e_circleShape,
          e_circleShape);
  AddType(b2PolyAndCircleContact_Create, b2PolyAndCircleContact_Destroy,
          e_polyShape, e_circleShape);
  AddType(b2PolyContact_Create, b2PolyContact_Destroy, e_polyShape,
          e_polyShape);
}

static void AddType(b2ContactCreateFcn *createFcn,
                    b2ContactDestroyFcn *destoryFcn, b2ShapeType type1,
                    b2ShapeType type2) {
  s_registers[type1][type2].createFcn = createFcn;
  s_registers[type1][type2].destroyFcn = destoryFcn;
  s_registers[type1][type2].primary = true;

  if (type1 != type2) {
    s_registers[type2][type1].createFcn = createFcn;
    s_registers[type2][type1].destroyFcn = destoryFcn;
    s_registers[type2][type1].primary = false;
  }
}

b2Contact *b2Contact_Create(b2Shape *shape1, b2Shape *shape2,
                            b2BlockAllocator *allocator) {
  if (s_initialized == false) {
    InitializeRegisters();
    s_initialized = true;
  }

  b2ShapeType type1 = shape1->m_type;
  b2ShapeType type2 = shape2->m_type;

  b2ContactCreateFcn *createFcn = s_registers[type1][type2].createFcn;
  if (createFcn) {
    if (s_registers[type1][type2].primary) {
      return createFcn(shape1, shape2, allocator);
    } else {
      b2Contact *c = createFcn(shape2, shape1, allocator);
      for (int32 i = 0; i < c->m_manifoldCount; ++i) {
        b2Manifold *m = c->GetManifolds(c) + i;
        m->normal = b2Vec2_neg(m->normal);
      }
      return c;
    }
  } else {
    return NULL;
  }
}

void b2Contact_Destroy(b2Contact *contact, b2BlockAllocator *allocator) {
  if (contact->m_manifoldCount > 0) {
    b2Body_WakeUp(contact->m_shape1->m_body);
    b2Body_WakeUp(contact->m_shape2->m_body);
  }

  b2ShapeType type1 = contact->m_shape1->m_type;
  b2ShapeType type2 = contact->m_shape2->m_type;

  b2ContactDestroyFcn *destroyFcn = s_registers[type1][type2].destroyFcn;
  destroyFcn(contact, allocator);
}

void b2Contact_ctor(b2Contact *contact, b2Shape *s1, b2Shape *s2) {
  contact->m_flags = 0;

  contact->m_shape1 = s1;
  contact->m_shape2 = s2;

  contact->m_manifoldCount = 0;

  contact->m_friction =
      sqrt(contact->m_shape1->m_friction * contact->m_shape2->m_friction);
  contact->m_restitution =
      b2Max(contact->m_shape1->m_restitution, contact->m_shape2->m_restitution);
  contact->m_prev = NULL;
  contact->m_next = NULL;

  contact->m_node1.contact = NULL;
  contact->m_node1.prev = NULL;
  contact->m_node1.next = NULL;
  contact->m_node1.other = NULL;

  contact->m_node2.contact = NULL;
  contact->m_node2.prev = NULL;
  contact->m_node2.next = NULL;
  contact->m_node2.other = NULL;
}
