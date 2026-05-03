/// @ref gtx_perpendicular
/// @file aster_linear/gtx/perpendicular.hpp
///
/// @see core (dependence)
/// @see gtx_projection (dependence)
///
/// @defgroup gtx_perpendicular ASTER_LINEAR_GTX_perpendicular
/// @ingroup gtx
///
/// Include <aster_linear/gtx/perpendicular.hpp> to use the features of this extension.
///
/// Perpendicular of a vector from other one

#pragma once

// Dependency:
#include "../aster_linear.hpp"
#include "../gtx/projection.hpp"

#if ASTER_LINEAR_MESSAGES == ASTER_LINEAR_ENABLE && !defined(ASTER_LINEAR_EXT_INCLUDED)
#	ifndef ASTER_LINEAR_ENABLE_EXPERIMENTAL
#		pragma message("ASTER_LINEAR: ASTER_LINEAR_GTX_perpendicular is an experimental extension and may change in the future. Use #define ASTER_LINEAR_ENABLE_EXPERIMENTAL before including it, if you really want to use it.")
#	else
#		pragma message("ASTER_LINEAR: ASTER_LINEAR_GTX_perpendicular extension included")
#	endif
#endif

namespace aster_linear
{
	/// @addtogroup gtx_perpendicular
	/// @{

	//! Projects x a perpendicular axis of Normal.
	//! From ASTER_LINEAR_GTX_perpendicular extension.
	template<typename genType>
	ASTER_LINEAR_FUNC_DECL genType perp(genType const& x, genType const& Normal);

	/// @}
}//namespace aster_linear

#include "perpendicular.inl"
