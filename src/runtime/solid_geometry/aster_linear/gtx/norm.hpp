/// @ref gtx_norm
/// @file aster_linear/gtx/norm.hpp
///
/// @see core (dependence)
/// @see gtx_quaternion (dependence)
/// @see gtx_component_wise (dependence)
///
/// @defgroup gtx_norm ASTER_LINEAR_GTX_norm
/// @ingroup gtx
///
/// Include <aster_linear/gtx/norm.hpp> to use the features of this extension.
///
/// Various ways to compute vector norms.

#pragma once

// Dependency:
#include "../geometric.hpp"
#include "../gtx/quaternion.hpp"
#include "../gtx/component_wise.hpp"

#if ASTER_LINEAR_MESSAGES == ASTER_LINEAR_ENABLE && !defined(ASTER_LINEAR_EXT_INCLUDED)
#	ifndef ASTER_LINEAR_ENABLE_EXPERIMENTAL
#		pragma message("ASTER_LINEAR: ASTER_LINEAR_GTX_norm is an experimental extension and may change in the future. Use #define ASTER_LINEAR_ENABLE_EXPERIMENTAL before including it, if you really want to use it.")
#	else
#		pragma message("ASTER_LINEAR: ASTER_LINEAR_GTX_norm extension included")
#	endif
#endif

namespace aster_linear
{
	/// @addtogroup gtx_norm
	/// @{

	/// Returns the squared length of x.
	/// From ASTER_LINEAR_GTX_norm extension.
	template<length_t L, typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL T length2(vec<L, T, Q> const& x);

	/// Returns the squared distance between p0 and p1, i.e., length2(p0 - p1).
	/// From ASTER_LINEAR_GTX_norm extension.
	template<length_t L, typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL T distance2(vec<L, T, Q> const& p0, vec<L, T, Q> const& p1);

	//! Returns the L1 norm between x and y.
	//! From ASTER_LINEAR_GTX_norm extension.
	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL T l1Norm(vec<3, T, Q> const& x, vec<3, T, Q> const& y);

	//! Returns the L1 norm of v.
	//! From ASTER_LINEAR_GTX_norm extension.
	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL T l1Norm(vec<3, T, Q> const& v);

	//! Returns the L2 norm between x and y.
	//! From ASTER_LINEAR_GTX_norm extension.
	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL T l2Norm(vec<3, T, Q> const& x, vec<3, T, Q> const& y);

	//! Returns the L2 norm of v.
	//! From ASTER_LINEAR_GTX_norm extension.
	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL T l2Norm(vec<3, T, Q> const& x);

	//! Returns the L norm between x and y.
	//! From ASTER_LINEAR_GTX_norm extension.
	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL T lxNorm(vec<3, T, Q> const& x, vec<3, T, Q> const& y, unsigned int Depth);

	//! Returns the L norm of v.
	//! From ASTER_LINEAR_GTX_norm extension.
	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL T lxNorm(vec<3, T, Q> const& x, unsigned int Depth);

	//! Returns the LMax norm between x and y.
	//! From ASTER_LINEAR_GTX_norm extension.
	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL T lMaxNorm(vec<3, T, Q> const& x, vec<3, T, Q> const& y);

	//! Returns the LMax norm of v.
	//! From ASTER_LINEAR_GTX_norm extension.
	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL T lMaxNorm(vec<3, T, Q> const& x);

	/// @}
}//namespace aster_linear

#include "norm.inl"
