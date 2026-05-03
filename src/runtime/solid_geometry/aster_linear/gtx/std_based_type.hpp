/// @ref gtx_std_based_type
/// @file aster_linear/gtx/std_based_type.hpp
///
/// @see core (dependence)
/// @see gtx_extented_min_max (dependence)
///
/// @defgroup gtx_std_based_type ASTER_LINEAR_GTX_std_based_type
/// @ingroup gtx
///
/// Include <aster_linear/gtx/std_based_type.hpp> to use the features of this extension.
///
/// Adds vector types based on STL value types.

#pragma once

// Dependency:
#include "../aster_linear.hpp"
#include <cstdlib>

#if ASTER_LINEAR_MESSAGES == ASTER_LINEAR_ENABLE && !defined(ASTER_LINEAR_EXT_INCLUDED)
#	ifndef ASTER_LINEAR_ENABLE_EXPERIMENTAL
#		pragma message("ASTER_LINEAR: ASTER_LINEAR_GTX_std_based_type is an experimental extension and may change in the future. Use #define ASTER_LINEAR_ENABLE_EXPERIMENTAL before including it, if you really want to use it.")
#	else
#		pragma message("ASTER_LINEAR: ASTER_LINEAR_GTX_std_based_type extension included")
#	endif
#endif

namespace aster_linear
{
	/// @addtogroup gtx_std_based_type
	/// @{

	/// Vector type based of one std::size_t component.
	/// @see ASTER_LINEAR_GTX_std_based_type
	typedef vec<1, std::size_t, defaultp>		size1;

	/// Vector type based of two std::size_t components.
	/// @see ASTER_LINEAR_GTX_std_based_type
	typedef vec<2, std::size_t, defaultp>		size2;

	/// Vector type based of three std::size_t components.
	/// @see ASTER_LINEAR_GTX_std_based_type
	typedef vec<3, std::size_t, defaultp>		size3;

	/// Vector type based of four std::size_t components.
	/// @see ASTER_LINEAR_GTX_std_based_type
	typedef vec<4, std::size_t, defaultp>		size4;

	/// Vector type based of one std::size_t component.
	/// @see ASTER_LINEAR_GTX_std_based_type
	typedef vec<1, std::size_t, defaultp>		size1_t;

	/// Vector type based of two std::size_t components.
	/// @see ASTER_LINEAR_GTX_std_based_type
	typedef vec<2, std::size_t, defaultp>		size2_t;

	/// Vector type based of three std::size_t components.
	/// @see ASTER_LINEAR_GTX_std_based_type
	typedef vec<3, std::size_t, defaultp>		size3_t;

	/// Vector type based of four std::size_t components.
	/// @see ASTER_LINEAR_GTX_std_based_type
	typedef vec<4, std::size_t, defaultp>		size4_t;

	/// @}
}//namespace aster_linear

#include "std_based_type.inl"
