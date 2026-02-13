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

#ifndef B2_CMATH_H
#define B2_CMATH_H

#include <box2d/b2Vec.h>
#include <fpmath/fpmath.h>
#include <math.h>

#define MIN_VALUE 5e-324

#define b2Max(a, b) ((a) > (b) ? (a) : (b))

static void b2Vec2_SetZero(b2Vec2 *v) {
  v->x = 0.0;
  v->y = 0.0;
}

static void b2Vec2_Set(b2Vec2 *v, float64 x_, float64 y_) {
  v->x = x_;
  v->y = y_;
}

static b2Vec2 b2Vec2_Make(float64 x, float64 y) {
  b2Vec2 v;
  b2Vec2_Set(&v, x, y);
  return v;
}

static float64 b2Vec2_Length(b2Vec2 *v) {
  return sqrt(v->x * v->x + v->y * v->y);
}

static float64 b2Vec2_Normalize(b2Vec2 *v) {
  float64 length = b2Vec2_Length(v);
  if (length < MIN_VALUE) {
    return 0.0;
  }
  float64 invLength = 1.0 / length;
  v->x *= invLength;
  v->y *= invLength;

  return length;
}

static void b2Mat22_SetAngle(b2Mat22 *m, float64 angle) {
  float64 c = fp_cos(angle), s = fp_sin(angle);
  m->col1.x = c;
  m->col2.x = -s;
  m->col1.y = s;
  m->col2.y = c;
}

static void b2Mat22_SetIdentity(b2Mat22 *m) {
  m->col1.x = 1.0;
  m->col2.x = 0.0;
  m->col1.y = 0.0;
  m->col2.y = 1.0;
}

static b2Mat22 b2Mat22_Invert(b2Mat22 *m) {
  float64 a = m->col1.x, b = m->col2.x, c = m->col1.y, d = m->col2.y;
  b2Mat22 B;
  float64 det = a * d - b * c;
  b2Assert(det != 0.0);
  det = 1.0 / det;
  B.col1.x = det * d;
  B.col2.x = -det * b;
  B.col1.y = -det * c;
  B.col2.y = det * a;
  return B;
}

static b2Vec2 b2Vec2_neg(b2Vec2 v) {
  b2Vec2 u = {-v.x, -v.y};
  return u;
}

#endif
