/// @ref ext_matrix_uint3x4
/// @file aster_linear/ext/matrix_uint3x4.hpp
///
/// @see core (dependence)
///
/// @defgroup ext_matrix_uint3x4 ASTER_LINEAR_EXT_matrix_uint3x4
/// @ingroup ext
///
/// Include <aster_linear/ext/matrix_uint3x4.hpp> to use the features of this extension.
///
/// Defines a number of matrices with integer types.

#pragma once

// Dependency:
#include "../mat3x4.hpp"

#if ASTER_LINEAR_MESSAGES == ASTER_LINEAR_ENABLE && !defined(ASTER_LINEAR_EXT_INCLUDED)
#	pragma message("ASTER_LINEAR: ASTER_LINEAR_EXT_matrix_uint3x4 extension included")
#endif

namespace aster_linear
{
	/// @addtogroup ext_matrix_uint3x4
	/// @{

	/// Signed integer 3x4 matrix.
	///
	/// @see ext_matrix_uint3x4
	typedef mat<3, 4, uint, defaultp>	umat3x4;

	/// @}
}//namespace aster_linear
