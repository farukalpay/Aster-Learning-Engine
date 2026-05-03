/// @ref gtc_reciprocal
/// @file aster_linear/gtc/reciprocal.hpp
///
/// @see core (dependence)
///
/// @defgroup gtc_reciprocal ASTER_LINEAR_GTC_reciprocal
/// @ingroup gtc
///
/// Include <aster_linear/gtc/reciprocal.hpp> to use the features of this extension.
///
/// Define secant, cosecant and cotangent functions.

#pragma once

// Dependencies
#include "../detail/setup.hpp"

#if ASTER_LINEAR_MESSAGES == ASTER_LINEAR_ENABLE && !defined(ASTER_LINEAR_EXT_INCLUDED)
#	pragma message("ASTER_LINEAR: ASTER_LINEAR_GTC_reciprocal extension included")
#endif

#include "../ext/scalar_reciprocal.hpp"
#include "../ext/vector_reciprocal.hpp"

