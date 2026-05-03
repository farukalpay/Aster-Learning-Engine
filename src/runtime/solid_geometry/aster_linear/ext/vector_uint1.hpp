/// @ref ext_vector_uint1
/// @file aster_linear/ext/vector_uint1.hpp
///
/// @defgroup ext_vector_uint1 ASTER_LINEAR_EXT_vector_uint1
/// @ingroup ext
///
/// Exposes uvec1 vector type.
///
/// Include <aster_linear/ext/vector_uvec1.hpp> to use the features of this extension.
///
/// @see ext_vector_int1 extension.
/// @see ext_vector_uint1_precision extension.

#pragma once

#include "../detail/type_vec1.hpp"

#if ASTER_LINEAR_MESSAGES == ASTER_LINEAR_ENABLE && !defined(ASTER_LINEAR_EXT_INCLUDED)
#	pragma message("ASTER_LINEAR: ASTER_LINEAR_EXT_vector_uint1 extension included")
#endif

namespace aster_linear
{
	/// @addtogroup ext_vector_uint1
	/// @{

	/// 1 component vector of unsigned integer numbers.
	typedef vec<1, unsigned int, defaultp>			uvec1;

	/// @}
}//namespace aster_linear

