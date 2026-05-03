/// @ref gtc_noise
/// @file aster_linear/gtc/noise.hpp
///
/// @see core (dependence)
///
/// @defgroup gtc_noise ASTER_LINEAR_GTC_noise
/// @ingroup gtc
///
/// Include <aster_linear/gtc/noise.hpp> to use the features of this extension.
///
/// Defines 2D, 3D and 4D procedural noise functions

#pragma once

// Dependencies
#include "../detail/setup.hpp"
#include "../detail/qualifier.hpp"
#include "../detail/_noise.hpp"
#include "../geometric.hpp"
#include "../common.hpp"
#include "../vector_relational.hpp"
#include "../vec2.hpp"
#include "../vec3.hpp"
#include "../vec4.hpp"

#if ASTER_LINEAR_MESSAGES == ASTER_LINEAR_ENABLE && !defined(ASTER_LINEAR_EXT_INCLUDED)
#	pragma message("ASTER_LINEAR: ASTER_LINEAR_GTC_noise extension included")
#endif

namespace aster_linear
{
	/// @addtogroup gtc_noise
	/// @{

	/// Classic perlin noise.
	/// @see gtc_noise
	template<length_t L, typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL T perlin(
		vec<L, T, Q> const& p);

	/// Periodic perlin noise.
	/// @see gtc_noise
	template<length_t L, typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL T perlin(
		vec<L, T, Q> const& p,
		vec<L, T, Q> const& rep);

	/// Simplex noise.
	/// @see gtc_noise
	template<length_t L, typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL T simplex(
		vec<L, T, Q> const& p);

	/// @}
}//namespace aster_linear

#include "noise.inl"
