/// @ref ext_vector_uint2_sized
/// @file aster_linear/ext/vector_uint2_sized.hpp
///
/// @defgroup ext_vector_uint2_sized ASTER_LINEAR_EXT_vector_uint2_sized
/// @ingroup ext
///
/// Exposes sized unsigned integer vector of 2 components type.
///
/// Include <aster_linear/ext/vector_uint2_sized.hpp> to use the features of this extension.
///
/// @see ext_scalar_uint_sized
/// @see ext_vector_int2_sized

#pragma once

#include "../ext/vector_uint2.hpp"
#include "../ext/scalar_uint_sized.hpp"

#if ASTER_LINEAR_MESSAGES == ASTER_LINEAR_ENABLE && !defined(ASTER_LINEAR_EXT_INCLUDED)
#	pragma message("ASTER_LINEAR: ASTER_LINEAR_EXT_vector_uint2_sized extension included")
#endif

namespace aster_linear
{
	/// @addtogroup ext_vector_uint2_sized
	/// @{

	/// 8 bit unsigned integer vector of 2 components type.
	///
	/// @see ext_vector_uint2_sized
	typedef vec<2, uint8, defaultp>		u8vec2;

	/// 16 bit unsigned integer vector of 2 components type.
	///
	/// @see ext_vector_uint2_sized
	typedef vec<2, uint16, defaultp>	u16vec2;

	/// 32 bit unsigned integer vector of 2 components type.
	///
	/// @see ext_vector_uint2_sized
	typedef vec<2, uint32, defaultp>	u32vec2;

	/// 64 bit unsigned integer vector of 2 components type.
	///
	/// @see ext_vector_uint2_sized
	typedef vec<2, uint64, defaultp>	u64vec2;

	/// @}
}//namespace aster_linear
