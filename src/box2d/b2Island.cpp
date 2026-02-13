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
#include <box2d/b2ContactSolver.h>
#include <box2d/b2Island.h>
#include <box2d/b2Joint.h>
#include <box2d/b2StackAllocator.h>
#include <box2d/b2World.h>

/*
Position Correction Notes
=========================
I tried the several algorithms for position correction of the 2D revolute joint.
I looked at these systems:
- simple pendulum (1m diameter sphere on massless 5m stick) with initial angular
velocity of 100 rad/s.
- suspension bridge with 30 1m long planks of length 1m.
- multi-link chain with 30 1m long links.

Here are the algorithms:

Baumgarte - A fraction of the position error is added to the velocity error.
There is no separate position solver.

Pseudo Velocities - After the velocity solver and position integration,
the position error, Jacobian, and effective mass are recomputed. Then
the velocity constraints are solved with pseudo velocities and a fraction
of the position error is added to the pseudo velocity error. The pseudo
velocities are initialized to zero and there is no warm-starting. After
the position solver, the pseudo velocities are added to the positions.
This is also called the First Order World method or the Position LCP method.

Modified Nonlinear Gauss-Seidel (NGS) - Like Pseudo Velocities except the
position error is re-computed for each constraint and the positions are updated
after the constraint is solved. The radius vectors (aka Jacobians) are
re-computed too (otherwise the algorithm has horrible instability). The pseudo
velocity states are not needed because they are effectively zero at the
beginning of each iteration. Since we have the current position error, we allow
the iterations to terminate early if the error becomes smaller than
b2_linearSlop.

Full NGS or just NGS - Like Modified NGS except the effective mass are
re-computed each time a constraint is solved.

Here are the results:
Baumgarte - this is the cheapest algorithm but it has some stability problems,
especially with the bridge. The chain links separate easily close to the root
and they jitter as they struggle to pull together. This is one of the most
common methods in the field. The big drawback is that the position correction
artificially affects the momentum, thus leading to instabilities and false
bounce. I used a bias factor of 0.2. A larger bias factor makes the bridge less
stable, a smaller factor makes joints and contacts more spongy.

Pseudo Velocities - the is more stable than the Baumgarte method. The bridge is
stable. However, joints still separate with large angular velocities. Drag the
simple pendulum in a circle quickly and the joint will separate. The chain
separates easily and does not recover. I used a bias factor of 0.2. A larger
value lead to the bridge collapsing when a heavy cube drops on it.

Modified NGS - this algorithm is better in some ways than Baumgarte and Pseudo
Velocities, but in other ways it is worse. The bridge and chain are much more
stable, but the simple pendulum goes unstable at high angular velocities.

Full NGS - stable in all tests. The joints display good stiffness. The bridge
still sags, but this is better than infinite forces.

Recommendations
Pseudo Velocities are not really worthwhile because the bridge and chain cannot
recover from joint separation. In other cases the benefit over Baumgarte is
small.

Modified NGS is not a robust method for the revolute joint due to the violent
instability seen in the simple pendulum. Perhaps it is viable with other
constraint types, especially scalar constraints where the effective mass is a
scalar.

This leaves Baumgarte and Full NGS. Baumgarte has small, but manageable
instabilities and is very fast. I don't think we can escape Baumgarte,
especially in highly demanding cases where high constraint fidelity is not
needed.

Full NGS is robust and easy on the eyes. I recommend this as an option for
higher fidelity simulation and certainly for suspension bridges and long chains.
Full NGS might be a good choice for ragdolls, especially motorized ragdolls
where joint separation can be problematic. The number of NGS iterations can be
reduced for better performance without harming robustness much.

Each joint in a can be handled differently in the position solver. So I
recommend a system where the user can select the algorithm on a per joint basis.
I would probably default to the slower Full NGS and let the user select the
faster Baumgarte method in performance critical scenarios.
*/

void b2Island_ctor(b2Island *island, int32 bodyCapacity, int32 contactCapacity,
                   int32 jointCapacity, b2StackAllocator *allocator) {
  island->m_bodyCapacity = bodyCapacity;
  island->m_contactCapacity = contactCapacity;
  island->m_jointCapacity = jointCapacity;
  island->m_bodyCount = 0;
  island->m_contactCount = 0;
  island->m_jointCount = 0;

  island->m_bodies = (b2Body **)b2StackAllocator_Allocate(
      allocator, bodyCapacity * sizeof(b2Body *));
  island->m_contacts = (b2Contact **)b2StackAllocator_Allocate(
      allocator, contactCapacity * sizeof(b2Contact *));
  island->m_joints = (b2Joint **)b2StackAllocator_Allocate(
      allocator, jointCapacity * sizeof(b2Joint *));

  island->m_allocator = allocator;
}

