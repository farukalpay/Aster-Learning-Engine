/// @ref gtx_matrix_cross_product
/// @file aster_linear/gtx/matrix_cross_product.hpp
///
/// @see core (dependence)
/// @see gtx_extented_min_max (dependence)
///
/// @defgroup gtx_matrix_cross_product ASTER_LINEAR_GTX_matrix_cross_product
/// @ingroup gtx
///
/// Include <aster_linear/gtx/matrix_cross_product.hpp> to use the features of this extension.
///
/// Build cross product matrices

#pragma once

// Dependency:
#include "../aster_linear.hpp"

#if ASTER_LINEAR_MESSAGES == ASTER_LINEAR_ENABLE && !defined(ASTER_LINEAR_EXT_INCLUDED)
#	ifndef ASTER_LINEAR_ENABLE_EXPERIMENTAL
#		pragma message("ASTER_LINEAR: ASTER_LINEAR_GTX_matrix_cross_product is an experimental extension and may change in the future. Use #define ASTER_LINEAR_ENABLE_EXPERIMENTAL before including it, if you really want to use it.")
#	else
#		pragma message("ASTER_LINEAR: ASTER_LINEAR_GTX_matrix_cross_product extension included")
#	endif
#endif

namespace aster_linear
{
	/// @addtogroup gtx_matrix_cross_product
	/// @{

	//! Build a cross product matrix.
	//! From ASTER_LINEAR_GTX_matrix_cross_product extension.
	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL mat<3, 3, T, Q> matrixCross3(
		vec<3, T, Q> const& x);

	//! Build a cross product matrix.
	//! From ASTER_LINEAR_GTX_matrix_cross_product extension.
	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL mat<4, 4, T, Q> matrixCross4(
		vec<3, T, Q> const& x);

	/// @}
}//namespace aster_linear

#include "matrix_cross_product.inl"
