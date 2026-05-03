/// @ref gtx_wrap
/// @file aster_linear/gtx/wrap.hpp
///
/// @see core (dependence)
///
/// @defgroup gtx_wrap ASTER_LINEAR_GTX_wrap
/// @ingroup gtx
///
/// Include <aster_linear/gtx/wrap.hpp> to use the features of this extension.
///
/// Wrapping mode of texture coordinates.

#pragma once

// Dependency:
#include "../aster_linear.hpp"
#include "../ext/scalar_common.hpp"
#include "../ext/vector_common.hpp"
#include "../gtc/vec1.hpp"

#if ASTER_LINEAR_MESSAGES == ASTER_LINEAR_ENABLE && !defined(ASTER_LINEAR_EXT_INCLUDED)
#	ifndef ASTER_LINEAR_ENABLE_EXPERIMENTAL
#		pragma message("ASTER_LINEAR: ASTER_LINEAR_GTX_wrap is an experimental extension and may change in the future. Use #define ASTER_LINEAR_ENABLE_EXPERIMENTAL before including it, if you really want to use it.")
#	else
#		pragma message("ASTER_LINEAR: ASTER_LINEAR_GTX_wrap extension included")
#	endif
#endif

namespace aster_linear
{
	/// @addtogroup gtx_wrap
	/// @{

	/// @}
}// namespace aster_linear

#include "wrap.inl"
