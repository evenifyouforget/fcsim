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

#ifndef B2_MATH_H
#define B2_MATH_H

#include <box2d/b2Settings.h>
#include <box2d/b2Vec.h>
#include <fpmath/fpmath.h>
#include <math.h>
#include <float.h>
#include <stdlib.h>

#define MIN_VALUE 5e-324

inline bool b2IsValid(UNUSED float64 x)
{
#ifdef _MSC_VER
	return _finite(x) != 0;
#else
	//return isfinite(x) != 0;
	return 1;
#endif
}

inline void b2Vec2_SetZero(b2Vec2 *v)
{
	v->x = 0.0;
	v->y = 0.0;
}

static void b2Vec2_Set(b2Vec2 *v, float64 x_, float64 y_)
{
	v->x = x_;
	v->y = y_;
}

inline b2Vec2 b2Vec2_Make(float64 x, float64 y)
{
	b2Vec2 v;
	b2Vec2_Set(&v, x, y);
	return v;
}

static float64 b2Vec2_Length(b2Vec2 *v)
{
	return sqrt(v->x * v->x + v->y * v->y);
}

inline float64 b2Vec2_Normalize(b2Vec2 *v)
{
	float64 length = b2Vec2_Length(v);
	if (length < MIN_VALUE)
	{
		return 0.0;
	}
	float64 invLength = 1.0 / length;
	v->x *= invLength;
	v->y *= invLength;

	return length;
}

inline bool b2Vec2_IsValid(const b2Vec2 *v)
{
	return b2IsValid(v->x) && b2IsValid(v->y);
}

static void b2Mat22_Set(b2Mat22 *m, const b2Vec2& c1, const b2Vec2& c2)
{
	m->col1 = c1;
	m->col2 = c2;
}

inline void b2Mat22_SetAngle(b2Mat22 *m, float64 angle)
{
	float64 c = fp_cos(angle), s = fp_sin(angle);
	m->col1.x = c; m->col2.x = -s;
	m->col1.y = s; m->col2.y = c;
}

inline void b2Mat22_SetIdentity(b2Mat22 *m)
{
	m->col1.x = 1.0; m->col2.x = 0.0;
	m->col1.y = 0.0; m->col2.y = 1.0;
}

inline b2Mat22 b2Mat22_Invert(b2Mat22 *m)
{
	float64 a = m->col1.x, b = m->col2.x, c = m->col1.y, d = m->col2.y;
	b2Mat22 B;
	float64 det = a * d - b * c;
	b2Assert(det != 0.0);
	det = 1.0 / det;
	B.col1.x =  det * d;	B.col2.x = -det * b;
	B.col1.y = -det * c;	B.col2.y =  det * a;
	return B;
}

// Solve A * x = b
inline b2Vec2 b2Mat22_Solve(b2Mat22 *m, const b2Vec2& b)
{
	float64 a11 = m->col1.x, a12 = m->col2.x, a21 = m->col1.y, a22 = m->col2.y;
	float64 det = a11 * a22 - a12 * a21;
	b2Assert(det != 0.0);
	det = 1.0 / det;
	b2Vec2 x;
	x.x = det * (a22 * b.x - a12 * b.y);
	x.y = det * (a11 * b.y - a21 * b.x);
	return x;
}


inline float64 b2Dot(const b2Vec2& a, const b2Vec2& b)
{
	return a.x * b.x + a.y * b.y;
}

inline float64 b2Cross(const b2Vec2& a, const b2Vec2& b)
{
	return a.x * b.y - a.y * b.x;
}

inline b2Vec2 b2Cross(const b2Vec2& a, float64 s)
{
	b2Vec2 v; b2Vec2_Set(&v, s * a.y, -s * a.x);
	return v;
}

inline b2Vec2 b2Cross(float64 s, const b2Vec2& a)
{
	b2Vec2 v; b2Vec2_Set(&v, -s * a.y, s * a.x);
	return v;
}

inline b2Vec2 b2Mul(const b2Mat22& A, const b2Vec2& v)
{
	b2Vec2 u;
	b2Vec2_Set(&u, A.col1.x * v.x + A.col2.x * v.y, A.col1.y * v.x + A.col2.y * v.y);
	return u;
}

