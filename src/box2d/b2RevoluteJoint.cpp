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
#include <box2d/b2RevoluteJoint.h>
#include <box2d/b2World.h>

#include <box2d/b2Island.h>

// Point-to-point constraint
// C = p2 - p1
// Cdot = v2 - v1
//      = v2 + cross(w2, r2) - v1 - cross(w1, r1)
// J = [-I -r1_skew I r2_skew ]
// Identity used:
// w k % (rx i + ry j) = w * (-ry i + rx j)

// Motor constraint
// Cdot = w2 - w1
// J = [0 0 -1 0 0 1]
// K = invI1 + invI2

void b2RevoluteJointDef_ctor(b2RevoluteJointDef *rev_joint_def) {
  b2JointDef_ctor(&rev_joint_def->m_jointDef);
  rev_joint_def->m_jointDef.type = e_revoluteJoint;
  b2Vec2_Set(&rev_joint_def->anchorPoint, 0.0, 0.0);
  rev_joint_def->lowerAngle = 0.0;
  rev_joint_def->upperAngle = 0.0;
  rev_joint_def->motorTorque = 0.0;
  rev_joint_def->motorSpeed = 0.0;
  rev_joint_def->enableLimit = false;
  rev_joint_def->enableMotor = false;
}

void b2RevoluteJoint_ctor(b2RevoluteJoint *rev_joint,
                          const b2RevoluteJointDef *def) {
  b2Joint_ctor(&rev_joint->m_joint, &def->m_jointDef);

  rev_joint->m_joint.GetAnchor1 = b2RevoluteJoint_GetAnchor1;
  rev_joint->m_joint.GetAnchor2 = b2RevoluteJoint_GetAnchor2;
  rev_joint->m_joint.GetReactionForce = b2RevoluteJoint_GetReactionForce;
  rev_joint->m_joint.GetReactionTorque = b2RevoluteJoint_GetReactionTorque;
  rev_joint->m_joint.PrepareVelocitySolver =
      b2RevoluteJoint_PrepareVelocitySolver;
  rev_joint->m_joint.SolveVelocityConstraints =
      b2RevoluteJoint_SolveVelocityConstraints;
  rev_joint->m_joint.SolvePositionConstraints =
      b2RevoluteJoint_SolvePositionConstraints;

  rev_joint->m_localAnchor1 =
      b2MulT(rev_joint->m_joint.m_body1->m_R,
             def->anchorPoint - rev_joint->m_joint.m_body1->m_position);
  rev_joint->m_localAnchor2 =
      b2MulT(rev_joint->m_joint.m_body2->m_R,
             def->anchorPoint - rev_joint->m_joint.m_body2->m_position);

  rev_joint->m_intialAngle = rev_joint->m_joint.m_body2->m_rotation -
                             rev_joint->m_joint.m_body1->m_rotation;

  b2Vec2_Set(&rev_joint->m_ptpImpulse, 0.0, 0.0);
  rev_joint->m_motorImpulse = 0.0;
  rev_joint->m_limitImpulse = 0.0;
  rev_joint->m_limitPositionImpulse = 0.0;

  rev_joint->m_lowerAngle = def->lowerAngle;
  rev_joint->m_upperAngle = def->upperAngle;
  rev_joint->m_maxMotorTorque = def->motorTorque;
  rev_joint->m_motorSpeed = def->motorSpeed;
  rev_joint->m_enableLimit = def->enableLimit;
  rev_joint->m_enableMotor = def->enableMotor;
}

