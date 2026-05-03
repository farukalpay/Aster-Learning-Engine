/// @ref simd
/// @file aster_linear/simd/geometric.h

#pragma once

#include "common.h"

#if ASTER_LINEAR_ARCH & ASTER_LINEAR_ARCH_SSE2_BIT

ASTER_LINEAR_FUNC_DECL alinear_vec4 alinear_vec4_dot(alinear_vec4 v1, alinear_vec4 v2);
ASTER_LINEAR_FUNC_DECL alinear_vec4 alinear_vec1_dot(alinear_vec4 v1, alinear_vec4 v2);

ASTER_LINEAR_FUNC_QUALIFIER alinear_vec4 alinear_vec4_length(alinear_vec4 x)
{
	alinear_vec4 const dot0 = alinear_vec4_dot(x, x);
	alinear_vec4 const sqt0 = _mm_sqrt_ps(dot0);
	return sqt0;
}

ASTER_LINEAR_FUNC_QUALIFIER alinear_vec4 alinear_vec4_distance(alinear_vec4 p0, alinear_vec4 p1)
{
	alinear_vec4 const sub0 = _mm_sub_ps(p0, p1);
	alinear_vec4 const len0 = alinear_vec4_length(sub0);
	return len0;
}

ASTER_LINEAR_FUNC_QUALIFIER alinear_vec4 alinear_vec4_dot(alinear_vec4 v1, alinear_vec4 v2)
{
#	if ASTER_LINEAR_ARCH & ASTER_LINEAR_ARCH_AVX_BIT
		return _mm_dp_ps(v1, v2, 0xff);
#	elif ASTER_LINEAR_ARCH & ASTER_LINEAR_ARCH_SSE3_BIT
		alinear_vec4 const mul0 = _mm_mul_ps(v1, v2);
		alinear_vec4 const hadd0 = _mm_hadd_ps(mul0, mul0);
		alinear_vec4 const hadd1 = _mm_hadd_ps(hadd0, hadd0);
		return hadd1;
#	else
		alinear_vec4 const mul0 = _mm_mul_ps(v1, v2);
		alinear_vec4 const swp0 = _mm_shuffle_ps(mul0, mul0, _MM_SHUFFLE(2, 3, 0, 1));
		alinear_vec4 const add0 = _mm_add_ps(mul0, swp0);
		alinear_vec4 const swp1 = _mm_shuffle_ps(add0, add0, _MM_SHUFFLE(0, 1, 2, 3));
		alinear_vec4 const add1 = _mm_add_ps(add0, swp1);
		return add1;
#	endif
}

ASTER_LINEAR_FUNC_QUALIFIER alinear_vec4 alinear_vec1_dot(alinear_vec4 v1, alinear_vec4 v2)
{
#	if ASTER_LINEAR_ARCH & ASTER_LINEAR_ARCH_AVX_BIT
		return _mm_dp_ps(v1, v2, 0xff);
#	elif ASTER_LINEAR_ARCH & ASTER_LINEAR_ARCH_SSE3_BIT
		alinear_vec4 const mul0 = _mm_mul_ps(v1, v2);
		alinear_vec4 const had0 = _mm_hadd_ps(mul0, mul0);
		alinear_vec4 const had1 = _mm_hadd_ps(had0, had0);
		return had1;
#	else
		alinear_vec4 const mul0 = _mm_mul_ps(v1, v2);
		alinear_vec4 const mov0 = _mm_movehl_ps(mul0, mul0);
		alinear_vec4 const add0 = _mm_add_ps(mov0, mul0);
		alinear_vec4 const swp1 = _mm_shuffle_ps(add0, add0, 1);
		alinear_vec4 const add1 = _mm_add_ss(add0, swp1);
		return add1;
#	endif
}

