/// @ref core
/// @file aster_linear/detail/func_exponential_simd.inl

#include "../simd/exponential.h"

#if ASTER_LINEAR_ARCH & ASTER_LINEAR_ARCH_SSE2_BIT

namespace aster_linear{
namespace detail
{
	template<qualifier Q>
	struct compute_sqrt<4, float, Q, true>
	{
		ASTER_LINEAR_FUNC_QUALIFIER static vec<4, float, Q> call(vec<4, float, Q> const& v)
		{
			vec<4, float, Q> Result;
			Result.data = _mm_sqrt_ps(v.data);
			return Result;
		}
	};

#	if ASTER_LINEAR_CONFIG_ALIGNED_GENTYPES == ASTER_LINEAR_ENABLE
	template<>
	struct compute_sqrt<4, float, aligned_lowp, true>
	{
		ASTER_LINEAR_FUNC_QUALIFIER static vec<4, float, aligned_lowp> call(vec<4, float, aligned_lowp> const& v)
		{
			vec<4, float, aligned_lowp> Result;
			Result.data = alinear_vec4_sqrt_lowp(v.data);
			return Result;
		}
	};
#	endif
}//namespace detail
}//namespace aster_linear

#endif//ASTER_LINEAR_ARCH & ASTER_LINEAR_ARCH_SSE2_BIT
