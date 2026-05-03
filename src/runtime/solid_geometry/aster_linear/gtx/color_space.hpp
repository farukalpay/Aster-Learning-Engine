/// @ref gtx_color_space
/// @file aster_linear/gtx/color_space.hpp
///
/// @see core (dependence)
///
/// @defgroup gtx_color_space ASTER_LINEAR_GTX_color_space
/// @ingroup gtx
///
/// Include <aster_linear/gtx/color_space.hpp> to use the features of this extension.
///
/// Related to RGB to HSV conversions and operations.

#pragma once

// Dependency:
#include "../aster_linear.hpp"

#if ASTER_LINEAR_MESSAGES == ASTER_LINEAR_ENABLE && !defined(ASTER_LINEAR_EXT_INCLUDED)
#	ifndef ASTER_LINEAR_ENABLE_EXPERIMENTAL
#		pragma message("ASTER_LINEAR: ASTER_LINEAR_GTX_color_space is an experimental extension and may change in the future. Use #define ASTER_LINEAR_ENABLE_EXPERIMENTAL before including it, if you really want to use it.")
#	else
#		pragma message("ASTER_LINEAR: ASTER_LINEAR_GTX_color_space extension included")
#	endif
#endif

namespace aster_linear
{
	/// @addtogroup gtx_color_space
	/// @{

	/// Converts a color from HSV color space to its color in RGB color space.
	/// @see gtx_color_space
	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL vec<3, T, Q> rgbColor(
		vec<3, T, Q> const& hsvValue);

	/// Converts a color from RGB color space to its color in HSV color space.
	/// @see gtx_color_space
	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL vec<3, T, Q> hsvColor(
		vec<3, T, Q> const& rgbValue);

	/// Build a saturation matrix.
	/// @see gtx_color_space
	template<typename T>
	ASTER_LINEAR_FUNC_DECL mat<4, 4, T, defaultp> saturation(
		T const s);

	/// Modify the saturation of a color.
	/// @see gtx_color_space
	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL vec<3, T, Q> saturation(
		T const s,
		vec<3, T, Q> const& color);

	/// Modify the saturation of a color.
	/// @see gtx_color_space
	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL vec<4, T, Q> saturation(
		T const s,
		vec<4, T, Q> const& color);

	/// Compute color luminosity associating ratios (0.33, 0.59, 0.11) to RGB canals.
	/// @see gtx_color_space
	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL T luminosity(
		vec<3, T, Q> const& color);

	/// @}
}//namespace aster_linear

#include "color_space.inl"
