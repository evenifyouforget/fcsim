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

#ifndef CIRCLE_CONTACT_H
#define CIRCLE_CONTACT_H

#include <box2d/b2Collision.h>
#include <box2d/b2Contact.h>

typedef struct b2BlockAllocator b2BlockAllocator;
struct b2BlockAllocator;

typedef struct b2CircleContact b2CircleContact;
struct b2CircleContact {
  b2Contact contact;
  b2Manifold m_manifold;
};

#ifdef __cplusplus
extern "C" {
#endif

b2Contact *b2CircleContact_Create(b2Shape *shape1, b2Shape *shape2,
                                  b2BlockAllocator *allocator);
void b2CircleContact_Destroy(b2Contact *contact, b2BlockAllocator *allocator);

#ifdef __cplusplus
}
#endif

#endif
