/// @ref gtx_extend
/// @file aster_linear/gtx/extend.hpp
///
/// @see core (dependence)
///
/// @defgroup gtx_extend ASTER_LINEAR_GTX_extend
/// @ingroup gtx
///
/// Include <aster_linear/gtx/extend.hpp> to use the features of this extension.
///
/// Extend a position from a source to a position at a defined length.

#pragma once

// Dependency:
#include "../aster_linear.hpp"

#if ASTER_LINEAR_MESSAGES == ASTER_LINEAR_ENABLE && !defined(ASTER_LINEAR_EXT_INCLUDED)
#	ifndef ASTER_LINEAR_ENABLE_EXPERIMENTAL
#		pragma message("ASTER_LINEAR: ASTER_LINEAR_GTX_extend is an experimental extension and may change in the future. Use #define ASTER_LINEAR_ENABLE_EXPERIMENTAL before including it, if you really want to use it.")
#	else
#		pragma message("ASTER_LINEAR: ASTER_LINEAR_GTX_extend extension included")
#	endif
#endif

namespace aster_linear
{
	/// @addtogroup gtx_extend
	/// @{

	/// Extends of Length the Origin position using the (Source - Origin) direction.
	/// @see gtx_extend
	template<typename genType>
	ASTER_LINEAR_FUNC_DECL genType extend(
		genType const& Origin,
		genType const& Source,
		typename genType::value_type const Length);

	/// @}
}//namespace aster_linear

#include "extend.inl"
