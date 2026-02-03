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

#ifndef B2_CONTACT_MANAGER_H
#define B2_CONTACT_MANAGER_H

#include <box2d/b2NullContact.h>
#include <box2d/b2PairManager.h>

typedef struct b2World b2World;
struct b2World;

typedef struct b2Contact b2Contact;
struct b2Contact;

typedef struct b2ContactManager b2ContactManager;
struct b2ContactManager {
  b2PairCallback m_pairCallback;

  b2World *m_world;

  // This lets us provide broadphase proxy pair user data for
  // contacts that shouldn't exist.
  b2NullContact m_nullContact;

  bool m_destroyImmediate;
};

#ifdef __cplusplus
extern "C" {
#endif

// Implements PairCallback
void *b2ContactManager_PairAdded(b2PairCallback *callback, void *proxyUserData1,
                                 void *proxyUserData2);

// Implements PairCallback
void b2ContactManager_PairRemoved(b2PairCallback *callback,
                                  void *proxyUserData1, void *proxyUserData2,
                                  void *pairUserData);

static void b2ContactManager_ctor(b2ContactManager *manager) {
  b2NullContact_ctor(&manager->m_nullContact);
  manager->m_world = NULL;
  manager->m_destroyImmediate = false;
  manager->m_pairCallback.PairAdded = b2ContactManager_PairAdded;
  manager->m_pairCallback.PairRemoved = b2ContactManager_PairRemoved;
}

void b2ContactManager_Collide(b2ContactManager *manager);

void b2ContactManager_CleanContactList(b2ContactManager *manager);

#ifdef __cplusplus
}
#endif

#endif
