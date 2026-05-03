/// @ref gtx_matrix_major_storage
/// @file aster_linear/gtx/matrix_major_storage.hpp
///
/// @see core (dependence)
/// @see gtx_extented_min_max (dependence)
///
/// @defgroup gtx_matrix_major_storage ASTER_LINEAR_GTX_matrix_major_storage
/// @ingroup gtx
///
/// Include <aster_linear/gtx/matrix_major_storage.hpp> to use the features of this extension.
///
/// Build matrices with specific matrix order, row or column

#pragma once

// Dependency:
#include "../aster_linear.hpp"

#if ASTER_LINEAR_MESSAGES == ASTER_LINEAR_ENABLE && !defined(ASTER_LINEAR_EXT_INCLUDED)
#	ifndef ASTER_LINEAR_ENABLE_EXPERIMENTAL
#		pragma message("ASTER_LINEAR: ASTER_LINEAR_GTX_matrix_major_storage is an experimental extension and may change in the future. Use #define ASTER_LINEAR_ENABLE_EXPERIMENTAL before including it, if you really want to use it.")
#	else
#		pragma message("ASTER_LINEAR: ASTER_LINEAR_GTX_matrix_major_storage extension included")
#	endif
#endif

namespace aster_linear
{
	/// @addtogroup gtx_matrix_major_storage
	/// @{

	//! Build a row major matrix from row vectors.
	//! From ASTER_LINEAR_GTX_matrix_major_storage extension.
	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL mat<2, 2, T, Q> rowMajor2(
		vec<2, T, Q> const& v1,
		vec<2, T, Q> const& v2);

	//! Build a row major matrix from other matrix.
	//! From ASTER_LINEAR_GTX_matrix_major_storage extension.
	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL mat<2, 2, T, Q> rowMajor2(
		mat<2, 2, T, Q> const& m);

	//! Build a row major matrix from row vectors.
	//! From ASTER_LINEAR_GTX_matrix_major_storage extension.
	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL mat<3, 3, T, Q> rowMajor3(
		vec<3, T, Q> const& v1,
		vec<3, T, Q> const& v2,
		vec<3, T, Q> const& v3);

	//! Build a row major matrix from other matrix.
	//! From ASTER_LINEAR_GTX_matrix_major_storage extension.
	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL mat<3, 3, T, Q> rowMajor3(
		mat<3, 3, T, Q> const& m);

	//! Build a row major matrix from row vectors.
	//! From ASTER_LINEAR_GTX_matrix_major_storage extension.
	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL mat<4, 4, T, Q> rowMajor4(
		vec<4, T, Q> const& v1,
		vec<4, T, Q> const& v2,
		vec<4, T, Q> const& v3,
		vec<4, T, Q> const& v4);

	//! Build a row major matrix from other matrix.
	//! From ASTER_LINEAR_GTX_matrix_major_storage extension.
	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL mat<4, 4, T, Q> rowMajor4(
		mat<4, 4, T, Q> const& m);

	//! Build a column major matrix from column vectors.
	//! From ASTER_LINEAR_GTX_matrix_major_storage extension.
	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL mat<2, 2, T, Q> colMajor2(
		vec<2, T, Q> const& v1,
		vec<2, T, Q> const& v2);

	//! Build a column major matrix from other matrix.
	//! From ASTER_LINEAR_GTX_matrix_major_storage extension.
	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL mat<2, 2, T, Q> colMajor2(
		mat<2, 2, T, Q> const& m);

	//! Build a column major matrix from column vectors.
	//! From ASTER_LINEAR_GTX_matrix_major_storage extension.
	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL mat<3, 3, T, Q> colMajor3(
		vec<3, T, Q> const& v1,
		vec<3, T, Q> const& v2,
		vec<3, T, Q> const& v3);

	//! Build a column major matrix from other matrix.
	//! From ASTER_LINEAR_GTX_matrix_major_storage extension.
	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL mat<3, 3, T, Q> colMajor3(
		mat<3, 3, T, Q> const& m);

	//! Build a column major matrix from column vectors.
	//! From ASTER_LINEAR_GTX_matrix_major_storage extension.
	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL mat<4, 4, T, Q> colMajor4(
		vec<4, T, Q> const& v1,
		vec<4, T, Q> const& v2,
		vec<4, T, Q> const& v3,
		vec<4, T, Q> const& v4);

	//! Build a column major matrix from other matrix.
	//! From ASTER_LINEAR_GTX_matrix_major_storage extension.
	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL mat<4, 4, T, Q> colMajor4(
		mat<4, 4, T, Q> const& m);

	/// @}
}//namespace aster_linear

#include "matrix_major_storage.inl"