void b2Island_dtor(b2Island *island) {
  // Warning: the order should reverse the constructor order.
  b2StackAllocator_Free(island->m_allocator, island->m_joints);
  b2StackAllocator_Free(island->m_allocator, island->m_contacts);
  b2StackAllocator_Free(island->m_allocator, island->m_bodies);
}

void b2Island_Clear(b2Island *island) {
  island->m_bodyCount = 0;
  island->m_contactCount = 0;
  island->m_jointCount = 0;
}

void b2Island_Solve(b2Island *island, const b2TimeStep *step,
                    const b2Vec2 &gravity) {
  for (int32 i = 0; i < island->m_bodyCount; ++i) {
    b2Body *b = island->m_bodies[i];

    if (b->m_invMass == 0.0)
      continue;

    b->m_linearVelocity += step->dt * (gravity + b->m_invMass * b->m_force);
    b->m_angularVelocity += step->dt * b->m_invI * b->m_torque;

    b->m_linearVelocity *= b->m_linearDamping;
    b->m_angularVelocity *= b->m_angularDamping;

    // Store positions for conservative advancement.
    b->m_position0 = b->m_position;
    b->m_rotation0 = b->m_rotation;
  }

  b2ContactSolver contactSolver;
  b2ContactSolver_ctor(&contactSolver, island->m_contacts,
                       island->m_contactCount, island->m_allocator);

  // Pre-solve
  b2ContactSolver_PreSolve(&contactSolver);

  for (int32 i = 0; i < island->m_jointCount; ++i) {
    island->m_joints[i]->PrepareVelocitySolver(island->m_joints[i]);
  }

  // Solve velocity constraints.
  for (int32 i = 0; i < step->iterations; ++i) {
    b2ContactSolver_SolveVelocityConstraints(&contactSolver);

    for (int32 j = 0; j < island->m_jointCount; ++j) {
      island->m_joints[j]->SolveVelocityConstraints(island->m_joints[j], step);
    }
  }

  // Integrate positions.
  for (int32 i = 0; i < island->m_bodyCount; ++i) {
    b2Body *b = island->m_bodies[i];

    if (b->m_invMass == 0.0)
      continue;

    b->m_position += step->dt * b->m_linearVelocity;
    b->m_rotation += step->dt * b->m_angularVelocity;

    b2Mat22_SetAngle(&b->m_R, b->m_rotation);
  }

  // Solve position constraints.
  if (b2World_s_enablePositionCorrection) {
    for (int32 iter = 0; iter < step->iterations; ++iter) {
      bool contactsOkay = b2ContactSolver_SolvePositionConstraints(
          &contactSolver, b2_contactBaumgarte);

      bool jointsOkay = true;
      for (int i = 0; i < island->m_jointCount; ++i) {
        bool jointOkay =
            island->m_joints[i]->SolvePositionConstraints(island->m_joints[i]);
        jointsOkay = jointsOkay && jointOkay;
      }

      if (contactsOkay && jointsOkay) {
        break;
      }
    }
  }

  // Post-solve.
  b2ContactSolver_PostSolve(&contactSolver);

  // Synchronize shapes and reset forces.
  for (int32 i = 0; i < island->m_bodyCount; ++i) {
    b2Body *b = island->m_bodies[i];

    if (b->m_invMass == 0.0)
      continue;

    b2Mat22_SetAngle(&b->m_R, b->m_rotation);

    b2Body_SynchronizeShapes(b);
    b2Vec2_Set(&b->m_force, 0.0, 0.0);
    b->m_torque = 0.0;
  }

  b2ContactSolver_dtor(&contactSolver);
}

void b2Island_UpdateSleep(b2Island *island, float64 dt) {
  float64 minSleepTime = DBL_MAX;

  const float64 linTolSqr = b2_linearSleepTolerance * b2_linearSleepTolerance;
  const float64 angTolSqr = b2_angularSleepTolerance * b2_angularSleepTolerance;

  for (int32 i = 0; i < island->m_bodyCount; ++i) {
    b2Body *b = island->m_bodies[i];
    if (b->m_invMass == 0.0) {
      continue;
    }

    if ((b->m_flags & b2Body_e_allowSleepFlag) == 0) {
      b->m_sleepTime = 0.0;
      minSleepTime = 0.0;
    }

    if ((b->m_flags & b2Body_e_allowSleepFlag) == 0 ||
        b->m_angularVelocity * b->m_angularVelocity > angTolSqr ||
        b2Dot(b->m_linearVelocity, b->m_linearVelocity) > linTolSqr) {
      b->m_sleepTime = 0.0;
      minSleepTime = 0.0;
    } else {
      b->m_sleepTime += dt;
      minSleepTime = b2Min(minSleepTime, b->m_sleepTime);
    }
  }

  if (minSleepTime >= b2_timeToSleep) {
    for (int32 i = 0; i < island->m_bodyCount; ++i) {
      b2Body *b = island->m_bodies[i];
      b->m_flags |= b2Body_e_sleepFlag;
    }
  }
}
