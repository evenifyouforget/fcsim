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

#ifndef CONTACT_H
#define CONTACT_H

#include <box2d/b2Collision.h>
#include <box2d/b2Shape.h>

typedef struct b2Body b2Body;
typedef struct b2Contact b2Contact;
typedef struct b2World b2World;
typedef struct b2BlockAllocator b2BlockAllocator;
struct b2Body;
struct b2Contact;
struct b2World;
struct b2BlockAllocator;

typedef b2Contact *b2ContactCreateFcn(b2Shape *shape1, b2Shape *shape2,
                                      b2BlockAllocator *allocator);
typedef void b2ContactDestroyFcn(b2Contact *contact,
                                 b2BlockAllocator *allocator);

typedef struct b2ContactNode b2ContactNode;
struct b2ContactNode {
  b2Body *other;
  b2Contact *contact;
  b2ContactNode *prev;
  b2ContactNode *next;
};

typedef struct b2ContactRegister b2ContactRegister;
struct b2ContactRegister {
  b2ContactCreateFcn *createFcn;
  b2ContactDestroyFcn *destroyFcn;
  bool primary;
};

enum {
  b2Contact_e_islandFlag = 0x0001,
  b2Contact_e_destroyFlag = 0x0002,
};

typedef struct b2Contact b2Contact;
struct b2Contact {
  b2Manifold *(*GetManifolds)(struct b2Contact *contact);
  void (*Evaluate)(struct b2Contact *contact);

  uint32 m_flags;

  // World pool and list pointers.
  b2Contact *m_prev;
  b2Contact *m_next;

  // Nodes for connecting bodies.
  b2ContactNode m_node1;
  b2ContactNode m_node2;

  b2Shape *m_shape1;
  b2Shape *m_shape2;

  int32 m_manifoldCount;

  // Combined friction
  float64 m_friction;
  float64 m_restitution;
};

#ifdef __cplusplus
extern "C" {
#endif

b2Contact *b2Contact_Create(b2Shape *shape1, b2Shape *shape2,
                            b2BlockAllocator *allocator);
void b2Contact_Destroy(b2Contact *contact, b2BlockAllocator *allocator);

void b2Contact_ctor(b2Contact *contact, b2Shape *s1, b2Shape *s2);

#ifdef __cplusplus
}
#endif

#endif
