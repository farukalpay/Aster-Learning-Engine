/// @ref gtx_matrix_query
/// @file aster_linear/gtx/matrix_query.hpp
///
/// @see core (dependence)
/// @see gtx_vector_query (dependence)
///
/// @defgroup gtx_matrix_query ASTER_LINEAR_GTX_matrix_query
/// @ingroup gtx
///
/// Include <aster_linear/gtx/matrix_query.hpp> to use the features of this extension.
///
/// Query to evaluate matrix properties

#pragma once

// Dependency:
#include "../aster_linear.hpp"
#include "../gtx/vector_query.hpp"
#include <limits>

#if ASTER_LINEAR_MESSAGES == ASTER_LINEAR_ENABLE && !defined(ASTER_LINEAR_EXT_INCLUDED)
#	ifndef ASTER_LINEAR_ENABLE_EXPERIMENTAL
#		pragma message("ASTER_LINEAR: ASTER_LINEAR_GTX_matrix_query is an experimental extension and may change in the future. Use #define ASTER_LINEAR_ENABLE_EXPERIMENTAL before including it, if you really want to use it.")
#	else
#		pragma message("ASTER_LINEAR: ASTER_LINEAR_GTX_matrix_query extension included")
#	endif
#endif

namespace aster_linear
{
	/// @addtogroup gtx_matrix_query
	/// @{

	/// Return whether a matrix a null matrix.
	/// From ASTER_LINEAR_GTX_matrix_query extension.
	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL bool isNull(mat<2, 2, T, Q> const& m, T const& epsilon);

	/// Return whether a matrix a null matrix.
	/// From ASTER_LINEAR_GTX_matrix_query extension.
	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL bool isNull(mat<3, 3, T, Q> const& m, T const& epsilon);

	/// Return whether a matrix is a null matrix.
	/// From ASTER_LINEAR_GTX_matrix_query extension.
	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL bool isNull(mat<4, 4, T, Q> const& m, T const& epsilon);

	/// Return whether a matrix is an identity matrix.
	/// From ASTER_LINEAR_GTX_matrix_query extension.
	template<length_t C, length_t R, typename T, qualifier Q, template<length_t, length_t, typename, qualifier> class matType>
	ASTER_LINEAR_FUNC_DECL bool isIdentity(matType<C, R, T, Q> const& m, T const& epsilon);

	/// Return whether a matrix is a normalized matrix.
	/// From ASTER_LINEAR_GTX_matrix_query extension.
	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL bool isNormalized(mat<2, 2, T, Q> const& m, T const& epsilon);

	/// Return whether a matrix is a normalized matrix.
	/// From ASTER_LINEAR_GTX_matrix_query extension.
	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL bool isNormalized(mat<3, 3, T, Q> const& m, T const& epsilon);

	/// Return whether a matrix is a normalized matrix.
	/// From ASTER_LINEAR_GTX_matrix_query extension.
	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL bool isNormalized(mat<4, 4, T, Q> const& m, T const& epsilon);

	/// Return whether a matrix is an orthonormalized matrix.
	/// From ASTER_LINEAR_GTX_matrix_query extension.
	template<length_t C, length_t R, typename T, qualifier Q, template<length_t, length_t, typename, qualifier> class matType>
	ASTER_LINEAR_FUNC_DECL bool isOrthogonal(matType<C, R, T, Q> const& m, T const& epsilon);

	/// @}
}//namespace aster_linear

#include "matrix_query.inl"
