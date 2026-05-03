/// @ref gtx_orthonormalize
/// @file aster_linear/gtx/orthonormalize.hpp
///
/// @see core (dependence)
/// @see gtx_extented_min_max (dependence)
///
/// @defgroup gtx_orthonormalize ASTER_LINEAR_GTX_orthonormalize
/// @ingroup gtx
///
/// Include <aster_linear/gtx/orthonormalize.hpp> to use the features of this extension.
///
/// Orthonormalize matrices.

#pragma once

// Dependency:
#include "../vec3.hpp"
#include "../mat3x3.hpp"
#include "../geometric.hpp"

#if ASTER_LINEAR_MESSAGES == ASTER_LINEAR_ENABLE && !defined(ASTER_LINEAR_EXT_INCLUDED)
#	ifndef ASTER_LINEAR_ENABLE_EXPERIMENTAL
#		pragma message("ASTER_LINEAR: ASTER_LINEAR_GTX_orthonormalize is an experimental extension and may change in the future. Use #define ASTER_LINEAR_ENABLE_EXPERIMENTAL before including it, if you really want to use it.")
#	else
#		pragma message("ASTER_LINEAR: ASTER_LINEAR_GTX_orthonormalize extension included")
#	endif
#endif

namespace aster_linear
{
	/// @addtogroup gtx_orthonormalize
	/// @{

	/// Returns the orthonormalized matrix of m.
	///
	/// @see gtx_orthonormalize
	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL mat<3, 3, T, Q> orthonormalize(mat<3, 3, T, Q> const& m);

	/// Orthonormalizes x according y.
	///
	/// @see gtx_orthonormalize
	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL vec<3, T, Q> orthonormalize(vec<3, T, Q> const& x, vec<3, T, Q> const& y);

	/// @}
}//namespace aster_linear

#include "orthonormalize.inl"