ASTER_LINEAR_FUNC_QUALIFIER alinear_vec4 alinear_vec4_cross(alinear_vec4 v1, alinear_vec4 v2)
{
	alinear_vec4 const swp0 = _mm_shuffle_ps(v1, v1, _MM_SHUFFLE(3, 0, 2, 1));
	alinear_vec4 const swp1 = _mm_shuffle_ps(v1, v1, _MM_SHUFFLE(3, 1, 0, 2));
	alinear_vec4 const swp2 = _mm_shuffle_ps(v2, v2, _MM_SHUFFLE(3, 0, 2, 1));
	alinear_vec4 const swp3 = _mm_shuffle_ps(v2, v2, _MM_SHUFFLE(3, 1, 0, 2));
	alinear_vec4 const mul0 = _mm_mul_ps(swp0, swp3);
	alinear_vec4 const mul1 = _mm_mul_ps(swp1, swp2);
	alinear_vec4 const sub0 = _mm_sub_ps(mul0, mul1);
	return sub0;
}

ASTER_LINEAR_FUNC_QUALIFIER alinear_vec4 alinear_vec4_normalize(alinear_vec4 v)
{
	alinear_vec4 const dot0 = alinear_vec4_dot(v, v);
	alinear_vec4 const isr0 = _mm_rsqrt_ps(dot0);
	alinear_vec4 const mul0 = _mm_mul_ps(v, isr0);
	return mul0;
}

ASTER_LINEAR_FUNC_QUALIFIER alinear_vec4 alinear_vec4_faceforward(alinear_vec4 N, alinear_vec4 I, alinear_vec4 Nref)
{
	alinear_vec4 const dot0 = alinear_vec4_dot(Nref, I);
	alinear_vec4 const sgn0 = alinear_vec4_sign(dot0);
	alinear_vec4 const mul0 = _mm_mul_ps(sgn0, _mm_set1_ps(-1.0f));
	alinear_vec4 const mul1 = _mm_mul_ps(N, mul0);
	return mul1;
}

ASTER_LINEAR_FUNC_QUALIFIER alinear_vec4 alinear_vec4_reflect(alinear_vec4 I, alinear_vec4 N)
{
	alinear_vec4 const dot0 = alinear_vec4_dot(N, I);
	alinear_vec4 const mul0 = _mm_mul_ps(N, dot0);
	alinear_vec4 const mul1 = _mm_mul_ps(mul0, _mm_set1_ps(2.0f));
	alinear_vec4 const sub0 = _mm_sub_ps(I, mul1);
	return sub0;
}

ASTER_LINEAR_FUNC_QUALIFIER __m128 alinear_vec4_refract(alinear_vec4 I, alinear_vec4 N, alinear_vec4 eta)
{
	alinear_vec4 const dot0 = alinear_vec4_dot(N, I);
	alinear_vec4 const mul0 = _mm_mul_ps(eta, eta);
	alinear_vec4 const mul1 = _mm_mul_ps(dot0, dot0);
	alinear_vec4 const sub0 = _mm_sub_ps(_mm_set1_ps(1.0f), mul0);
	alinear_vec4 const sub1 = _mm_sub_ps(_mm_set1_ps(1.0f), mul1);
	alinear_vec4 const mul2 = _mm_mul_ps(sub0, sub1);

	if(_mm_movemask_ps(_mm_cmplt_ss(mul2, _mm_set1_ps(0.0f))) == 0)
		return _mm_set1_ps(0.0f);

	alinear_vec4 const sqt0 = _mm_sqrt_ps(mul2);
	alinear_vec4 const mad0 = alinear_vec4_fma(eta, dot0, sqt0);
	alinear_vec4 const mul4 = _mm_mul_ps(mad0, N);
	alinear_vec4 const mul5 = _mm_mul_ps(eta, I);
	alinear_vec4 const sub2 = _mm_sub_ps(mul5, mul4);

	return sub2;
}

#endif//ASTER_LINEAR_ARCH & ASTER_LINEAR_ARCH_SSE2_BIT
