/// @ref gtx_matrix_decompose
/// @file aster_linear/gtx/matrix_decompose.hpp
///
/// @see core (dependence)
///
/// @defgroup gtx_matrix_decompose ASTER_LINEAR_GTX_matrix_decompose
/// @ingroup gtx
///
/// Include <aster_linear/gtx/matrix_decompose.hpp> to use the features of this extension.
///
/// Decomposes a model matrix to translations, rotation and scale components

#pragma once

// Dependencies
#include "../mat4x4.hpp"
#include "../vec3.hpp"
#include "../vec4.hpp"
#include "../geometric.hpp"
#include "../gtc/quaternion.hpp"
#include "../gtc/matrix_transform.hpp"

#if ASTER_LINEAR_MESSAGES == ASTER_LINEAR_ENABLE && !defined(ASTER_LINEAR_EXT_INCLUDED)
#	ifndef ASTER_LINEAR_ENABLE_EXPERIMENTAL
#		pragma message("ASTER_LINEAR: ASTER_LINEAR_GTX_matrix_decompose is an experimental extension and may change in the future. Use #define ASTER_LINEAR_ENABLE_EXPERIMENTAL before including it, if you really want to use it.")
#	else
#		pragma message("ASTER_LINEAR: ASTER_LINEAR_GTX_matrix_decompose extension included")
#	endif
#endif

namespace aster_linear
{
	/// @addtogroup gtx_matrix_decompose
	/// @{

	/// Decomposes a model matrix to translations, rotation and scale components
	/// @see gtx_matrix_decompose
	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL bool decompose(
		mat<4, 4, T, Q> const& modelMatrix,
		vec<3, T, Q> & scale, qua<T, Q> & orientation, vec<3, T, Q> & translation, vec<3, T, Q> & skew, vec<4, T, Q> & perspective);

	/// @}
}//namespace aster_linear

#include "matrix_decompose.inl"
