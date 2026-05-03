/// @ref gtx_functions
/// @file aster_linear/gtx/functions.hpp
///
/// @see core (dependence)
/// @see gtc_quaternion (dependence)
///
/// @defgroup gtx_functions ASTER_LINEAR_GTX_functions
/// @ingroup gtx
///
/// Include <aster_linear/gtx/functions.hpp> to use the features of this extension.
///
/// List of useful common functions.

#pragma once

// Dependencies
#include "../detail/setup.hpp"
#include "../detail/qualifier.hpp"
#include "../detail/type_vec2.hpp"

#if ASTER_LINEAR_MESSAGES == ASTER_LINEAR_ENABLE && !defined(ASTER_LINEAR_EXT_INCLUDED)
#	ifndef ASTER_LINEAR_ENABLE_EXPERIMENTAL
#		pragma message("ASTER_LINEAR: ASTER_LINEAR_GTX_functions is an experimental extension and may change in the future. Use #define ASTER_LINEAR_ENABLE_EXPERIMENTAL before including it, if you really want to use it.")
#	else
#		pragma message("ASTER_LINEAR: ASTER_LINEAR_GTX_functions extension included")
#	endif
#endif

namespace aster_linear
{
	/// @addtogroup gtx_functions
	/// @{

	/// 1D gauss function
	///
	/// @see gtc_epsilon
	template<typename T>
	ASTER_LINEAR_FUNC_DECL T gauss(
		T x,
		T ExpectedValue,
		T StandardDeviation);

	/// 2D gauss function
	///
	/// @see gtc_epsilon
	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL T gauss(
		vec<2, T, Q> const& Coord,
		vec<2, T, Q> const& ExpectedValue,
		vec<2, T, Q> const& StandardDeviation);

	/// @}
}//namespace aster_linear

#include "functions.inl"

