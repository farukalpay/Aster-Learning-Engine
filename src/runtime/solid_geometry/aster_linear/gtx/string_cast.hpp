/// @ref gtx_string_cast
/// @file aster_linear/gtx/string_cast.hpp
///
/// @see core (dependence)
/// @see gtx_integer (dependence)
/// @see gtx_quaternion (dependence)
///
/// @defgroup gtx_string_cast ASTER_LINEAR_GTX_string_cast
/// @ingroup gtx
///
/// Include <aster_linear/gtx/string_cast.hpp> to use the features of this extension.
///
/// Setup strings for ASTER_LINEAR type values

#pragma once

// Dependency:
#include "../aster_linear.hpp"
#include "../gtc/type_precision.hpp"
#include "../gtc/quaternion.hpp"
#include "../gtx/dual_quaternion.hpp"
#include <string>
#include <cmath>

#if ASTER_LINEAR_MESSAGES == ASTER_LINEAR_ENABLE && !defined(ASTER_LINEAR_EXT_INCLUDED)
#	ifndef ASTER_LINEAR_ENABLE_EXPERIMENTAL
#		pragma message("ASTER_LINEAR: ASTER_LINEAR_GTX_string_cast is an experimental extension and may change in the future. Use #define ASTER_LINEAR_ENABLE_EXPERIMENTAL before including it, if you really want to use it.")
#	else
#		pragma message("ASTER_LINEAR: ASTER_LINEAR_GTX_string_cast extension included")
#	endif
#endif

namespace aster_linear
{
	/// @addtogroup gtx_string_cast
	/// @{

	/// Create a string from a ASTER_LINEAR vector or matrix typed variable.
	/// @see gtx_string_cast extension.
	template<typename genType>
	ASTER_LINEAR_FUNC_DECL std::string to_string(genType const& x);

	/// @}
}//namespace aster_linear

#include "string_cast.inl"
