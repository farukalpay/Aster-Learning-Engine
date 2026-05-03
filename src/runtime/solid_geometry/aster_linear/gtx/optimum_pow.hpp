/// @ref gtx_optimum_pow
/// @file aster_linear/gtx/optimum_pow.hpp
///
/// @see core (dependence)
///
/// @defgroup gtx_optimum_pow ASTER_LINEAR_GTX_optimum_pow
/// @ingroup gtx
///
/// Include <aster_linear/gtx/optimum_pow.hpp> to use the features of this extension.
///
/// Integer exponentiation of power functions.

#pragma once

// Dependency:
#include "../aster_linear.hpp"

#if ASTER_LINEAR_MESSAGES == ASTER_LINEAR_ENABLE && !defined(ASTER_LINEAR_EXT_INCLUDED)
#	ifndef ASTER_LINEAR_ENABLE_EXPERIMENTAL
#		pragma message("ASTER_LINEAR: ASTER_LINEAR_GTX_optimum_pow is an experimental extension and may change in the future. Use #define ASTER_LINEAR_ENABLE_EXPERIMENTAL before including it, if you really want to use it.")
#	else
#		pragma message("ASTER_LINEAR: ASTER_LINEAR_GTX_optimum_pow extension included")
#	endif
#endif

namespace aster_linear{
namespace gtx
{
	/// @addtogroup gtx_optimum_pow
	/// @{

	/// Returns x raised to the power of 2.
	///
	/// @see gtx_optimum_pow
	template<typename genType>
	ASTER_LINEAR_FUNC_DECL genType pow2(genType const& x);

	/// Returns x raised to the power of 3.
	///
	/// @see gtx_optimum_pow
	template<typename genType>
	ASTER_LINEAR_FUNC_DECL genType pow3(genType const& x);

	/// Returns x raised to the power of 4.
	///
	/// @see gtx_optimum_pow
	template<typename genType>
	ASTER_LINEAR_FUNC_DECL genType pow4(genType const& x);

	/// @}
}//namespace gtx
}//namespace aster_linear

#include "optimum_pow.inl"
