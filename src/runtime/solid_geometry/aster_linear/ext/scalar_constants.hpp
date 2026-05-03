/// @ref ext_scalar_constants
/// @file aster_linear/ext/scalar_constants.hpp
///
/// @defgroup ext_scalar_constants ASTER_LINEAR_EXT_scalar_constants
/// @ingroup ext
///
/// Provides a list of constants and precomputed useful values.
///
/// Include <aster_linear/ext/scalar_constants.hpp> to use the features of this extension.

#pragma once

// Dependencies
#include "../detail/setup.hpp"

#if ASTER_LINEAR_MESSAGES == ASTER_LINEAR_ENABLE && !defined(ASTER_LINEAR_EXT_INCLUDED)
#	pragma message("ASTER_LINEAR: ASTER_LINEAR_EXT_scalar_constants extension included")
#endif

namespace aster_linear
{
	/// @addtogroup ext_scalar_constants
	/// @{

	/// Return the epsilon constant for floating point types.
	template<typename genType>
	ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR genType epsilon();

	/// Return the pi constant for floating point types.
	template<typename genType>
	ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR genType pi();

	/// Return the value of cos(1 / 2) for floating point types.
	template<typename genType>
	ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR genType cos_one_over_two();

	/// @}
} //namespace aster_linear

#include "scalar_constants.inl"
