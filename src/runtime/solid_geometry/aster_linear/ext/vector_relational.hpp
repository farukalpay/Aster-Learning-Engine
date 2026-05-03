/// @ref ext_vector_relational
/// @file aster_linear/ext/vector_relational.hpp
///
/// @see core (dependence)
/// @see ext_scalar_integer (dependence)
///
/// @defgroup ext_vector_relational ASTER_LINEAR_EXT_vector_relational
/// @ingroup ext
///
/// Exposes comparison functions for vector types that take a user defined epsilon values.
///
/// Include <aster_linear/ext/vector_relational.hpp> to use the features of this extension.
///
/// @see core_vector_relational
/// @see ext_scalar_relational
/// @see ext_matrix_relational

#pragma once

// Dependencies
#include "../detail/qualifier.hpp"

#if ASTER_LINEAR_MESSAGES == ASTER_LINEAR_ENABLE && !defined(ASTER_LINEAR_EXT_INCLUDED)
#	pragma message("ASTER_LINEAR: ASTER_LINEAR_EXT_vector_relational extension included")
#endif

namespace aster_linear
{
	/// @addtogroup ext_vector_relational
	/// @{

	/// Returns the component-wise comparison of |x - y| < epsilon.
	/// True if this expression is satisfied.
	///
	/// @tparam L Integer between 1 and 4 included that qualify the dimension of the vector
	/// @tparam T Floating-point or integer scalar types
	/// @tparam Q Value from qualifier enum
	template<length_t L, typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<L, bool, Q> equal(vec<L, T, Q> const& x, vec<L, T, Q> const& y, T epsilon);

	/// Returns the component-wise comparison of |x - y| < epsilon.
	/// True if this expression is satisfied.
	///
	/// @tparam L Integer between 1 and 4 included that qualify the dimension of the vector
	/// @tparam T Floating-point or integer scalar types
	/// @tparam Q Value from qualifier enum
	template<length_t L, typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<L, bool, Q> equal(vec<L, T, Q> const& x, vec<L, T, Q> const& y, vec<L, T, Q> const& epsilon);

	/// Returns the component-wise comparison of |x - y| >= epsilon.
	/// True if this expression is not satisfied.
	///
	/// @tparam L Integer between 1 and 4 included that qualify the dimension of the vector
	/// @tparam T Floating-point or integer scalar types
	/// @tparam Q Value from qualifier enum
	template<length_t L, typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<L, bool, Q> notEqual(vec<L, T, Q> const& x, vec<L, T, Q> const& y, T epsilon);

	/// Returns the component-wise comparison of |x - y| >= epsilon.
	/// True if this expression is not satisfied.
	///
	/// @tparam L Integer between 1 and 4 included that qualify the dimension of the vector
	/// @tparam T Floating-point or integer scalar types
	/// @tparam Q Value from qualifier enum
	template<length_t L, typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<L, bool, Q> notEqual(vec<L, T, Q> const& x, vec<L, T, Q> const& y, vec<L, T, Q> const& epsilon);

	/// Returns the component-wise comparison between two vectors in term of ULPs.
	/// True if this expression is satisfied.
	///
	/// @tparam L Integer between 1 and 4 included that qualify the dimension of the vector
	/// @tparam T Floating-point
	/// @tparam Q Value from qualifier enum
	template<length_t L, typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<L, bool, Q> equal(vec<L, T, Q> const& x, vec<L, T, Q> const& y, int ULPs);

	/// Returns the component-wise comparison between two vectors in term of ULPs.
	/// True if this expression is satisfied.
	///
	/// @tparam L Integer between 1 and 4 included that qualify the dimension of the vector
	/// @tparam T Floating-point
	/// @tparam Q Value from qualifier enum
	template<length_t L, typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<L, bool, Q> equal(vec<L, T, Q> const& x, vec<L, T, Q> const& y, vec<L, int, Q> const& ULPs);

	/// Returns the component-wise comparison between two vectors in term of ULPs.
	/// True if this expression is not satisfied.
	///
	/// @tparam L Integer between 1 and 4 included that qualify the dimension of the vector
	/// @tparam T Floating-point
	/// @tparam Q Value from qualifier enum
	template<length_t L, typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<L, bool, Q> notEqual(vec<L, T, Q> const& x, vec<L, T, Q> const& y, int ULPs);

	/// Returns the component-wise comparison between two vectors in term of ULPs.
	/// True if this expression is not satisfied.
	///
	/// @tparam L Integer between 1 and 4 included that qualify the dimension of the vector
	/// @tparam T Floating-point
	/// @tparam Q Value from qualifier enum
	template<length_t L, typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<L, bool, Q> notEqual(vec<L, T, Q> const& x, vec<L, T, Q> const& y, vec<L, int, Q> const& ULPs);

	/// @}
}//namespace aster_linear

#include "vector_relational.inl"
