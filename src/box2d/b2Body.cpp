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

#include <box2d/b2Body.h>
#include <box2d/b2Contact.h>
#include <box2d/b2Joint.h>
#include <box2d/b2Math.h>
#include <box2d/b2Shape.h>
#include <box2d/b2World.h>
#include <string.h>

void b2BodyDef_ctor(b2BodyDef *def) {
  def->userData = NULL;
  memset(def->shapes, 0, sizeof(def->shapes));
  b2Vec2_Set(&def->position, 0.0, 0.0);
  def->rotation = 0.0;
  b2Vec2_Set(&def->linearVelocity, 0.0, 0.0);
  def->angularVelocity = 0.0;
  def->linearDamping = 0.0;
  def->angularDamping = 0.0;
  def->allowSleep = true;
  def->isSleeping = false;
}

void b2Body_ctor(b2Body *body, const b2BodyDef *bd, b2World *world) {
  body->m_flags = 0;
  body->m_position = bd->position;
  body->m_rotation = bd->rotation;
  b2Mat22_SetAngle(&body->m_R, body->m_rotation);
  body->m_position0 = body->m_position;
  body->m_rotation0 = body->m_rotation;
  body->m_world = world;

  body->m_linearDamping = b2Clamp(1.0 - bd->linearDamping, 0.0, 1.0);
  body->m_angularDamping = b2Clamp(1.0 - bd->angularDamping, 0.0, 1.0);

  b2Vec2_Set(&body->m_force, 0.0, 0.0);
  body->m_torque = 0.0;

  body->m_mass = 0.0;

  b2MassData massDatas[b2_maxShapesPerBody];

  // Compute the shape mass properties, the bodies total mass and COM.
  body->m_shapeCount = 0;
  b2Vec2_Set(&body->m_center, 0.0, 0.0);
  for (int32 i = 0; i < b2_maxShapesPerBody; ++i) {
    const b2ShapeDef *sd = bd->shapes[i];
    if (sd == NULL)
      break;
    b2MassData *massData = massDatas + i;
    b2ShapeDef_ComputeMass(sd, massData);
    body->m_mass += massData->mass;
    body->m_center += massData->mass * (sd->localPosition + massData->center);
    ++body->m_shapeCount;
  }

  // Compute center of mass, and shift the origin to the COM.
  if (body->m_mass > 0.0) {
    body->m_center *= 1.0 / body->m_mass;
    body->m_position += b2Mul(body->m_R, body->m_center);
  } else {
    body->m_flags |= b2Body_e_staticFlag;
  }

  // Compute the moment of inertia.
  body->m_I = 0.0;
  for (int32 i = 0; i < body->m_shapeCount; ++i) {
    const b2ShapeDef *sd = bd->shapes[i];
    b2MassData *massData = massDatas + i;
    body->m_I += massData->I;
    b2Vec2 r = sd->localPosition + massData->center - body->m_center;
    body->m_I += massData->mass * b2Dot(r, r);
  }

  if (body->m_mass > 0.0) {
    body->m_invMass = 1.0 / body->m_mass;
  } else {
    body->m_invMass = 0.0;
  }

  if (body->m_I > 0.0) {
    body->m_invI = 1.0 / body->m_I;
  } else {
    body->m_I = 0.0;
    body->m_invI = 0.0;
  }

  // Compute the center of mass velocity.
  body->m_linearVelocity =
      bd->linearVelocity + b2Cross(bd->angularVelocity, body->m_center);
  body->m_angularVelocity = bd->angularVelocity;

  body->m_jointList = NULL;
  body->m_contactList = NULL;
  body->m_prev = NULL;
  body->m_next = NULL;

  // Create the shapes.
  body->m_shapeList = NULL;
  for (int32 i = 0; i < body->m_shapeCount; ++i) {
    const b2ShapeDef *sd = bd->shapes[i];
    b2Shape *shape = b2Shape_Create(sd, body, body->m_center);
    shape->m_next = body->m_shapeList;
    body->m_shapeList = shape;
  }

  body->m_sleepTime = 0.0;
  if (bd->allowSleep) {
    body->m_flags |= b2Body_e_allowSleepFlag;
  }
  if (bd->isSleeping) {
    body->m_flags |= b2Body_e_sleepFlag;
  }

  if ((body->m_flags & b2Body_e_sleepFlag) || body->m_invMass == 0.0) {
    b2Vec2_Set(&body->m_linearVelocity, 0.0, 0.0);
    body->m_angularVelocity = 0.0;
  }

  body->m_userData = bd->userData;
}

void b2Body_dtor(b2Body *body) {
  b2Shape *s = body->m_shapeList;
  while (s) {
    b2Shape *s0 = s;
    s = s->m_next;

    b2Shape_Destroy(s0);
  }
}

b2Vec2 b2Body_GetOriginPosition(const b2Body *body) {
  return body->m_position - b2Mul(body->m_R, body->m_center);
}

void b2Body_SynchronizeShapes(b2Body *body) {
  b2Mat22 R0;
  b2Mat22_SetAngle(&R0, body->m_rotation0);
  for (b2Shape *s = body->m_shapeList; s; s = s->m_next) {
    s->Synchronize(s, body->m_position0, &R0, body->m_position, &body->m_R);
  }
}

void b2Body_Freeze(b2Body *body) {
  body->m_flags |= b2Body_e_frozenFlag;
  b2Vec2_SetZero(&body->m_linearVelocity);
  body->m_angularVelocity = 0.0;
  for (b2Shape *s = body->m_shapeList; s; s = s->m_next) {
    b2Shape_DestroyProxy(s);
  }
}
