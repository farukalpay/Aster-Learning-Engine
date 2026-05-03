/// @ref gtc_vec1
/// @file aster_linear/gtc/vec1.hpp
///
/// @see core (dependence)
///
/// @defgroup gtc_vec1 ASTER_LINEAR_GTC_vec1
/// @ingroup gtc
///
/// Include <aster_linear/gtc/vec1.hpp> to use the features of this extension.
///
/// Add vec1, ivec1, uvec1 and bvec1 types.

#pragma once

// Dependency:
#include "../ext/vector_bool1.hpp"
#include "../ext/vector_bool1_precision.hpp"
#include "../ext/vector_float1.hpp"
#include "../ext/vector_float1_precision.hpp"
#include "../ext/vector_double1.hpp"
#include "../ext/vector_double1_precision.hpp"
#include "../ext/vector_int1.hpp"
#include "../ext/vector_int1_sized.hpp"
#include "../ext/vector_uint1.hpp"
#include "../ext/vector_uint1_sized.hpp"

#if ASTER_LINEAR_MESSAGES == ASTER_LINEAR_ENABLE && !defined(ASTER_LINEAR_EXT_INCLUDED)
#	pragma message("ASTER_LINEAR: ASTER_LINEAR_GTC_vec1 extension included")
#endif