void b2RevoluteJoint_PrepareVelocitySolver(b2Joint *joint) {
  b2RevoluteJoint *revoluteJoint = (b2RevoluteJoint *)joint;
  b2Body *b1 = joint->m_body1;
  b2Body *b2 = joint->m_body2;

  // Compute the effective mass matrix.
  b2Vec2 r1 = b2Mul(b1->m_R, revoluteJoint->m_localAnchor1);
  b2Vec2 r2 = b2Mul(b2->m_R, revoluteJoint->m_localAnchor2);

  // K    = [(1/m1 + 1/m2) * eye(2) - skew(r1) * invI1 * skew(r1) - skew(r2) *
  // invI2 * skew(r2)]
  //      = [1/m1+1/m2     0    ] + invI1 * [r1.y*r1.y -r1.x*r1.y] + invI2 *
  //      [r1.y*r1.y -r1.x*r1.y]
  //        [    0     1/m1+1/m2]           [-r1.x*r1.y r1.x*r1.x] [-r1.x*r1.y
  //        r1.x*r1.x]
  float64 invMass1 = b1->m_invMass, invMass2 = b2->m_invMass;
  float64 invI1 = b1->m_invI, invI2 = b2->m_invI;

  b2Mat22 K1;
  K1.col1.x = invMass1 + invMass2;
  K1.col2.x = 0.0;
  K1.col1.y = 0.0;
  K1.col2.y = invMass1 + invMass2;

  b2Mat22 K2;
  K2.col1.x = invI1 * r1.y * r1.y;
  K2.col2.x = -invI1 * r1.x * r1.y;
  K2.col1.y = -invI1 * r1.x * r1.y;
  K2.col2.y = invI1 * r1.x * r1.x;

  b2Mat22 K3;
  K3.col1.x = invI2 * r2.y * r2.y;
  K3.col2.x = -invI2 * r2.x * r2.y;
  K3.col1.y = -invI2 * r2.x * r2.y;
  K3.col2.y = invI2 * r2.x * r2.x;

  b2Mat22 K = K1 + K2 + K3;
  revoluteJoint->m_ptpMass = b2Mat22_Invert(&K);

  revoluteJoint->m_motorMass = 1.0 / (invI1 + invI2);

  if (revoluteJoint->m_enableMotor == false) {
    revoluteJoint->m_motorImpulse = 0.0;
  }

  if (revoluteJoint->m_enableLimit) {
    float64 jointAngle =
        b2->m_rotation - b1->m_rotation - revoluteJoint->m_intialAngle;
    if (b2Abs(revoluteJoint->m_upperAngle - revoluteJoint->m_lowerAngle) <
        2.0 * b2_angularSlop) {
      revoluteJoint->m_limitState = e_equalLimits;
    } else if (jointAngle <= revoluteJoint->m_lowerAngle) {
      if (revoluteJoint->m_limitState != e_atLowerLimit) {
        revoluteJoint->m_limitImpulse = 0.0;
      }
      revoluteJoint->m_limitState = e_atLowerLimit;
    } else if (jointAngle >= revoluteJoint->m_upperAngle) {
      if (revoluteJoint->m_limitState != e_atUpperLimit) {
        revoluteJoint->m_limitImpulse = 0.0;
      }
      revoluteJoint->m_limitState = e_atUpperLimit;
    } else {
      revoluteJoint->m_limitState = e_inactiveLimit;
      revoluteJoint->m_limitImpulse = 0.0;
    }
  } else {
    revoluteJoint->m_limitImpulse = 0.0;
  }

  if (b2World_s_enableWarmStarting) {
    b1->m_linearVelocity -= invMass1 * revoluteJoint->m_ptpImpulse;
    b1->m_angularVelocity -=
        invI1 * (b2Cross(r1, revoluteJoint->m_ptpImpulse) +
                 revoluteJoint->m_motorImpulse + revoluteJoint->m_limitImpulse);

    b2->m_linearVelocity += invMass2 * revoluteJoint->m_ptpImpulse;
    b2->m_angularVelocity +=
        invI2 * (b2Cross(r2, revoluteJoint->m_ptpImpulse) +
                 revoluteJoint->m_motorImpulse + revoluteJoint->m_limitImpulse);
  } else {
    b2Vec2_SetZero(&revoluteJoint->m_ptpImpulse);
    revoluteJoint->m_motorImpulse = 0.0;
    revoluteJoint->m_limitImpulse = 0.0;
  }

  revoluteJoint->m_limitPositionImpulse = 0.0;
}

