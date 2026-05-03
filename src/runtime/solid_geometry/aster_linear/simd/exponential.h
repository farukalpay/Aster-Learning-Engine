/// @ref simd
/// @file aster_linear/simd/experimental.h

#pragma once

#include "platform.h"

#if ASTER_LINEAR_ARCH & ASTER_LINEAR_ARCH_SSE2_BIT

ASTER_LINEAR_FUNC_QUALIFIER alinear_f32vec4 alinear_vec1_sqrt_lowp(alinear_f32vec4 x)
{
	return _mm_mul_ss(_mm_rsqrt_ss(x), x);
}

ASTER_LINEAR_FUNC_QUALIFIER alinear_f32vec4 alinear_vec4_sqrt_lowp(alinear_f32vec4 x)
{
	return _mm_mul_ps(_mm_rsqrt_ps(x), x);
}

#endif//ASTER_LINEAR_ARCH & ASTER_LINEAR_ARCH_SSE2_BIT
