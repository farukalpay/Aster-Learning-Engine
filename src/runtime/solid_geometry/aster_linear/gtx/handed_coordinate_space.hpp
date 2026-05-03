/// @ref gtx_handed_coordinate_space
/// @file aster_linear/gtx/handed_coordinate_space.hpp
///
/// @see core (dependence)
///
/// @defgroup gtx_handed_coordinate_space ASTER_LINEAR_GTX_handed_coordinate_space
/// @ingroup gtx
///
/// Include <aster_linear/gtx/handed_coordinate_system.hpp> to use the features of this extension.
///
/// To know if a set of three basis vectors defines a right or left-handed coordinate system.

#pragma once

// Dependency:
#include "../aster_linear.hpp"

#if ASTER_LINEAR_MESSAGES == ASTER_LINEAR_ENABLE && !defined(ASTER_LINEAR_EXT_INCLUDED)
#	ifndef ASTER_LINEAR_ENABLE_EXPERIMENTAL
#		pragma message("ASTER_LINEAR: ASTER_LINEAR_GTX_handed_coordinate_space is an experimental extension and may change in the future. Use #define ASTER_LINEAR_ENABLE_EXPERIMENTAL before including it, if you really want to use it.")
#	else
#		pragma message("ASTER_LINEAR: ASTER_LINEAR_GTX_handed_coordinate_space extension included")
#	endif
#endif

namespace aster_linear
{
	/// @addtogroup gtx_handed_coordinate_space
	/// @{

	//! Return if a trihedron right handed or not.
	//! From ASTER_LINEAR_GTX_handed_coordinate_space extension.
	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL bool rightHanded(
		vec<3, T, Q> const& tangent,
		vec<3, T, Q> const& binormal,
		vec<3, T, Q> const& normal);

	//! Return if a trihedron left handed or not.
	//! From ASTER_LINEAR_GTX_handed_coordinate_space extension.
	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL bool leftHanded(
		vec<3, T, Q> const& tangent,
		vec<3, T, Q> const& binormal,
		vec<3, T, Q> const& normal);

	/// @}
}// namespace aster_linear

#include "handed_coordinate_space.inl"
