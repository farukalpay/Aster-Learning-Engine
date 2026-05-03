/// @ref ext_quaternion_exponential
/// @file aster_linear/ext/quaternion_exponential.hpp
///
/// @defgroup ext_quaternion_exponential ASTER_LINEAR_EXT_quaternion_exponential
/// @ingroup ext
///
/// Provides exponential functions for quaternion types
///
/// Include <aster_linear/ext/quaternion_exponential.hpp> to use the features of this extension.
///
/// @see core_exponential
/// @see ext_quaternion_float
/// @see ext_quaternion_double

#pragma once

// Dependency:
#include "../common.hpp"
#include "../trigonometric.hpp"
#include "../geometric.hpp"
#include "../ext/scalar_constants.hpp"

#if ASTER_LINEAR_MESSAGES == ASTER_LINEAR_ENABLE && !defined(ASTER_LINEAR_EXT_INCLUDED)
#	pragma message("ASTER_LINEAR: ASTER_LINEAR_EXT_quaternion_exponential extension included")
#endif

namespace aster_linear
{
	/// @addtogroup ext_quaternion_transform
	/// @{

	/// Returns a exponential of a quaternion.
	///
	/// @tparam T A floating-point scalar type
	/// @tparam Q A value from qualifier enum
	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL qua<T, Q> exp(qua<T, Q> const& q);

	/// Returns a logarithm of a quaternion
	///
	/// @tparam T A floating-point scalar type
	/// @tparam Q A value from qualifier enum
	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL qua<T, Q> log(qua<T, Q> const& q);

	/// Returns a quaternion raised to a power.
	///
	/// @tparam T A floating-point scalar type
	/// @tparam Q A value from qualifier enum
	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL qua<T, Q> pow(qua<T, Q> const& q, T y);

	/// Returns the square root of a quaternion
	///
	/// @tparam T A floating-point scalar type
	/// @tparam Q A value from qualifier enum
	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL qua<T, Q> sqrt(qua<T, Q> const& q);

	/// @}
} //namespace aster_linear

#include "quaternion_exponential.inl"
