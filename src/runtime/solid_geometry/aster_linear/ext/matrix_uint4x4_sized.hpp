/// @ref ext_matrix_uint4x4_sized
/// @file aster_linear/ext/matrix_uint4x4_sized.hpp
///
/// @see core (dependence)
///
/// @defgroup ext_matrix_uint4x4_sized ASTER_LINEAR_EXT_matrix_uint4x4_sized
/// @ingroup ext
///
/// Include <aster_linear/ext/matrix_uint4x4_sized.hpp> to use the features of this extension.
///
/// Defines a number of matrices with integer types.

#pragma once

// Dependency:
#include "../mat4x4.hpp"
#include "../ext/scalar_uint_sized.hpp"

#if ASTER_LINEAR_MESSAGES == ASTER_LINEAR_ENABLE && !defined(ASTER_LINEAR_EXT_INCLUDED)
#	pragma message("ASTER_LINEAR: ASTER_LINEAR_EXT_matrix_uint4x4_sized extension included")
#endif

namespace aster_linear
{
	/// @addtogroup ext_matrix_uint4x4_sized
	/// @{

	/// 8 bit unsigned integer 4x4 matrix.
	///
	/// @see ext_matrix_uint4x4_sized
	typedef mat<4, 4, uint8, defaultp>				u8mat4x4;

	/// 16 bit unsigned integer 4x4 matrix.
	///
	/// @see ext_matrix_uint4x4_sized
	typedef mat<4, 4, uint16, defaultp>				u16mat4x4;

	/// 32 bit unsigned integer 4x4 matrix.
	///
	/// @see ext_matrix_uint4x4_sized
	typedef mat<4, 4, uint32, defaultp>				u32mat4x4;

	/// 64 bit unsigned integer 4x4 matrix.
	///
	/// @see ext_matrix_uint4x4_sized
	typedef mat<4, 4, uint64, defaultp>				u64mat4x4;


	/// 8 bit unsigned integer 4x4 matrix.
	///
	/// @see ext_matrix_uint4x4_sized
	typedef mat<4, 4, uint8, defaultp>				u8mat4;

	/// 16 bit unsigned integer 4x4 matrix.
	///
	/// @see ext_matrix_uint4x4_sized
	typedef mat<4, 4, uint16, defaultp>				u16mat4;

	/// 32 bit unsigned integer 4x4 matrix.
	///
	/// @see ext_matrix_uint4x4_sized
	typedef mat<4, 4, uint32, defaultp>				u32mat4;

	/// 64 bit unsigned integer 4x4 matrix.
	///
	/// @see ext_matrix_uint4x4_sized
	typedef mat<4, 4, uint64, defaultp>				u64mat4;

	/// @}
}//namespace aster_linear
