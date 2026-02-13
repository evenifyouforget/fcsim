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

#ifndef B2_BODY_H
#define B2_BODY_H

#include <box2d/b2Joint.h>
#include <box2d/b2Shape.h>
#include <box2d/b2Vec.h>

typedef struct b2Joint b2Joint;
struct b2Joint;

typedef struct b2Contact b2Contact;
struct b2Contact;

typedef struct b2World b2World;
struct b2World;

typedef struct b2JointNode b2JointNode;
struct b2JointNode;

typedef struct b2ContactNode b2ContactNode;
struct b2ContactNode;

typedef struct b2BodyDef b2BodyDef;
struct b2BodyDef {
  void *userData;
  b2ShapeDef *shapes[b2_maxShapesPerBody];
  b2Vec2 position;
  float64 rotation;
  b2Vec2 linearVelocity;
  float64 angularVelocity;
  float64 linearDamping;
  float64 angularDamping;
  bool allowSleep;
  bool isSleeping;
};

enum {
  b2Body_e_staticFlag = 0x0001,
  b2Body_e_frozenFlag = 0x0002,
  b2Body_e_islandFlag = 0x0004,
  b2Body_e_sleepFlag = 0x0008,
  b2Body_e_allowSleepFlag = 0x0010,
  b2Body_e_destroyFlag = 0x0020,
};

// A rigid body. Internal computation are done in terms
// of the center of mass position. The center of mass may
// be offset from the body's origin.
typedef struct b2Body b2Body;
struct b2Body {
  uint32 m_flags;

  b2Vec2 m_position; // center of mass position
  float64 m_rotation;
  b2Mat22 m_R;

  // Conservative advancement data.
  b2Vec2 m_position0;
  float64 m_rotation0;

  b2Vec2 m_linearVelocity;
  float64 m_angularVelocity;

  b2Vec2 m_force;
  float64 m_torque;

  b2Vec2 m_center; // local vector from client origin to center of mass

  b2World *m_world;
  b2Body *m_prev;
  b2Body *m_next;

  b2Shape *m_shapeList;
  int32 m_shapeCount;

  b2JointNode *m_jointList;
  b2ContactNode *m_contactList;

  float64 m_mass, m_invMass;
  float64 m_I, m_invI;

  float64 m_linearDamping;
  float64 m_angularDamping;

  float64 m_sleepTime;

  void *m_userData;
};

#ifdef __cplusplus
extern "C" {
#endif

void b2BodyDef_ctor(b2BodyDef *def);

void b2Body_ctor(b2Body *body, const b2BodyDef *bd, b2World *world);

void b2Body_dtor(b2Body *body);

void b2Body_SynchronizeShapes(b2Body *body);

// This is called when the child shape has no proxy.
void b2Body_Freeze(b2Body *body);

static inline void b2BodyDef_AddShape(b2BodyDef *def, b2ShapeDef *shape) {
  for (int32 i = 0; i < b2_maxShapesPerBody; ++i) {
    if (def->shapes[i] == NULL) {
      def->shapes[i] = shape;
      break;
    }
  }
}

// Get the position of the body's origin. The body's origin does not
// necessarily coincide with the center of mass. It depends on how the
// shapes are created.
b2Vec2 b2Body_GetOriginPosition(const b2Body *body);

static inline float64 b2Body_GetRotation(const b2Body *body) {
  return body->m_rotation;
}

static inline bool b2Body_IsStatic(const b2Body *body) {
  return (body->m_flags & b2Body_e_staticFlag) == b2Body_e_staticFlag;
}

static inline bool b2Body_IsFrozen(const b2Body *body) {
  return (body->m_flags & b2Body_e_frozenFlag) == b2Body_e_frozenFlag;
}

static inline bool b2Body_IsSleeping(const b2Body *body) {
  return (body->m_flags & b2Body_e_sleepFlag) == b2Body_e_sleepFlag;
}

static inline void b2Body_WakeUp(b2Body *body) {
  body->m_flags &= ~b2Body_e_sleepFlag;
  body->m_sleepTime = 0.0;
}

// This is used to prevent connected bodies from colliding.
// It may lie, depending on the collideConnected flag.
static inline bool b2Body_IsConnected(const b2Body *body, const b2Body *other) {
  for (b2JointNode *jn = body->m_jointList; jn; jn = jn->next) {
    if (jn->other == other)
      return jn->joint->m_collideConnected == false;
  }

  return false;
}

#ifdef __cplusplus
}
#endif

#endif
