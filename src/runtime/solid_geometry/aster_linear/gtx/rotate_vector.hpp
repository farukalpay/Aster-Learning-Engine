/// @ref gtx_rotate_vector
/// @file aster_linear/gtx/rotate_vector.hpp
///
/// @see core (dependence)
/// @see gtx_transform (dependence)
///
/// @defgroup gtx_rotate_vector ASTER_LINEAR_GTX_rotate_vector
/// @ingroup gtx
///
/// Include <aster_linear/gtx/rotate_vector.hpp> to use the features of this extension.
///
/// Function to directly rotate a vector

#pragma once

// Dependency:
#include "../gtx/transform.hpp"
#include "../gtc/epsilon.hpp"
#include "../ext/vector_relational.hpp"
#include "../aster_linear.hpp"

#if ASTER_LINEAR_MESSAGES == ASTER_LINEAR_ENABLE && !defined(ASTER_LINEAR_EXT_INCLUDED)
#	ifndef ASTER_LINEAR_ENABLE_EXPERIMENTAL
#		pragma message("ASTER_LINEAR: ASTER_LINEAR_GTX_rotate_vector is an experimental extension and may change in the future. Use #define ASTER_LINEAR_ENABLE_EXPERIMENTAL before including it, if you really want to use it.")
#	else
#		pragma message("ASTER_LINEAR: ASTER_LINEAR_GTX_rotate_vector extension included")
#	endif
#endif

namespace aster_linear
{
	/// @addtogroup gtx_rotate_vector
	/// @{

	/// Returns Spherical interpolation between two vectors
	///
	/// @param x A first vector
	/// @param y A second vector
	/// @param a Interpolation factor. The interpolation is defined beyond the range [0, 1].
	///
	/// @see gtx_rotate_vector
	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL vec<3, T, Q> slerp(
		vec<3, T, Q> const& x,
		vec<3, T, Q> const& y,
		T const& a);

	//! Rotate a two dimensional vector.
	//! From ASTER_LINEAR_GTX_rotate_vector extension.
	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL vec<2, T, Q> rotate(
		vec<2, T, Q> const& v,
		T const& angle);

	//! Rotate a three dimensional vector around an axis.
	//! From ASTER_LINEAR_GTX_rotate_vector extension.
	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL vec<3, T, Q> rotate(
		vec<3, T, Q> const& v,
		T const& angle,
		vec<3, T, Q> const& normal);

	//! Rotate a four dimensional vector around an axis.
	//! From ASTER_LINEAR_GTX_rotate_vector extension.
	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL vec<4, T, Q> rotate(
		vec<4, T, Q> const& v,
		T const& angle,
		vec<3, T, Q> const& normal);

	//! Rotate a three dimensional vector around the X axis.
	//! From ASTER_LINEAR_GTX_rotate_vector extension.
	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL vec<3, T, Q> rotateX(
		vec<3, T, Q> const& v,
		T const& angle);

	//! Rotate a three dimensional vector around the Y axis.
	//! From ASTER_LINEAR_GTX_rotate_vector extension.
	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL vec<3, T, Q> rotateY(
		vec<3, T, Q> const& v,
		T const& angle);

	//! Rotate a three dimensional vector around the Z axis.
	//! From ASTER_LINEAR_GTX_rotate_vector extension.
	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL vec<3, T, Q> rotateZ(
		vec<3, T, Q> const& v,
		T const& angle);

	//! Rotate a four dimensional vector around the X axis.
	//! From ASTER_LINEAR_GTX_rotate_vector extension.
	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL vec<4, T, Q> rotateX(
		vec<4, T, Q> const& v,
		T const& angle);

	//! Rotate a four dimensional vector around the Y axis.
	//! From ASTER_LINEAR_GTX_rotate_vector extension.
	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL vec<4, T, Q> rotateY(
		vec<4, T, Q> const& v,
		T const& angle);

	//! Rotate a four dimensional vector around the Z axis.
	//! From ASTER_LINEAR_GTX_rotate_vector extension.
	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL vec<4, T, Q> rotateZ(
		vec<4, T, Q> const& v,
		T const& angle);

	//! Build a rotation matrix from a normal and a up vector.
	//! From ASTER_LINEAR_GTX_rotate_vector extension.
	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL mat<4, 4, T, Q> orientation(
		vec<3, T, Q> const& Normal,
		vec<3, T, Q> const& Up);

	/// @}
}//namespace aster_linear

#include "rotate_vector.inl"
