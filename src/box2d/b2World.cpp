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
#include <box2d/b2BroadPhase.h>
#include <box2d/b2Collision.h>
#include <box2d/b2Contact.h>
#include <box2d/b2Island.h>
#include <box2d/b2Joint.h>
#include <box2d/b2Shape.h>
#include <box2d/b2World.h>

int32 b2World_s_enablePositionCorrection = 1;
int32 b2World_s_enableWarmStarting = 1;

void b2World_ctor(b2World *world, const b2AABB *worldAABB, b2Vec2 gravity,
                  bool doSleep) {
  b2BlockAllocator_ctor(&world->m_blockAllocator);
  b2StackAllocator_ctor(&world->m_stackAllocator);
  b2ContactManager_ctor(&world->m_contactManager);

  world->m_filter = NULL;

  world->m_bodyList = NULL;
  world->m_contactList = NULL;
  world->m_jointList = NULL;

  world->m_bodyCount = 0;
  world->m_contactCount = 0;
  world->m_jointCount = 0;

  world->m_bodyDestroyList = NULL;

  world->m_allowSleep = doSleep;

  world->m_gravity = gravity;

  world->m_contactManager.m_world = world;
  world->m_broadPhase = (b2BroadPhase *)b2Alloc(sizeof(b2BroadPhase));
  b2BroadPhase_ctor(world->m_broadPhase, *worldAABB,
                    &world->m_contactManager.m_pairCallback);

  b2BodyDef bd;
  b2BodyDef_ctor(&bd);
  world->m_groundBody = b2World_CreateBody(world, &bd);
}

void b2World_dtor(b2World *world) {
  b2World_DestroyBody(world, world->m_groundBody);
  b2Free(world->m_broadPhase);

  b2BlockAllocator_dtor(&world->m_blockAllocator);
}

void b2World_SetFilter(b2World *world, b2CollisionFilter filter) {
  world->m_filter = filter;
}

b2Body *b2World_CreateBody(b2World *world, const b2BodyDef *def) {
  b2Body *b = (b2Body *)b2BlockAllocator_Allocate(&world->m_blockAllocator,
                                                  sizeof(b2Body));
  b2Body_ctor(b, def, world);
  b->m_prev = NULL;

  b->m_next = world->m_bodyList;
  if (world->m_bodyList) {
    world->m_bodyList->m_prev = b;
  }
  world->m_bodyList = b;
  ++world->m_bodyCount;

  return b;
}

// Body destruction is deferred to make contact processing more robust.
void b2World_DestroyBody(b2World *world, b2Body *b) {
  if (b->m_flags & b2Body_e_destroyFlag) {
    return;
  }

  // Remove from normal body list.
  if (b->m_prev) {
    b->m_prev->m_next = b->m_next;
  }

  if (b->m_next) {
    b->m_next->m_prev = b->m_prev;
  }

  if (b == world->m_bodyList) {
    world->m_bodyList = b->m_next;
  }

  b->m_flags |= b2Body_e_destroyFlag;
  --world->m_bodyCount;

  // Add to the deferred destruction list.
  b->m_prev = NULL;
  b->m_next = world->m_bodyDestroyList;
  world->m_bodyDestroyList = b;
}

extern "C" void b2World_CleanBodyList(b2World *world) {
  world->m_contactManager.m_destroyImmediate = true;

  b2Body *b = world->m_bodyDestroyList;
  while (b) {
    // Preserve the next pointer.
    b2Body *b0 = b;
    b = b->m_next;

    // Delete the attached joints
    b2JointNode *jn = b0->m_jointList;
    while (jn) {
      b2JointNode *jn0 = jn;
      jn = jn->next;

      b2World_DestroyJoint(world, jn0->joint);
    }

    b2Body_dtor(b0);
    b2BlockAllocator_Free(&world->m_blockAllocator, b0, sizeof(b2Body));
  }

  // Reset the list.
  world->m_bodyDestroyList = NULL;

  world->m_contactManager.m_destroyImmediate = false;
}

