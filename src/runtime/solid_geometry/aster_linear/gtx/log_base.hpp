/// @ref gtx_log_base
/// @file aster_linear/gtx/log_base.hpp
///
/// @see core (dependence)
///
/// @defgroup gtx_log_base ASTER_LINEAR_GTX_log_base
/// @ingroup gtx
///
/// Include <aster_linear/gtx/log_base.hpp> to use the features of this extension.
///
/// Logarithm for any base. base can be a vector or a scalar.

#pragma once

// Dependency:
#include "../aster_linear.hpp"

#if ASTER_LINEAR_MESSAGES == ASTER_LINEAR_ENABLE && !defined(ASTER_LINEAR_EXT_INCLUDED)
#	ifndef ASTER_LINEAR_ENABLE_EXPERIMENTAL
#		pragma message("ASTER_LINEAR: ASTER_LINEAR_GTX_log_base is an experimental extension and may change in the future. Use #define ASTER_LINEAR_ENABLE_EXPERIMENTAL before including it, if you really want to use it.")
#	else
#		pragma message("ASTER_LINEAR: ASTER_LINEAR_GTX_log_base extension included")
#	endif
#endif

namespace aster_linear
{
	/// @addtogroup gtx_log_base
	/// @{

	/// Logarithm for any base.
	/// From ASTER_LINEAR_GTX_log_base.
	template<typename genType>
	ASTER_LINEAR_FUNC_DECL genType log(
		genType const& x,
		genType const& base);

	/// Logarithm for any base.
	/// From ASTER_LINEAR_GTX_log_base.
	template<length_t L, typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL vec<L, T, Q> sign(
		vec<L, T, Q> const& x,
		vec<L, T, Q> const& base);

	/// @}
}//namespace aster_linear

#include "log_base.inl"