void b2RevoluteJoint_SolveVelocityConstraints(b2Joint *joint,
                                              const b2TimeStep *step) {
  b2RevoluteJoint *revoluteJoint = (b2RevoluteJoint *)joint;

  b2Body *b1 = joint->m_body1;
  b2Body *b2 = joint->m_body2;

  b2Vec2 r1 = b2Mul(b1->m_R, revoluteJoint->m_localAnchor1);
  b2Vec2 r2 = b2Mul(b2->m_R, revoluteJoint->m_localAnchor2);

  // Solve point-to-point constraint
  b2Vec2 ptpCdot = b2->m_linearVelocity + b2Cross(b2->m_angularVelocity, r2) -
                   b1->m_linearVelocity - b2Cross(b1->m_angularVelocity, r1);
  b2Vec2 ptpImpulse = -b2Mul(revoluteJoint->m_ptpMass, ptpCdot);
  revoluteJoint->m_ptpImpulse += ptpImpulse;

  b1->m_linearVelocity -= b1->m_invMass * ptpImpulse;
  b1->m_angularVelocity -= b1->m_invI * b2Cross(r1, ptpImpulse);

  b2->m_linearVelocity += b2->m_invMass * ptpImpulse;
  b2->m_angularVelocity += b2->m_invI * b2Cross(r2, ptpImpulse);

  if (revoluteJoint->m_enableMotor &&
      revoluteJoint->m_limitState != e_equalLimits) {
    float64 motorCdot = b2->m_angularVelocity - b1->m_angularVelocity -
                        revoluteJoint->m_motorSpeed;
    float64 motorImpulse = -revoluteJoint->m_motorMass * motorCdot;
    float64 oldMotorImpulse = revoluteJoint->m_motorImpulse;
    revoluteJoint->m_motorImpulse =
        b2Clamp(revoluteJoint->m_motorImpulse + motorImpulse,
                -step->dt * revoluteJoint->m_maxMotorTorque,
                step->dt * revoluteJoint->m_maxMotorTorque);
    motorImpulse = revoluteJoint->m_motorImpulse - oldMotorImpulse;
    b1->m_angularVelocity -= b1->m_invI * motorImpulse;
    b2->m_angularVelocity += b2->m_invI * motorImpulse;
  }

  if (revoluteJoint->m_enableLimit &&
      revoluteJoint->m_limitState != e_inactiveLimit) {
    float64 limitCdot = b2->m_angularVelocity - b1->m_angularVelocity;
    float64 limitImpulse = -revoluteJoint->m_motorMass * limitCdot;

    if (revoluteJoint->m_limitState == e_equalLimits) {
      revoluteJoint->m_limitImpulse += limitImpulse;
    } else if (revoluteJoint->m_limitState == e_atLowerLimit) {
      float64 oldLimitImpulse = revoluteJoint->m_limitImpulse;
      revoluteJoint->m_limitImpulse =
          b2Max(revoluteJoint->m_limitImpulse + limitImpulse, 0.0);
      limitImpulse = revoluteJoint->m_limitImpulse - oldLimitImpulse;
    } else if (revoluteJoint->m_limitState == e_atUpperLimit) {
      float64 oldLimitImpulse = revoluteJoint->m_limitImpulse;
      revoluteJoint->m_limitImpulse =
          b2Min(revoluteJoint->m_limitImpulse + limitImpulse, 0.0);
      limitImpulse = revoluteJoint->m_limitImpulse - oldLimitImpulse;
    }

    b1->m_angularVelocity -= b1->m_invI * limitImpulse;
    b2->m_angularVelocity += b2->m_invI * limitImpulse;
  }
}