b2Joint *b2World_CreateJoint(b2World *world, const b2JointDef *def) {
  b2Joint *j = b2Joint_Create(def, &world->m_blockAllocator);

  // Connect to the world list.
  j->m_prev = NULL;
  j->m_next = world->m_jointList;
  if (world->m_jointList) {
    world->m_jointList->m_prev = j;
  }
  world->m_jointList = j;
  ++world->m_jointCount;

  // Connect to the bodies
  j->m_node1.joint = j;
  j->m_node1.other = j->m_body2;
  j->m_node1.prev = NULL;
  j->m_node1.next = j->m_body1->m_jointList;
  if (j->m_body1->m_jointList)
    j->m_body1->m_jointList->prev = &j->m_node1;
  j->m_body1->m_jointList = &j->m_node1;

  j->m_node2.joint = j;
  j->m_node2.other = j->m_body1;
  j->m_node2.prev = NULL;
  j->m_node2.next = j->m_body2->m_jointList;
  if (j->m_body2->m_jointList)
    j->m_body2->m_jointList->prev = &j->m_node2;
  j->m_body2->m_jointList = &j->m_node2;

  // If the joint prevents collisions, then reset collision filtering.
  if (def->collideConnected == false) {
    // Reset the proxies on the body with the minimum number of shapes.
    b2Body *b = def->body1->m_shapeCount < def->body2->m_shapeCount
                    ? def->body1
                    : def->body2;
    for (b2Shape *s = b->m_shapeList; s; s = s->m_next) {
      s->ResetProxy(s, world->m_broadPhase);
    }
  }

  return j;
}

void b2World_DestroyJoint(b2World *world, b2Joint *j) {
  bool collideConnected = j->m_collideConnected;

  // Remove from the world.
  if (j->m_prev) {
    j->m_prev->m_next = j->m_next;
  }

  if (j->m_next) {
    j->m_next->m_prev = j->m_prev;
  }

  if (j == world->m_jointList) {
    world->m_jointList = j->m_next;
  }

  // Disconnect from island graph.
  b2Body *body1 = j->m_body1;
  b2Body *body2 = j->m_body2;

  // Wake up touching bodies.
  b2Body_WakeUp(body1);
  b2Body_WakeUp(body2);

  // Remove from body 1
  if (j->m_node1.prev) {
    j->m_node1.prev->next = j->m_node1.next;
  }

  if (j->m_node1.next) {
    j->m_node1.next->prev = j->m_node1.prev;
  }

  if (&j->m_node1 == body1->m_jointList) {
    body1->m_jointList = j->m_node1.next;
  }

  j->m_node1.prev = NULL;
  j->m_node1.next = NULL;

  // Remove from body 2
  if (j->m_node2.prev) {
    j->m_node2.prev->next = j->m_node2.next;
  }

  if (j->m_node2.next) {
    j->m_node2.next->prev = j->m_node2.prev;
  }

  if (&j->m_node2 == body2->m_jointList) {
    body2->m_jointList = j->m_node2.next;
  }

  j->m_node2.prev = NULL;
  j->m_node2.next = NULL;

  b2Joint_Destroy(j, &world->m_blockAllocator);

  --world->m_jointCount;

  // If the joint prevents collisions, then reset collision filtering.
  if (collideConnected == false) {
    // Reset the proxies on the body with the minimum number of shapes.
    b2Body *b = body1->m_shapeCount < body2->m_shapeCount ? body1 : body2;
    for (b2Shape *s = b->m_shapeList; s; s = s->m_next) {
      s->ResetProxy(s, world->m_broadPhase);
    }
  }
}

