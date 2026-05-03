/// @ref ext_matrix_int3x2_sized
/// @file aster_linear/ext/matrix_int3x2_sized.hpp
///
/// @see core (dependence)
///
/// @defgroup ext_matrix_int3x2_sized ASTER_LINEAR_EXT_matrix_int3x2_sized
/// @ingroup ext
///
/// Include <aster_linear/ext/matrix_int3x2_sized.hpp> to use the features of this extension.
///
/// Defines a number of matrices with integer types.

#pragma once

// Dependency:
#include "../mat3x2.hpp"
#include "../ext/scalar_int_sized.hpp"

#if ASTER_LINEAR_MESSAGES == ASTER_LINEAR_ENABLE && !defined(ASTER_LINEAR_EXT_INCLUDED)
#	pragma message("ASTER_LINEAR: ASTER_LINEAR_EXT_matrix_int3x2_sized extension included")
#endif

namespace aster_linear
{
	/// @addtogroup ext_matrix_int3x2_sized
	/// @{

	/// 8 bit signed integer 3x2 matrix.
	///
	/// @see ext_matrix_int3x2_sized
	typedef mat<3, 2, int8, defaultp>				i8mat3x2;

	/// 16 bit signed integer 3x2 matrix.
	///
	/// @see ext_matrix_int3x2_sized
	typedef mat<3, 2, int16, defaultp>				i16mat3x2;

	/// 32 bit signed integer 3x2 matrix.
	///
	/// @see ext_matrix_int3x2_sized
	typedef mat<3, 2, int32, defaultp>				i32mat3x2;

	/// 64 bit signed integer 3x2 matrix.
	///
	/// @see ext_matrix_int3x2_sized
	typedef mat<3, 2, int64, defaultp>				i64mat3x2;

	/// @}
}//namespace aster_linear