inline b2Vec2 b2MulT(const b2Mat22& A, const b2Vec2& v)
{
	b2Vec2 u;
	b2Vec2_Set(&u, b2Dot(v, A.col1), b2Dot(v, A.col2));
	return u;
}

inline b2Vec2 operator - (const b2Vec2& v)
{
	b2Vec2 u; b2Vec2_Set(&u, -v.x, -v.y);
	return u;
}

inline void operator += (b2Vec2& v, const b2Vec2& u)
{
	v.x += u.x; v.y += u.y;
}

inline void operator -= (b2Vec2& v, const b2Vec2& u)
{
	v.x -= u.x; v.y -= u.y;
}

inline void operator *= (b2Vec2& v, float64 a)
{
	v.x *= a; v.y *= a;
}

inline b2Vec2 operator + (const b2Vec2& a, const b2Vec2& b)
{
	b2Vec2 v; b2Vec2_Set(&v, a.x + b.x, a.y + b.y);
	return v;
}

inline b2Vec2 operator - (const b2Vec2& a, const b2Vec2& b)
{
	b2Vec2 v; b2Vec2_Set(&v, a.x - b.x, a.y - b.y);
	return v;
}

inline b2Vec2 operator * (float64 s, const b2Vec2& a)
{
	b2Vec2 v; b2Vec2_Set(&v, s * a.x, s * a.y);
	return v;
}

inline bool operator == (const b2Vec2& a, const b2Vec2& b)
{
	return a.x == b.x && a.y == b.y;
}

inline b2Mat22 operator + (const b2Mat22& A, const b2Mat22& B)
{
	b2Mat22 C;
	b2Mat22_Set(&C, A.col1 + B.col1, A.col2 + B.col2);
	return C;
}

// A * B
inline b2Mat22 b2Mul(const b2Mat22& A, const b2Mat22& B)
{
	b2Mat22 C;
	b2Mat22_Set(&C, b2Mul(A, B.col1), b2Mul(A, B.col2));
	return C;
}

// A^T * B
inline b2Mat22 b2MulT(const b2Mat22& A, const b2Mat22& B)
{
	b2Vec2 c1; b2Vec2_Set(&c1, b2Dot(A.col1, B.col1), b2Dot(A.col2, B.col1));
	b2Vec2 c2; b2Vec2_Set(&c2, b2Dot(A.col1, B.col2), b2Dot(A.col2, B.col2));
	b2Mat22 C;
	b2Mat22_Set(&C, c1, c2);
	return C;
}

inline float64 b2Abs(float64 a)
{
	return a > 0.0 ? a : -a;
}

inline b2Vec2 b2Abs(const b2Vec2& a)
{
	b2Vec2 b; b2Vec2_Set(&b, fabs(a.x), fabs(a.y));
	return b;
}

inline b2Mat22 b2Abs(const b2Mat22& A)
{
	b2Mat22 B;
	b2Mat22_Set(&B, b2Abs(A.col1), b2Abs(A.col2));
	return B;
}

template <typename T>
inline T b2Min(T a, T b)
{
	return a < b ? a : b;
}

inline b2Vec2 b2Min(const b2Vec2& a, const b2Vec2& b)
{
	b2Vec2 c;
	c.x = b2Min(a.x, b.x);
	c.y = b2Min(a.y, b.y);
	return c;
}

template <typename T>
inline T b2Max(T a, T b)
{
	return a > b ? a : b;
}

inline b2Vec2 b2Max(const b2Vec2& a, const b2Vec2& b)
{
	b2Vec2 c;
	c.x = b2Max(a.x, b.x);
	c.y = b2Max(a.y, b.y);
	return c;
}

template <typename T>
inline T b2Clamp(T a, T low, T high)
{
	return b2Max(low, b2Min(a, high));
}

inline b2Vec2 b2Clamp(const b2Vec2& a, const b2Vec2& low, const b2Vec2& high)
{
	return b2Max(low, b2Min(a, high));
}

template<typename T> inline void b2Swap(T& a, T& b)
{
	T tmp = a;
	a = b;
	b = tmp;
}

#endif
