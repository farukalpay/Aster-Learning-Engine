/// @ref gtx_transform
/// @file aster_linear/gtx/transform.hpp
///
/// @see core (dependence)
/// @see gtc_matrix_transform (dependence)
/// @see gtx_transform
/// @see gtx_transform2
///
/// @defgroup gtx_transform ASTER_LINEAR_GTX_transform
/// @ingroup gtx
///
/// Include <aster_linear/gtx/transform.hpp> to use the features of this extension.
///
/// Add transformation matrices

#pragma once

// Dependency:
#include "../aster_linear.hpp"
#include "../gtc/matrix_transform.hpp"

#if ASTER_LINEAR_MESSAGES == ASTER_LINEAR_ENABLE && !defined(ASTER_LINEAR_EXT_INCLUDED)
#	ifndef ASTER_LINEAR_ENABLE_EXPERIMENTAL
#		pragma message("ASTER_LINEAR: ASTER_LINEAR_GTX_transform is an experimental extension and may change in the future. Use #define ASTER_LINEAR_ENABLE_EXPERIMENTAL before including it, if you really want to use it.")
#	else
#		pragma message("ASTER_LINEAR: ASTER_LINEAR_GTX_transform extension included")
#	endif
#endif

namespace aster_linear
{
	/// @addtogroup gtx_transform
	/// @{

	/// Transforms a matrix with a translation 4 * 4 matrix created from 3 scalars.
	/// @see gtc_matrix_transform
	/// @see gtx_transform
	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL mat<4, 4, T, Q> translate(
		vec<3, T, Q> const& v);

	/// Builds a rotation 4 * 4 matrix created from an axis of 3 scalars and an angle expressed in radians.
	/// @see gtc_matrix_transform
	/// @see gtx_transform
	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL mat<4, 4, T, Q> rotate(
		T angle,
		vec<3, T, Q> const& v);

	/// Transforms a matrix with a scale 4 * 4 matrix created from a vector of 3 components.
	/// @see gtc_matrix_transform
	/// @see gtx_transform
	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL mat<4, 4, T, Q> scale(
		vec<3, T, Q> const& v);

	/// @}
}// namespace aster_linear

#include "transform.inl"
