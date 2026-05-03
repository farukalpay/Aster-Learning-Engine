/// @ref ext_vector_int1
/// @file aster_linear/ext/vector_int1.hpp
///
/// @defgroup ext_vector_int1 ASTER_LINEAR_EXT_vector_int1
/// @ingroup ext
///
/// Exposes ivec1 vector type.
///
/// Include <aster_linear/ext/vector_int1.hpp> to use the features of this extension.
///
/// @see ext_vector_uint1 extension.
/// @see ext_vector_int1_precision extension.

#pragma once

#include "../detail/type_vec1.hpp"

#if ASTER_LINEAR_MESSAGES == ASTER_LINEAR_ENABLE && !defined(ASTER_LINEAR_EXT_INCLUDED)
#	pragma message("ASTER_LINEAR: ASTER_LINEAR_EXT_vector_int1 extension included")
#endif

namespace aster_linear
{
	/// @addtogroup ext_vector_int1
	/// @{

	/// 1 component vector of signed integer numbers.
	typedef vec<1, int, defaultp>			ivec1;

	/// @}
}//namespace aster_linear

