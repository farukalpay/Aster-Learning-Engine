/// @ref gtx_transform2
/// @file aster_linear/gtx/transform2.hpp
///
/// @see core (dependence)
/// @see gtx_transform (dependence)
///
/// @defgroup gtx_transform2 ASTER_LINEAR_GTX_transform2
/// @ingroup gtx
///
/// Include <aster_linear/gtx/transform2.hpp> to use the features of this extension.
///
/// Add extra transformation matrices

#pragma once

// Dependency:
#include "../aster_linear.hpp"
#include "../gtx/transform.hpp"

#if ASTER_LINEAR_MESSAGES == ASTER_LINEAR_ENABLE && !defined(ASTER_LINEAR_EXT_INCLUDED)
#	ifndef ASTER_LINEAR_ENABLE_EXPERIMENTAL
#		pragma message("ASTER_LINEAR: ASTER_LINEAR_GTX_transform2 is an experimental extension and may change in the future. Use #define ASTER_LINEAR_ENABLE_EXPERIMENTAL before including it, if you really want to use it.")
#	else
#		pragma message("ASTER_LINEAR: ASTER_LINEAR_GTX_transform2 extension included")
#	endif
#endif

namespace aster_linear
{
	/// @addtogroup gtx_transform2
	/// @{

	//! Transforms a matrix with a shearing on X axis.
	//! From ASTER_LINEAR_GTX_transform2 extension.
	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL mat<3, 3, T, Q> shearX2D(mat<3, 3, T, Q> const& m, T y);

	//! Transforms a matrix with a shearing on Y axis.
	//! From ASTER_LINEAR_GTX_transform2 extension.
	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL mat<3, 3, T, Q> shearY2D(mat<3, 3, T, Q> const& m, T x);

	//! Transforms a matrix with a shearing on X axis
	//! From ASTER_LINEAR_GTX_transform2 extension.
	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL mat<4, 4, T, Q> shearX3D(mat<4, 4, T, Q> const& m, T y, T z);

	//! Transforms a matrix with a shearing on Y axis.
	//! From ASTER_LINEAR_GTX_transform2 extension.
	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL mat<4, 4, T, Q> shearY3D(mat<4, 4, T, Q> const& m, T x, T z);

	//! Transforms a matrix with a shearing on Z axis.
	//! From ASTER_LINEAR_GTX_transform2 extension.
	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL mat<4, 4, T, Q> shearZ3D(mat<4, 4, T, Q> const& m, T x, T y);

	//template<typename T> ASTER_LINEAR_FUNC_QUALIFIER mat<4, 4, T, Q> shear(const mat<4, 4, T, Q> & m, shearPlane, planePoint, angle)
	// Identity + tan(angle) * cross(Normal, OnPlaneVector)     0
	// - dot(PointOnPlane, normal) * OnPlaneVector              1

	// Reflect functions seem to don't work
	//template<typename T> mat<3, 3, T, Q> reflect2D(const mat<3, 3, T, Q> & m, const vec<3, T, Q>& normal){return reflect2DGTX(m, normal);}									//!< \brief Build a reflection matrix (from ASTER_LINEAR_GTX_transform2 extension)
	//template<typename T> mat<4, 4, T, Q> reflect3D(const mat<4, 4, T, Q> & m, const vec<3, T, Q>& normal){return reflect3DGTX(m, normal);}									//!< \brief Build a reflection matrix (from ASTER_LINEAR_GTX_transform2 extension)

	//! Build planar projection matrix along normal axis.
	//! From ASTER_LINEAR_GTX_transform2 extension.
	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL mat<3, 3, T, Q> proj2D(mat<3, 3, T, Q> const& m, vec<3, T, Q> const& normal);

	//! Build planar projection matrix along normal axis.
	//! From ASTER_LINEAR_GTX_transform2 extension.
	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL mat<4, 4, T, Q> proj3D(mat<4, 4, T, Q> const & m, vec<3, T, Q> const& normal);

	//! Build a scale bias matrix.
	//! From ASTER_LINEAR_GTX_transform2 extension.
	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL mat<4, 4, T, Q> scaleBias(T scale, T bias);

	//! Build a scale bias matrix.
	//! From ASTER_LINEAR_GTX_transform2 extension.
	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL mat<4, 4, T, Q> scaleBias(mat<4, 4, T, Q> const& m, T scale, T bias);

	/// @}
}// namespace aster_linear

#include "transform2.inl"
