/// @ref ext_vector_bool1_precision
/// @file aster_linear/ext/vector_bool1_precision.hpp
///
/// @defgroup ext_vector_bool1_precision ASTER_LINEAR_EXT_vector_bool1_precision
/// @ingroup ext
///
/// Exposes highp_bvec1, mediump_bvec1 and lowp_bvec1 types.
///
/// Include <aster_linear/ext/vector_bool1_precision.hpp> to use the features of this extension.

#pragma once

#include "../detail/type_vec1.hpp"

#if ASTER_LINEAR_MESSAGES == ASTER_LINEAR_ENABLE && !defined(ASTER_LINEAR_EXT_INCLUDED)
#	pragma message("ASTER_LINEAR: ASTER_LINEAR_EXT_vector_bool1_precision extension included")
#endif

namespace aster_linear
{
	/// @addtogroup ext_vector_bool1_precision
	/// @{

	/// 1 component vector of bool values.
	typedef vec<1, bool, highp>			highp_bvec1;

	/// 1 component vector of bool values.
	typedef vec<1, bool, mediump>		mediump_bvec1;

	/// 1 component vector of bool values.
	typedef vec<1, bool, lowp>			lowp_bvec1;

	/// @}
}//namespace aster_linear
