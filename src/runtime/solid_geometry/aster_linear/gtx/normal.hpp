/// @ref gtx_normal
/// @file aster_linear/gtx/normal.hpp
///
/// @see core (dependence)
/// @see gtx_extented_min_max (dependence)
///
/// @defgroup gtx_normal ASTER_LINEAR_GTX_normal
/// @ingroup gtx
///
/// Include <aster_linear/gtx/normal.hpp> to use the features of this extension.
///
/// Compute the normal of a triangle.

#pragma once

// Dependency:
#include "../aster_linear.hpp"

#if ASTER_LINEAR_MESSAGES == ASTER_LINEAR_ENABLE && !defined(ASTER_LINEAR_EXT_INCLUDED)
#	ifndef ASTER_LINEAR_ENABLE_EXPERIMENTAL
#		pragma message("ASTER_LINEAR: ASTER_LINEAR_GTX_normal is an experimental extension and may change in the future. Use #define ASTER_LINEAR_ENABLE_EXPERIMENTAL before including it, if you really want to use it.")
#	else
#		pragma message("ASTER_LINEAR: ASTER_LINEAR_GTX_normal extension included")
#	endif
#endif

namespace aster_linear
{
	/// @addtogroup gtx_normal
	/// @{

	/// Computes triangle normal from triangle points.
	///
	/// @see gtx_normal
	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL vec<3, T, Q> triangleNormal(vec<3, T, Q> const& p1, vec<3, T, Q> const& p2, vec<3, T, Q> const& p3);

	/// @}
}//namespace aster_linear

#include "normal.inl"
