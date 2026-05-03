/// @ref gtx_scalar_relational
/// @file aster_linear/gtx/scalar_relational.hpp
///
/// @see core (dependence)
///
/// @defgroup gtx_scalar_relational ASTER_LINEAR_GTX_scalar_relational
/// @ingroup gtx
///
/// Include <aster_linear/gtx/scalar_relational.hpp> to use the features of this extension.
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
	/// @addtogroup gtx_scalar_relational
	/// @{



	/// @}
}//namespace aster_linear

#include "scalar_relational.inl"
