#pragma once

#include "setup.hpp"
#include <limits>

namespace aster_linear{
namespace detail
{
	template<typename genFIType, bool /*signed*/>
	struct compute_abs
	{};

	template<typename genFIType>
	struct compute_abs<genFIType, true>
	{
		ASTER_LINEAR_FUNC_QUALIFIER ASTER_LINEAR_CONSTEXPR static genFIType call(genFIType x)
		{
			ASTER_LINEAR_STATIC_ASSERT(
				std::numeric_limits<genFIType>::is_iec559 || std::numeric_limits<genFIType>::is_signed,
				"'abs' only accept floating-point and integer scalar or vector inputs");

			return x >= genFIType(0) ? x : -x;
			// TODO, perf comp with: *(((int *) &x) + 1) &= 0x7fffffff;
		}
	};

#if (ASTER_LINEAR_COMPILER & ASTER_LINEAR_COMPILER_CUDA) || (ASTER_LINEAR_COMPILER & ASTER_LINEAR_COMPILER_HIP)
	template<>
	struct compute_abs<float, true>
	{
		ASTER_LINEAR_FUNC_QUALIFIER ASTER_LINEAR_CONSTEXPR static float call(float x)
		{
			return fabsf(x);
		}
	};
#endif

	template<typename genFIType>
	struct compute_abs<genFIType, false>
	{
		ASTER_LINEAR_FUNC_QUALIFIER ASTER_LINEAR_CONSTEXPR static genFIType call(genFIType x)
		{
			ASTER_LINEAR_STATIC_ASSERT(
				(!std::numeric_limits<genFIType>::is_signed && std::numeric_limits<genFIType>::is_integer),
				"'abs' only accept floating-point and integer scalar or vector inputs");
			return x;
		}
	};
}//namespace detail
}//namespace aster_linear
