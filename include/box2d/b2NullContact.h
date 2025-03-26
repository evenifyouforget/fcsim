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

#ifndef B2_NULL_CONTACT_H
#define B2_NULL_CONTACT_H

#include <box2d/b2Contact.h>
#include <stddef.h>

typedef struct b2NullContact b2NullContact;
struct b2NullContact
{
	b2Contact contact;
};

static void b2NullContact_Evaluate(UNUSED struct b2Contact *contact)
{
}

static b2Manifold* b2NullContact_GetManifolds(UNUSED struct b2Contact *contact)
{
	return NULL;
}

static void b2NullContact_ctor(b2NullContact *null_contact)
{
	null_contact->contact.Evaluate = b2NullContact_Evaluate;
	null_contact->contact.GetManifolds = b2NullContact_GetManifolds;
}

#endif