void b2World_Step(b2World *world, float64 dt, int32 iterations) {
  b2TimeStep step;
  step.dt = dt;
  step.iterations = iterations;
  if (dt > 0.0) {
    step.inv_dt = 1.0 / dt;
  } else {
    step.inv_dt = 0.0;
  }

  // Handle deferred contact destruction.
  b2ContactManager_CleanContactList(&world->m_contactManager);

  // Handle deferred body destruction.
  b2World_CleanBodyList(world);

  // Update contacts.
  b2ContactManager_Collide(&world->m_contactManager);

  // Size the island for the worst case.
  b2Island island;
  b2Island_ctor(&island, world->m_bodyCount, world->m_contactCount,
                world->m_jointCount, &world->m_stackAllocator);

  // Clear all the island flags.
  for (b2Body *b = world->m_bodyList; b; b = b->m_next) {
    b->m_flags &= ~b2Body_e_islandFlag;
  }
  for (b2Contact *c = world->m_contactList; c; c = c->m_next) {
    c->m_flags &= ~b2Contact_e_islandFlag;
  }
  for (b2Joint *j = world->m_jointList; j; j = j->m_next) {
    j->m_islandFlag = false;
  }

  // Build and simulate all awake islands.
  int32 stackSize = world->m_bodyCount;
  b2Body **stack = (b2Body **)b2StackAllocator_Allocate(
      &world->m_stackAllocator, stackSize * sizeof(b2Body *));
  for (b2Body *seed = world->m_bodyList; seed; seed = seed->m_next) {
    if (seed->m_flags & (b2Body_e_staticFlag | b2Body_e_islandFlag |
                         b2Body_e_sleepFlag | b2Body_e_frozenFlag)) {
      continue;
    }

    // Reset island and stack.
    b2Island_Clear(&island);
    int32 stackCount = 0;
    stack[stackCount++] = seed;
    seed->m_flags |= b2Body_e_islandFlag;

    // Perform a depth first search (DFS) on the constraint graph.
    while (stackCount > 0) {
      // Grab the next body off the stack and add it to the island.
      b2Body *b = stack[--stackCount];
      b2Island_AddBody(&island, b);

      // Make sure the body is awake.
      b->m_flags &= ~b2Body_e_sleepFlag;

      // To keep islands as small as possible, we don't
      // propagate islands across static bodies.
      if (b->m_flags & b2Body_e_staticFlag) {
        continue;
      }

      // Search all contacts connected to this body.
      for (b2ContactNode *cn = b->m_contactList; cn; cn = cn->next) {
        if (cn->contact->m_flags & b2Contact_e_islandFlag) {
          continue;
        }

        b2Island_AddContact(&island, cn->contact);
        cn->contact->m_flags |= b2Contact_e_islandFlag;

        b2Body *other = cn->other;
        if (other->m_flags & b2Body_e_islandFlag) {
          continue;
        }

        stack[stackCount++] = other;
        other->m_flags |= b2Body_e_islandFlag;
      }

      // Search all joints connect to this body.
      for (b2JointNode *jn = b->m_jointList; jn; jn = jn->next) {
        if (jn->joint->m_islandFlag == true) {
          continue;
        }

        b2Island_AddJoint(&island, jn->joint);
        jn->joint->m_islandFlag = true;

        b2Body *other = jn->other;
        if (other->m_flags & b2Body_e_islandFlag) {
          continue;
        }

        stack[stackCount++] = other;
        other->m_flags |= b2Body_e_islandFlag;
      }
    }

    b2Island_Solve(&island, &step, world->m_gravity);

    if (world->m_allowSleep) {
      b2Island_UpdateSleep(&island, dt);
    }

    // Post solve cleanup.
    for (int32 i = 0; i < island.m_bodyCount; ++i) {
      // Allow static bodies to participate in other islands.
      b2Body *b = island.m_bodies[i];
      if (b->m_flags & b2Body_e_staticFlag) {
        b->m_flags &= ~b2Body_e_islandFlag;
      }
    }
  }

  b2StackAllocator_Free(&world->m_stackAllocator, stack);

  b2BroadPhase_Commit(world->m_broadPhase);

  b2Island_dtor(&island);
}
