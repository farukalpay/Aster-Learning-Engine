/// @ref ext_vector_bool1
/// @file aster_linear/ext/vector_bool1.hpp
///
/// @defgroup ext_vector_bool1 ASTER_LINEAR_EXT_vector_bool1
/// @ingroup ext
///
/// Exposes bvec1 vector type.
///
/// Include <aster_linear/ext/vector_bool1.hpp> to use the features of this extension.
///
/// @see ext_vector_bool1_precision extension.

#pragma once

#include "../detail/type_vec1.hpp"

#if ASTER_LINEAR_MESSAGES == ASTER_LINEAR_ENABLE && !defined(ASTER_LINEAR_EXT_INCLUDED)
#	pragma message("ASTER_LINEAR: ASTER_LINEAR_EXT_vector_bool1 extension included")
#endif

namespace aster_linear
{
	/// @addtogroup ext_vector_bool1
	/// @{

	/// 1 components vector of boolean.
	typedef vec<1, bool, defaultp>		bvec1;

	/// @}
}//namespace aster_linear
