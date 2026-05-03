/// @ref ext_vector_int4_sized
/// @file aster_linear/ext/vector_int4_sized.hpp
///
/// @defgroup ext_vector_int4_sized ASTER_LINEAR_EXT_vector_int4_sized
/// @ingroup ext
///
/// Exposes sized signed integer vector of 4 components type.
///
/// Include <aster_linear/ext/vector_int4_sized.hpp> to use the features of this extension.
///
/// @see ext_scalar_int_sized
/// @see ext_vector_uint4_sized

#pragma once

#include "../ext/vector_int4.hpp"
#include "../ext/scalar_int_sized.hpp"

#if ASTER_LINEAR_MESSAGES == ASTER_LINEAR_ENABLE && !defined(ASTER_LINEAR_EXT_INCLUDED)
#	pragma message("ASTER_LINEAR: ASTER_LINEAR_EXT_vector_int4_sized extension included")
#endif

namespace aster_linear
{
	/// @addtogroup ext_vector_int4_sized
	/// @{

	/// 8 bit signed integer vector of 4 components type.
	///
	/// @see ext_vector_int4_sized
	typedef vec<4, int8, defaultp>		i8vec4;

	/// 16 bit signed integer vector of 4 components type.
	///
	/// @see ext_vector_int4_sized
	typedef vec<4, int16, defaultp>		i16vec4;

	/// 32 bit signed integer vector of 4 components type.
	///
	/// @see ext_vector_int4_sized
	typedef vec<4, int32, defaultp>		i32vec4;

	/// 64 bit signed integer vector of 4 components type.
	///
	/// @see ext_vector_int4_sized
	typedef vec<4, int64, defaultp>		i64vec4;

	/// @}
}//namespace aster_linear
