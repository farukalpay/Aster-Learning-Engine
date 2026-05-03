/// @ref ext_quaternion_transform
/// @file aster_linear/ext/quaternion_transform.hpp
///
/// @defgroup ext_quaternion_transform ASTER_LINEAR_EXT_quaternion_transform
/// @ingroup ext
///
/// Provides transformation functions for quaternion types
///
/// Include <aster_linear/ext/quaternion_transform.hpp> to use the features of this extension.
///
/// @see ext_quaternion_float
/// @see ext_quaternion_double
/// @see ext_quaternion_exponential
/// @see ext_quaternion_geometric
/// @see ext_quaternion_relational
/// @see ext_quaternion_trigonometric

#pragma once

// Dependency:
#include "../common.hpp"
#include "../trigonometric.hpp"
#include "../geometric.hpp"

#if ASTER_LINEAR_MESSAGES == ASTER_LINEAR_ENABLE && !defined(ASTER_LINEAR_EXT_INCLUDED)
#	pragma message("ASTER_LINEAR: ASTER_LINEAR_EXT_quaternion_transform extension included")
#endif

namespace aster_linear
{
	/// @addtogroup ext_quaternion_transform
	/// @{

	/// Rotates a quaternion from a vector of 3 components axis and an angle.
	///
	/// @param q Source orientation
	/// @param angle Angle expressed in radians.
	/// @param axis Axis of the rotation
	///
	/// @tparam T Floating-point scalar types
	/// @tparam Q Value from qualifier enum
	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL qua<T, Q> rotate(qua<T, Q> const& q, T const& angle, vec<3, T, Q> const& axis);
	/// @}
} //namespace aster_linear

#include "quaternion_transform.inl"
