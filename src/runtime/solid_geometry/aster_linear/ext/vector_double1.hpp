/// @ref ext_vector_double1
/// @file aster_linear/ext/vector_double1.hpp
///
/// @defgroup ext_vector_double1 ASTER_LINEAR_EXT_vector_double1
/// @ingroup ext
///
/// Exposes double-precision floating point vector type with one component.
///
/// Include <aster_linear/ext/vector_double1.hpp> to use the features of this extension.
///
/// @see ext_vector_double1_precision extension.
/// @see ext_vector_float1 extension.

#pragma once

#include "../detail/type_vec1.hpp"

#if ASTER_LINEAR_MESSAGES == ASTER_LINEAR_ENABLE && !defined(ASTER_LINEAR_EXT_INCLUDED)
#	pragma message("ASTER_LINEAR: ASTER_LINEAR_EXT_vector_double1 extension included")
#endif

namespace aster_linear
{
	/// @addtogroup ext_vector_double1
	/// @{

	/// 1 components vector of double-precision floating-point numbers.
	typedef vec<1, double, defaultp>		dvec1;

	/// @}
}//namespace aster_linear
