/*
 *  VS_Math.cpp
 *  VectorStorm
 *
 *  Created by Trevor Powell on 17/05/07.
 *  Copyright 2007 Trevor Powell. All rights reserved.
 *
 */

#include "VS_Math.h"
#include "VS_Vector.h"
#include <math.h>

float vsCos(float theta)
{
	return cosf(theta);
}

float vsSin(float theta)
{
	return sinf(theta);
}

float vsTan(float theta)
{
	return tanf(theta);
}

float vsACos(float theta)
{
	return acosf(theta);
}

float vsASin(float theta)
{
	return asinf(theta);
}

float vsATan2(float opp, float adj)
{
	return atan2f(opp, adj);
}

static unsigned int fast_sqrt_table[0x10000];  // declare table of square roots

typedef union FastSqrtUnion
{
	float f;
	uint32_t i;
} FastSqrtUnion;

void  build_sqrt_table()
{
	uint32_t i;
	FastSqrtUnion s;

	for (i = 0; i <= 0x7FFF; i++)
	{

		// Build a float with the bit pattern i as mantissa
		//  and an exponent of 0, stored as 127

		s.i = (i << 8) | (0x7F << 23);
		s.f = (float)sqrt(s.f);

		// Take the square root then strip the first 7 bits of
		//  the mantissa into the table

		fast_sqrt_table[i + 0x8000] = (s.i & 0x7FFFFF);

		// Repeat the process, this time with an exponent of 1,
		//  stored as 128

		s.i = (i << 8) | (0x80 << 23);
		s.f = (float)sqrt(s.f);

		fast_sqrt_table[i] = (s.i & 0x7FFFFF);
	}
}

#define FP_BITS(fp) (*(uint32_t *)&(fp))

inline float fastsqrt(float n)
{

	if (FP_BITS(n) == 0)
		return 0.0;                 // check for square root of 0

	FP_BITS(n) = fast_sqrt_table[(FP_BITS(n) >> 8) & 0xFFFF] | ((((FP_BITS(n) - 0x3F800000) >> 1) + 0x3F800000) & 0x7F800000);

	return n;
}

bool sqrtInitted = false;

float vsSqrt(float n)
{
//	return sqrtf(in);

	if ( !sqrtInitted )
	{
		build_sqrt_table();
		sqrtInitted = true;
	}

	return fastsqrt(n);
}

int vsNextPowerOfTwo( int value )
{
	value--;
	value |= value >> 1;
	value |= value >> 2;
	value |= value >> 4;
	value |= value >> 8;
	value |= value >> 16;
	value++;

	return value;	// it's like magic!
}

bool vsCollideRayVsTriangle( const vsVector3D &orig, const vsVector3D &dir, const vsVector3D &vert0, const vsVector3D &vert1, const vsVector3D &vert2, float *t, float *u, float *v)
{
	const float EPSILON = 0.000001f;

	vsVector3D edge1, edge2, tvec, pvec, qvec;
	float det,inv_det;

	/* find vectors for two edges sharing vert0 */
	edge1 = vert1 - vert0;
	edge2 = vert2 - vert0;

	/* begin calculating determinant - also used to calculate U parameter */
	pvec = dir.Cross(edge2);

	/* if determinant is near zero, ray lies in plane of triangle */
	det = edge1.Dot( pvec );

	if (det > -EPSILON && det < EPSILON)
		return false;
	inv_det = 1.0f / det;

	/* calculate distance from vert0 to ray origin */
	tvec = orig - vert0;

	/* calculate U parameter and test bounds */
	*u = tvec.Dot(pvec) * inv_det;
	if (*u < 0.0f || *u > 1.0f)
		return false;

	/* prepare to test V parameter */
	qvec = tvec.Cross( edge1);

	/* calculate V parameter and test bounds */
	*v = dir.Dot(qvec) * inv_det;
	if (*v < 0.0f || *u + *v > 1.0f)
		return false;

	/* calculate t, ray intersects triangle */
	*t = edge2.Dot(qvec) * inv_det;

	return (*t >= 0.f);
}

