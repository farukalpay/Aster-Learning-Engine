/// @ref gtx_exterior_product
/// @file aster_linear/gtx/exterior_product.hpp
///
/// @see core (dependence)
/// @see gtx_exterior_product (dependence)
///
/// @defgroup gtx_exterior_product ASTER_LINEAR_GTX_exterior_product
/// @ingroup gtx
///
/// Include <aster_linear/gtx/exterior_product.hpp> to use the features of this extension.
///
/// @brief Allow to perform bit operations on integer values

#pragma once

// Dependencies
#include "../detail/setup.hpp"
#include "../detail/qualifier.hpp"

#if ASTER_LINEAR_MESSAGES == ASTER_LINEAR_ENABLE && !defined(ASTER_LINEAR_EXT_INCLUDED)
#	ifndef ASTER_LINEAR_ENABLE_EXPERIMENTAL
#		pragma message("ASTER_LINEAR: ASTER_LINEAR_GTX_exterior_product is an experimental extension and may change in the future. Use #define ASTER_LINEAR_ENABLE_EXPERIMENTAL before including it, if you really want to use it.")
#	else
#		pragma message("ASTER_LINEAR: ASTER_LINEAR_GTX_exterior_product extension included")
#	endif
#endif

namespace aster_linear
{
	/// @addtogroup gtx_exterior_product
	/// @{

	/// Returns the cross product of x and y.
	///
	/// @tparam T Floating-point scalar types
	/// @tparam Q Value from qualifier enum
	///
	/// @see <a href="https://en.wikipedia.org/wiki/Exterior_algebra#Cross_and_triple_products">Exterior product</a>
	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL T cross(vec<2, T, Q> const& v, vec<2, T, Q> const& u);

	/// @}
} //namespace aster_linear

#include "exterior_product.inl"
