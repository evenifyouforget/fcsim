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

// The pair manager is used by the broad-phase to quickly add/remove/find pairs
// of overlapping proxies. It is based closely on code provided by Pierre Terdiman.
// http://www.codercorner.com/IncrementalSAP.txt

#ifndef B2_PAIR_MANAGER_H
#define B2_PAIR_MANAGER_H

#include <box2d/b2Settings.h>
#include <box2d/b2Vec.h>

#include <limits.h>

typedef struct b2BroadPhase b2BroadPhase;
struct b2BroadPhase;
typedef struct b2Proxy b2Proxy;
struct b2Proxy;

#define b2_nullPair USHRT_MAX
#define b2_nullProxy USHRT_MAX
#define b2_tableCapacity b2_maxPairs	// must be a power of two
#define b2_tableMask (b2_tableCapacity - 1)

enum
{
	b2Pair_e_pairBuffered	= 0x0001,
	b2Pair_e_pairRemoved	= 0x0002,
	b2Pair_e_pairFinal		= 0x0004,
};

typedef struct b2Pair b2Pair;
struct b2Pair
{
	void* userData;
	uint16 proxyId1;
	uint16 proxyId2;
	uint16 next;
	uint16 status;
};

static inline void b2Pair_SetBuffered(b2Pair *pair)
{
	pair->status |= b2Pair_e_pairBuffered;

}
static inline void b2Pair_ClearBuffered(b2Pair *pair)
{
	pair->status &= ~b2Pair_e_pairBuffered;
}

static inline bool b2Pair_IsBuffered(b2Pair *pair)
{
	return (pair->status & b2Pair_e_pairBuffered) == b2Pair_e_pairBuffered;
}

static inline void b2Pair_SetRemoved(b2Pair *pair)
{
	pair->status |= b2Pair_e_pairRemoved;
}

static inline void b2Pair_ClearRemoved(b2Pair *pair)
{
	pair->status &= ~b2Pair_e_pairRemoved;
}

static inline bool b2Pair_IsRemoved(b2Pair *pair)
{
	return (pair->status & b2Pair_e_pairRemoved) == b2Pair_e_pairRemoved;
}

static inline void b2Pair_SetFinal(b2Pair *pair)
{
	pair->status |= b2Pair_e_pairFinal;
}

static inline bool b2Pair_IsFinal(b2Pair *pair)
{
	return (pair->status & b2Pair_e_pairFinal) == b2Pair_e_pairFinal;
}

typedef struct b2BufferedPair b2BufferedPair;
struct b2BufferedPair
{
	uint16 proxyId1;
	uint16 proxyId2;
};

typedef struct b2PairCallback b2PairCallback;
struct b2PairCallback
{
	// This should return the new pair user data. It is okay if the
	// user data is null.
	void* (*PairAdded)(b2PairCallback *callback, void* proxyUserData1, void* proxyUserData2);

	// This should free the pair's user data. In extreme circumstances, it is possible
	// this will be called with null pairUserData because the pair never existed.
	void (*PairRemoved)(b2PairCallback *callback, void* proxyUserData1, void* proxyUserData2, void* pairUserData);
};

typedef struct b2PairManager b2PairManager;
struct b2PairManager
{
	b2BroadPhase *m_broadPhase;
	b2PairCallback *m_callback;
	b2Pair m_pairs[b2_maxPairs];
	uint16 m_freePair;
	int32 m_pairCount;

	b2BufferedPair m_pairBuffer[b2_maxPairs];
	int32 m_pairBufferCount;

	uint16 m_hashTable[b2_tableCapacity];
};

void b2PairManager_ctor(b2PairManager *manager);

void b2PairManager_Initialize(b2PairManager *manager, b2BroadPhase* broadPhase, b2PairCallback* callback);

void b2PairManager_AddBufferedPair(b2PairManager *manager, int32 proxyId1, int32 proxyId2);

void b2PairManager_RemoveBufferedPair(b2PairManager *manager, int32 proxyId1, int32 proxyId2);

void b2PairManager_Commit(b2PairManager *manager);

#endif
