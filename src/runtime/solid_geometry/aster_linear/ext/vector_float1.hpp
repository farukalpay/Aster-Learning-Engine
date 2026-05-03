/// @ref ext_vector_float1
/// @file aster_linear/ext/vector_float1.hpp
///
/// @defgroup ext_vector_float1 ASTER_LINEAR_EXT_vector_float1
/// @ingroup ext
///
/// Exposes single-precision floating point vector type with one component.
///
/// Include <aster_linear/ext/vector_float1.hpp> to use the features of this extension.
///
/// @see ext_vector_float1_precision extension.
/// @see ext_vector_double1 extension.

#pragma once

#include "../detail/type_vec1.hpp"

#if ASTER_LINEAR_MESSAGES == ASTER_LINEAR_ENABLE && !defined(ASTER_LINEAR_EXT_INCLUDED)
#	pragma message("ASTER_LINEAR: ASTER_LINEAR_EXT_vector_float1 extension included")
#endif

namespace aster_linear
{
	/// @addtogroup ext_vector_float1
	/// @{

	/// 1 components vector of single-precision floating-point numbers.
	typedef vec<1, float, defaultp>		vec1;

	/// @}
}//namespace aster_linear
