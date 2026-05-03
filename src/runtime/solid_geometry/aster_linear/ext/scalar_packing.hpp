/// @ref ext_scalar_packing
/// @file aster_linear/ext/scalar_packing.hpp
///
/// @see core (dependence)
///
/// @defgroup ext_scalar_packing ASTER_LINEAR_EXT_scalar_packing
/// @ingroup ext
///
/// Include <aster_linear/ext/scalar_packing.hpp> to use the features of this extension.
///
/// This extension provides a set of function to convert scalar values to packed
/// formats.

#pragma once

// Dependency:
#include "../detail/qualifier.hpp"

#if ASTER_LINEAR_MESSAGES == ASTER_LINEAR_ENABLE && !defined(ASTER_LINEAR_EXT_INCLUDED)
#	pragma message("ASTER_LINEAR: ASTER_LINEAR_EXT_scalar_packing extension included")
#endif

namespace aster_linear
{
	/// @addtogroup ext_scalar_packing
	/// @{


	/// @}
}// namespace aster_linear

#include "scalar_packing.inl"