bool b2RevoluteJoint_SolvePositionConstraints(b2Joint *joint) {
  b2RevoluteJoint *revoluteJoint = (b2RevoluteJoint *)joint;

  b2Body *b1 = joint->m_body1;
  b2Body *b2 = joint->m_body2;

  float64 positionError = 0.0;

  // Solve point-to-point position error.
  b2Vec2 r1 = b2Mul(b1->m_R, revoluteJoint->m_localAnchor1);
  b2Vec2 r2 = b2Mul(b2->m_R, revoluteJoint->m_localAnchor2);

  b2Vec2 p1 = b1->m_position + r1;
  b2Vec2 p2 = b2->m_position + r2;
  b2Vec2 ptpC = p2 - p1;

  positionError = b2Vec2_Length(&ptpC);

  // Prevent overly large corrections.
  // b2Vec2 dpMax(b2_maxLinearCorrection, b2_maxLinearCorrection);
  // ptpC = b2Clamp(ptpC, -dpMax, dpMax);

  float64 invMass1 = b1->m_invMass, invMass2 = b2->m_invMass;
  float64 invI1 = b1->m_invI, invI2 = b2->m_invI;

  b2Mat22 K1;
  K1.col1.x = invMass1 + invMass2;
  K1.col2.x = 0.0;
  K1.col1.y = 0.0;
  K1.col2.y = invMass1 + invMass2;

  b2Mat22 K2;
  K2.col1.x = invI1 * r1.y * r1.y;
  K2.col2.x = -invI1 * r1.x * r1.y;
  K2.col1.y = -invI1 * r1.x * r1.y;
  K2.col2.y = invI1 * r1.x * r1.x;

  b2Mat22 K3;
  K3.col1.x = invI2 * r2.y * r2.y;
  K3.col2.x = -invI2 * r2.x * r2.y;
  K3.col1.y = -invI2 * r2.x * r2.y;
  K3.col2.y = invI2 * r2.x * r2.x;

  b2Mat22 K = K1 + K2 + K3;
  b2Vec2 impulse = b2Mat22_Solve(&K, -ptpC);

  b1->m_position -= b1->m_invMass * impulse;
  b1->m_rotation -= b1->m_invI * b2Cross(r1, impulse);
  b2Mat22_SetAngle(&b1->m_R, b1->m_rotation);

  b2->m_position += b2->m_invMass * impulse;
  b2->m_rotation += b2->m_invI * b2Cross(r2, impulse);
  b2Mat22_SetAngle(&b2->m_R, b2->m_rotation);

  // Handle limits.
  float64 angularError = 0.0;

  if (revoluteJoint->m_enableLimit &&
      revoluteJoint->m_limitState != e_inactiveLimit) {
    float64 angle =
        b2->m_rotation - b1->m_rotation - revoluteJoint->m_intialAngle;
    float64 limitImpulse = 0.0;

    if (revoluteJoint->m_limitState == e_equalLimits) {
      // Prevent large angular corrections
      float64 limitC =
          b2Clamp(angle, -b2_maxAngularCorrection, b2_maxAngularCorrection);
      limitImpulse = -revoluteJoint->m_motorMass * limitC;
      angularError = b2Abs(limitC);
    } else if (revoluteJoint->m_limitState == e_atLowerLimit) {
      float64 limitC = angle - revoluteJoint->m_lowerAngle;
      angularError = b2Max(0.0, -limitC);

      // Prevent large angular corrections and allow some slop.
      limitC = b2Clamp(limitC + b2_angularSlop, -b2_maxAngularCorrection, 0.0);
      limitImpulse = -revoluteJoint->m_motorMass * limitC;
      float64 oldLimitImpulse = revoluteJoint->m_limitPositionImpulse;
      revoluteJoint->m_limitPositionImpulse =
          b2Max(revoluteJoint->m_limitPositionImpulse + limitImpulse, 0.0);
      limitImpulse = revoluteJoint->m_limitPositionImpulse - oldLimitImpulse;
    } else if (revoluteJoint->m_limitState == e_atUpperLimit) {
      float64 limitC = angle - revoluteJoint->m_upperAngle;
      angularError = b2Max(0.0, limitC);

      // Prevent large angular corrections and allow some slop.
      limitC = b2Clamp(limitC - b2_angularSlop, 0.0, b2_maxAngularCorrection);
      limitImpulse = -revoluteJoint->m_motorMass * limitC;
      float64 oldLimitImpulse = revoluteJoint->m_limitPositionImpulse;
      revoluteJoint->m_limitPositionImpulse =
          b2Min(revoluteJoint->m_limitPositionImpulse + limitImpulse, 0.0);
      limitImpulse = revoluteJoint->m_limitPositionImpulse - oldLimitImpulse;
    }

    b1->m_rotation -= b1->m_invI * limitImpulse;
    b2Mat22_SetAngle(&b1->m_R, b1->m_rotation);
    b2->m_rotation += b2->m_invI * limitImpulse;
    b2Mat22_SetAngle(&b2->m_R, b2->m_rotation);
  }

  return positionError <= b2_linearSlop && angularError <= b2_angularSlop;
}

b2Vec2 b2RevoluteJoint_GetAnchor1(b2Joint *joint) {
  b2RevoluteJoint *revoluteJoint = (b2RevoluteJoint *)joint;
  b2Body *b1 = joint->m_body1;
  return b1->m_position + b2Mul(b1->m_R, revoluteJoint->m_localAnchor1);
}

b2Vec2 b2RevoluteJoint_GetAnchor2(b2Joint *joint) {
  b2RevoluteJoint *revoluteJoint = (b2RevoluteJoint *)joint;
  b2Body *b2 = joint->m_body2;
  return b2->m_position + b2Mul(b2->m_R, revoluteJoint->m_localAnchor2);
}

b2Vec2 b2RevoluteJoint_GetReactionForce(b2Joint *joint, float64 invTimeStep) {
  b2RevoluteJoint *revoluteJoint = (b2RevoluteJoint *)joint;
  return invTimeStep * revoluteJoint->m_ptpImpulse;
}

float64 b2RevoluteJoint_GetReactionTorque(b2Joint *joint, float64 invTimeStep) {
  b2RevoluteJoint *revoluteJoint = (b2RevoluteJoint *)joint;
  return invTimeStep * revoluteJoint->m_limitImpulse;
}
