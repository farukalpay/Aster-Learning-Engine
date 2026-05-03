/// @ref ext_matrix_int3x2
/// @file aster_linear/ext/matrix_int3x2.hpp
///
/// @see core (dependence)
///
/// @defgroup ext_matrix_int3x2 ASTER_LINEAR_EXT_matrix_int3x2
/// @ingroup ext
///
/// Include <aster_linear/ext/matrix_int3x2.hpp> to use the features of this extension.
///
/// Defines a number of matrices with integer types.

#pragma once

// Dependency:
#include "../mat3x2.hpp"

#if ASTER_LINEAR_MESSAGES == ASTER_LINEAR_ENABLE && !defined(ASTER_LINEAR_EXT_INCLUDED)
#	pragma message("ASTER_LINEAR: ASTER_LINEAR_EXT_matrix_int3x2 extension included")
#endif

namespace aster_linear
{
	/// @addtogroup ext_matrix_int3x2
	/// @{

	/// Signed integer 3x2 matrix.
	///
	/// @see ext_matrix_int3x2
	typedef mat<3, 2, int, defaultp>	imat3x2;

	/// @}
}//namespace aster_linear
