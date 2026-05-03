/// @ref gtx_fast_exponential
/// @file aster_linear/gtx/fast_exponential.hpp
///
/// @see core (dependence)
/// @see gtx_half_float (dependence)
///
/// @defgroup gtx_fast_exponential ASTER_LINEAR_GTX_fast_exponential
/// @ingroup gtx
///
/// Include <aster_linear/gtx/fast_exponential.hpp> to use the features of this extension.
///
/// Fast but less accurate implementations of exponential based functions.

#pragma once

// Dependency:
#include "../aster_linear.hpp"

#if ASTER_LINEAR_MESSAGES == ASTER_LINEAR_ENABLE && !defined(ASTER_LINEAR_EXT_INCLUDED)
#	ifndef ASTER_LINEAR_ENABLE_EXPERIMENTAL
#		pragma message("ASTER_LINEAR: ASTER_LINEAR_GTX_fast_exponential is an experimental extension and may change in the future. Use #define ASTER_LINEAR_ENABLE_EXPERIMENTAL before including it, if you really want to use it.")
#	else
#		pragma message("ASTER_LINEAR: ASTER_LINEAR_GTX_fast_exponential extension included")
#	endif
#endif

namespace aster_linear
{
	/// @addtogroup gtx_fast_exponential
	/// @{

	/// Faster than the common pow function but less accurate.
	/// @see gtx_fast_exponential
	template<typename genType>
	ASTER_LINEAR_FUNC_DECL genType fastPow(genType x, genType y);

	/// Faster than the common pow function but less accurate.
	/// @see gtx_fast_exponential
	template<length_t L, typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL vec<L, T, Q> fastPow(vec<L, T, Q> const& x, vec<L, T, Q> const& y);

	/// Faster than the common pow function but less accurate.
	/// @see gtx_fast_exponential
	template<typename genTypeT, typename genTypeU>
	ASTER_LINEAR_FUNC_DECL genTypeT fastPow(genTypeT x, genTypeU y);

	/// Faster than the common pow function but less accurate.
	/// @see gtx_fast_exponential
	template<length_t L, typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL vec<L, T, Q> fastPow(vec<L, T, Q> const& x);

	/// Faster than the common exp function but less accurate.
	/// @see gtx_fast_exponential
	template<typename T>
	ASTER_LINEAR_FUNC_DECL T fastExp(T x);

	/// Faster than the common exp function but less accurate.
	/// @see gtx_fast_exponential
	template<length_t L, typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL vec<L, T, Q> fastExp(vec<L, T, Q> const& x);

	/// Faster than the common log function but less accurate.
	/// @see gtx_fast_exponential
	template<typename T>
	ASTER_LINEAR_FUNC_DECL T fastLog(T x);

	/// Faster than the common exp2 function but less accurate.
	/// @see gtx_fast_exponential
	template<length_t L, typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL vec<L, T, Q> fastLog(vec<L, T, Q> const& x);

	/// Faster than the common exp2 function but less accurate.
	/// @see gtx_fast_exponential
	template<typename T>
	ASTER_LINEAR_FUNC_DECL T fastExp2(T x);

	/// Faster than the common exp2 function but less accurate.
	/// @see gtx_fast_exponential
	template<length_t L, typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL vec<L, T, Q> fastExp2(vec<L, T, Q> const& x);

	/// Faster than the common log2 function but less accurate.
	/// @see gtx_fast_exponential
	template<typename T>
	ASTER_LINEAR_FUNC_DECL T fastLog2(T x);

	/// Faster than the common log2 function but less accurate.
	/// @see gtx_fast_exponential
	template<length_t L, typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL vec<L, T, Q> fastLog2(vec<L, T, Q> const& x);

	/// @}
}//namespace aster_linear

#include "fast_exponential.inl"
