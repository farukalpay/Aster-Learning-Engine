/// @ref ext_matrix_int4x3
/// @file aster_linear/ext/matrix_int4x3.hpp
///
/// @see core (dependence)
///
/// @defgroup ext_matrix_int4x3 ASTER_LINEAR_EXT_matrix_int4x3
/// @ingroup ext
///
/// Include <aster_linear/ext/matrix_int4x3.hpp> to use the features of this extension.
///
/// Defines a number of matrices with integer types.

#pragma once

// Dependency:
#include "../mat4x3.hpp"

#if ASTER_LINEAR_MESSAGES == ASTER_LINEAR_ENABLE && !defined(ASTER_LINEAR_EXT_INCLUDED)
#	pragma message("ASTER_LINEAR: ASTER_LINEAR_EXT_matrix_int4x3 extension included")
#endif

namespace aster_linear
{
	/// @addtogroup ext_matrix_int4x3
	/// @{

	/// Signed integer 4x3 matrix.
	///
	/// @see ext_matrix_int4x3
	typedef mat<4, 3, int, defaultp>	imat4x3;

	/// @}
}//namespace aster_linear
