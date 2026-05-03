/// @ref ext_matrix_uint2x2
/// @file aster_linear/ext/matrix_uint2x2.hpp
///
/// @see core (dependence)
///
/// @defgroup ext_matrix_uint2x2 ASTER_LINEAR_EXT_matrix_uint2x2
/// @ingroup ext
///
/// Include <aster_linear/ext/matrix_uint2x2.hpp> to use the features of this extension.
///
/// Defines a number of matrices with integer types.

#pragma once

// Dependency:
#include "../mat2x2.hpp"

#if ASTER_LINEAR_MESSAGES == ASTER_LINEAR_ENABLE && !defined(ASTER_LINEAR_EXT_INCLUDED)
#	pragma message("ASTER_LINEAR: ASTER_LINEAR_EXT_matrix_uint2x2 extension included")
#endif

namespace aster_linear
{
	/// @addtogroup ext_matrix_uint2x2
	/// @{

	/// Unsigned integer 2x2 matrix.
	///
	/// @see ext_matrix_uint2x2
	typedef mat<2, 2, uint, defaultp>	umat2x2;

	/// Unsigned integer 2x2 matrix.
	///
	/// @see ext_matrix_uint2x2
	typedef mat<2, 2, uint, defaultp>	umat2;

	/// @}
}//namespace aster_linear
