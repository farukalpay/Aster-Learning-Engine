/// @ref ext_matrix_uint4x4
/// @file aster_linear/ext/matrix_uint4x4.hpp
///
/// @see core (dependence)
///
/// @defgroup ext_matrix_uint4x4 ASTER_LINEAR_EXT_matrix_uint4x4
/// @ingroup ext
///
/// Include <aster_linear/ext/matrix_uint4x4.hpp> to use the features of this extension.
///
/// Defines a number of matrices with integer types.

#pragma once

// Dependency:
#include "../mat4x4.hpp"

#if ASTER_LINEAR_MESSAGES == ASTER_LINEAR_ENABLE && !defined(ASTER_LINEAR_EXT_INCLUDED)
#	pragma message("ASTER_LINEAR: ASTER_LINEAR_EXT_matrix_uint4x4 extension included")
#endif

namespace aster_linear
{
	/// @addtogroup ext_matrix_uint4x4
	/// @{

	/// Unsigned integer 4x4 matrix.
	///
	/// @see ext_matrix_uint4x4
	typedef mat<4, 4, uint, defaultp>	umat4x4;

	/// Unsigned integer 4x4 matrix.
	///
	/// @see ext_matrix_uint4x4
	typedef mat<4, 4, uint, defaultp>	umat4;

	/// @}
}//namespace aster_linear
