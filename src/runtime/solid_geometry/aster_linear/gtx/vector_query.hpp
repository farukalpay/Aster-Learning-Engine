/// @ref gtx_vector_query
/// @file aster_linear/gtx/vector_query.hpp
///
/// @see core (dependence)
///
/// @defgroup gtx_vector_query ASTER_LINEAR_GTX_vector_query
/// @ingroup gtx
///
/// Include <aster_linear/gtx/vector_query.hpp> to use the features of this extension.
///
/// Query informations of vector types

#pragma once

// Dependency:
#include "../aster_linear.hpp"
#include <cfloat>
#include <limits>

#if ASTER_LINEAR_MESSAGES == ASTER_LINEAR_ENABLE && !defined(ASTER_LINEAR_EXT_INCLUDED)
#	ifndef ASTER_LINEAR_ENABLE_EXPERIMENTAL
#		pragma message("ASTER_LINEAR: ASTER_LINEAR_GTX_vector_query is an experimental extension and may change in the future. Use #define ASTER_LINEAR_ENABLE_EXPERIMENTAL before including it, if you really want to use it.")
#	else
#		pragma message("ASTER_LINEAR: ASTER_LINEAR_GTX_vector_query extension included")
#	endif
#endif

namespace aster_linear
{
	/// @addtogroup gtx_vector_query
	/// @{

	//! Check whether two vectors are collinears.
	/// @see gtx_vector_query extensions.
	template<length_t L, typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL bool areCollinear(vec<L, T, Q> const& v0, vec<L, T, Q> const& v1, T const& epsilon);

	//! Check whether two vectors are orthogonals.
	/// @see gtx_vector_query extensions.
	template<length_t L, typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL bool areOrthogonal(vec<L, T, Q> const& v0, vec<L, T, Q> const& v1, T const& epsilon);

	//! Check whether a vector is normalized.
	/// @see gtx_vector_query extensions.
	template<length_t L, typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL bool isNormalized(vec<L, T, Q> const& v, T const& epsilon);

	//! Check whether a vector is null.
	/// @see gtx_vector_query extensions.
	template<length_t L, typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL bool isNull(vec<L, T, Q> const& v, T const& epsilon);

	//! Check whether a each component of a vector is null.
	/// @see gtx_vector_query extensions.
	template<length_t L, typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL vec<L, bool, Q> isCompNull(vec<L, T, Q> const& v, T const& epsilon);

	//! Check whether two vectors are orthonormal.
	/// @see gtx_vector_query extensions.
	template<length_t L, typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL bool areOrthonormal(vec<L, T, Q> const& v0, vec<L, T, Q> const& v1, T const& epsilon);

	/// @}
}// namespace aster_linear

#include "vector_query.inl"
