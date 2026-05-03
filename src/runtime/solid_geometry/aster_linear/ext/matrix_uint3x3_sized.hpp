/// @ref ext_matrix_uint3x3_sized
/// @file aster_linear/ext/matrix_uint3x3_sized.hpp
///
/// @see core (dependence)
///
/// @defgroup ext_matrix_uint3x3_sized ASTER_LINEAR_EXT_matrix_uint3x3_sized
/// @ingroup ext
///
/// Include <aster_linear/ext/matrix_uint3x3_sized.hpp> to use the features of this extension.
///
/// Defines a number of matrices with integer types.

#pragma once

// Dependency:
#include "../mat3x3.hpp"
#include "../ext/scalar_uint_sized.hpp"

#if ASTER_LINEAR_MESSAGES == ASTER_LINEAR_ENABLE && !defined(ASTER_LINEAR_EXT_INCLUDED)
#	pragma message("ASTER_LINEAR: ASTER_LINEAR_EXT_matrix_uint3x3_sized extension included")
#endif

namespace aster_linear
{
	/// @addtogroup ext_matrix_uint3x3_sized
	/// @{

	/// 8 bit unsigned integer 3x3 matrix.
	///
	/// @see ext_matrix_uint3x3_sized
	typedef mat<3, 3, uint8, defaultp>				u8mat3x3;

	/// 16 bit unsigned integer 3x3 matrix.
	///
	/// @see ext_matrix_uint3x3_sized
	typedef mat<3, 3, uint16, defaultp>				u16mat3x3;

	/// 32 bit unsigned integer 3x3 matrix.
	///
	/// @see ext_matrix_uint3x3_sized
	typedef mat<3, 3, uint32, defaultp>				u32mat3x3;

	/// 64 bit unsigned integer 3x3 matrix.
	///
	/// @see ext_matrix_uint3x3_sized
	typedef mat<3, 3, uint64, defaultp>				u64mat3x3;


	/// 8 bit unsigned integer 3x3 matrix.
	///
	/// @see ext_matrix_uint3x3_sized
	typedef mat<3, 3, uint8, defaultp>				u8mat3;

	/// 16 bit unsigned integer 3x3 matrix.
	///
	/// @see ext_matrix_uint3x3_sized
	typedef mat<3, 3, uint16, defaultp>				u16mat3;

	/// 32 bit unsigned integer 3x3 matrix.
	///
	/// @see ext_matrix_uint3x3_sized
	typedef mat<3, 3, uint32, defaultp>				u32mat3;

	/// 64 bit unsigned integer 3x3 matrix.
	///
	/// @see ext_matrix_uint3x3_sized
	typedef mat<3, 3, uint64, defaultp>				u64mat3;

	/// @}
}//namespace aster_linear
