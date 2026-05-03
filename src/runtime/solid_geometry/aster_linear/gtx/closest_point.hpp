/// @ref gtx_closest_point
/// @file aster_linear/gtx/closest_point.hpp
///
/// @see core (dependence)
///
/// @defgroup gtx_closest_point ASTER_LINEAR_GTX_closest_point
/// @ingroup gtx
///
/// Include <aster_linear/gtx/closest_point.hpp> to use the features of this extension.
///
/// Find the point on a straight line which is the closet of a point.

#pragma once

// Dependency:
#include "../aster_linear.hpp"

#if ASTER_LINEAR_MESSAGES == ASTER_LINEAR_ENABLE && !defined(ASTER_LINEAR_EXT_INCLUDED)
#	ifndef ASTER_LINEAR_ENABLE_EXPERIMENTAL
#		pragma message("ASTER_LINEAR: ASTER_LINEAR_GTX_closest_point is an experimental extension and may change in the future. Use #define ASTER_LINEAR_ENABLE_EXPERIMENTAL before including it, if you really want to use it.")
#	else
#		pragma message("ASTER_LINEAR: ASTER_LINEAR_GTX_closest_point extension included")
#	endif
#endif

namespace aster_linear
{
	/// @addtogroup gtx_closest_point
	/// @{

	/// Find the point on a straight line which is the closet of a point.
	/// @see gtx_closest_point
	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL vec<3, T, Q> closestPointOnLine(
		vec<3, T, Q> const& point,
		vec<3, T, Q> const& a,
		vec<3, T, Q> const& b);

	/// 2d lines work as well
	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL vec<2, T, Q> closestPointOnLine(
		vec<2, T, Q> const& point,
		vec<2, T, Q> const& a,
		vec<2, T, Q> const& b);

	/// @}
}// namespace aster_linear

#include "closest_point.inl"
