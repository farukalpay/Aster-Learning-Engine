/// @ref gtc_integer
/// @file aster_linear/gtc/integer.hpp
///
/// @see core (dependence)
/// @see gtc_integer (dependence)
///
/// @defgroup gtc_integer ASTER_LINEAR_GTC_integer
/// @ingroup gtc
///
/// Include <aster_linear/gtc/integer.hpp> to use the features of this extension.
///
/// @brief Allow to perform bit operations on integer values

#pragma once

// Dependencies
#include "../detail/setup.hpp"
#include "../detail/qualifier.hpp"
#include "../common.hpp"
#include "../integer.hpp"
#include "../exponential.hpp"
#include "../ext/scalar_common.hpp"
#include "../ext/vector_common.hpp"
#include <limits>

#if ASTER_LINEAR_MESSAGES == ASTER_LINEAR_ENABLE && !defined(ASTER_LINEAR_EXT_INCLUDED)
#	pragma message("ASTER_LINEAR: ASTER_LINEAR_GTC_integer extension included")
#endif

namespace aster_linear
{
	/// @addtogroup gtc_integer
	/// @{

	/// Returns the log2 of x for integer values. Usefull to compute mipmap count from the texture size.
	/// @see gtc_integer
	template<typename genIUType>
	ASTER_LINEAR_FUNC_DECL genIUType log2(genIUType x);

	/// @}
} //namespace aster_linear

#include "integer.inl"
