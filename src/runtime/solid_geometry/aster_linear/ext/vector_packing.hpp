/// @ref ext_vector_packing
/// @file aster_linear/ext/vector_packing.hpp
///
/// @see core (dependence)
///
/// @defgroup ext_vector_packing ASTER_LINEAR_EXT_vector_packing
/// @ingroup ext
///
/// Include <aster_linear/ext/vector_packing.hpp> to use the features of this extension.
///
/// This extension provides a set of function to convert vectors to packed
/// formats.

#pragma once

// Dependency:
#include "../detail/qualifier.hpp"

#if ASTER_LINEAR_MESSAGES == ASTER_LINEAR_ENABLE && !defined(ASTER_LINEAR_EXT_INCLUDED)
#	pragma message("ASTER_LINEAR: ASTER_LINEAR_EXT_vector_packing extension included")
#endif

namespace aster_linear
{
	/// @addtogroup ext_vector_packing
	/// @{


	/// @}
}// namespace aster_linear

#include "vector_packing.inl"
