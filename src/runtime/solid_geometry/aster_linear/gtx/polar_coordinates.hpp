/// @ref gtx_polar_coordinates
/// @file aster_linear/gtx/polar_coordinates.hpp
///
/// @see core (dependence)
///
/// @defgroup gtx_polar_coordinates ASTER_LINEAR_GTX_polar_coordinates
/// @ingroup gtx
///
/// Include <aster_linear/gtx/polar_coordinates.hpp> to use the features of this extension.
///
/// Conversion from Euclidean space to polar space and revert.

#pragma once

// Dependency:
#include "../aster_linear.hpp"

#if ASTER_LINEAR_MESSAGES == ASTER_LINEAR_ENABLE && !defined(ASTER_LINEAR_EXT_INCLUDED)
#	ifndef ASTER_LINEAR_ENABLE_EXPERIMENTAL
#		pragma message("ASTER_LINEAR: ASTER_LINEAR_GTX_polar_coordinates is an experimental extension and may change in the future. Use #define ASTER_LINEAR_ENABLE_EXPERIMENTAL before including it, if you really want to use it.")
#	else
#		pragma message("ASTER_LINEAR: ASTER_LINEAR_GTX_polar_coordinates extension included")
#	endif
#endif

namespace aster_linear
{
	/// @addtogroup gtx_polar_coordinates
	/// @{

	/// Convert Euclidean to Polar coordinates, x is the latitude, y the longitude and z the xz distance.
	///
	/// @see gtx_polar_coordinates
	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL vec<3, T, Q> polar(
		vec<3, T, Q> const& euclidean);

	/// Convert Polar to Euclidean coordinates.
	///
	/// @see gtx_polar_coordinates
	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL vec<3, T, Q> euclidean(
		vec<2, T, Q> const& polar);

	/// @}
}//namespace aster_linear

#include "polar_coordinates.inl"
