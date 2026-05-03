/// @ref gtx_mixed_product
/// @file aster_linear/gtx/mixed_product.hpp
///
/// @see core (dependence)
///
/// @defgroup gtx_mixed_product ASTER_LINEAR_GTX_mixed_producte
/// @ingroup gtx
///
/// Include <aster_linear/gtx/mixed_product.hpp> to use the features of this extension.
///
/// Mixed product of 3 vectors.

#pragma once

// Dependency:
#include "../aster_linear.hpp"

#if ASTER_LINEAR_MESSAGES == ASTER_LINEAR_ENABLE && !defined(ASTER_LINEAR_EXT_INCLUDED)
#	ifndef ASTER_LINEAR_ENABLE_EXPERIMENTAL
#		pragma message("ASTER_LINEAR: ASTER_LINEAR_GTX_mixed_product is an experimental extension and may change in the future. Use #define ASTER_LINEAR_ENABLE_EXPERIMENTAL before including it, if you really want to use it.")
#	else
#		pragma message("ASTER_LINEAR: ASTER_LINEAR_GTX_mixed_product extension included")
#	endif
#endif

namespace aster_linear
{
	/// @addtogroup gtx_mixed_product
	/// @{

	/// @brief Mixed product of 3 vectors (from ASTER_LINEAR_GTX_mixed_product extension)
	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL T mixedProduct(
		vec<3, T, Q> const& v1,
		vec<3, T, Q> const& v2,
		vec<3, T, Q> const& v3);

	/// @}
}// namespace aster_linear

#include "mixed_product.inl"
