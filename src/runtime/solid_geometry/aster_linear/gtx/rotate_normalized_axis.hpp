/// @ref gtx_rotate_normalized_axis
/// @file aster_linear/gtx/rotate_normalized_axis.hpp
///
/// @see core (dependence)
/// @see gtc_matrix_transform
/// @see gtc_quaternion
///
/// @defgroup gtx_rotate_normalized_axis ASTER_LINEAR_GTX_rotate_normalized_axis
/// @ingroup gtx
///
/// Include <aster_linear/gtx/rotate_normalized_axis.hpp> to use the features of this extension.
///
/// Quaternions and matrices rotations around normalized axis.

#pragma once

// Dependency:
#include "../aster_linear.hpp"
#include "../gtc/epsilon.hpp"
#include "../gtc/quaternion.hpp"

#if ASTER_LINEAR_MESSAGES == ASTER_LINEAR_ENABLE && !defined(ASTER_LINEAR_EXT_INCLUDED)
#	ifndef ASTER_LINEAR_ENABLE_EXPERIMENTAL
#		pragma message("ASTER_LINEAR: ASTER_LINEAR_GTX_rotate_normalized_axis is an experimental extension and may change in the future. Use #define ASTER_LINEAR_ENABLE_EXPERIMENTAL before including it, if you really want to use it.")
#	else
#		pragma message("ASTER_LINEAR: ASTER_LINEAR_GTX_rotate_normalized_axis extension included")
#	endif
#endif

namespace aster_linear
{
	/// @addtogroup gtx_rotate_normalized_axis
	/// @{

	/// Builds a rotation 4 * 4 matrix created from a normalized axis and an angle.
	///
	/// @param m Input matrix multiplied by this rotation matrix.
	/// @param angle Rotation angle expressed in radians.
	/// @param axis Rotation axis, must be normalized.
	/// @tparam T Value type used to build the matrix. Currently supported: half (not recommended), float or double.
	///
	/// @see gtx_rotate_normalized_axis
	/// @see - rotate(T angle, T x, T y, T z)
	/// @see - rotate(mat<4, 4, T, Q> const& m, T angle, T x, T y, T z)
	/// @see - rotate(T angle, vec<3, T, Q> const& v)
	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL mat<4, 4, T, Q> rotateNormalizedAxis(
		mat<4, 4, T, Q> const& m,
		T const& angle,
		vec<3, T, Q> const& axis);

	/// Rotates a quaternion from a vector of 3 components normalized axis and an angle.
	///
	/// @param q Source orientation
	/// @param angle Angle expressed in radians.
	/// @param axis Normalized axis of the rotation, must be normalized.
	///
	/// @see gtx_rotate_normalized_axis
	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL qua<T, Q> rotateNormalizedAxis(
		qua<T, Q> const& q,
		T const& angle,
		vec<3, T, Q> const& axis);

	/// @}
}//namespace aster_linear

#include "rotate_normalized_axis.inl"
