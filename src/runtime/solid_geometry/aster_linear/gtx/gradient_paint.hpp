/// @ref gtx_gradient_paint
/// @file aster_linear/gtx/gradient_paint.hpp
///
/// @see core (dependence)
/// @see gtx_optimum_pow (dependence)
///
/// @defgroup gtx_gradient_paint ASTER_LINEAR_GTX_gradient_paint
/// @ingroup gtx
///
/// Include <aster_linear/gtx/gradient_paint.hpp> to use the features of this extension.
///
/// Functions that return the color of procedural gradient for specific coordinates.

#pragma once

// Dependency:
#include "../aster_linear.hpp"
#include "../gtx/optimum_pow.hpp"

#if ASTER_LINEAR_MESSAGES == ASTER_LINEAR_ENABLE && !defined(ASTER_LINEAR_EXT_INCLUDED)
#	ifndef ASTER_LINEAR_ENABLE_EXPERIMENTAL
#		pragma message("ASTER_LINEAR: ASTER_LINEAR_GTX_gradient_paint is an experimental extension and may change in the future. Use #define ASTER_LINEAR_ENABLE_EXPERIMENTAL before including it, if you really want to use it.")
#	else
#		pragma message("ASTER_LINEAR: ASTER_LINEAR_GTX_gradient_paint extension included")
#	endif
#endif

namespace aster_linear
{
	/// @addtogroup gtx_gradient_paint
	/// @{

	/// Return a color from a radial gradient.
	/// @see - gtx_gradient_paint
	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL T radialGradient(
		vec<2, T, Q> const& Center,
		T const& Radius,
		vec<2, T, Q> const& Focal,
		vec<2, T, Q> const& Position);

	/// Return a color from a linear gradient.
	/// @see - gtx_gradient_paint
	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL T linearGradient(
		vec<2, T, Q> const& Point0,
		vec<2, T, Q> const& Point1,
		vec<2, T, Q> const& Position);

	/// @}
}// namespace aster_linear

#include "gradient_paint.inl"
