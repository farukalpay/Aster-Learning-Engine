/// @ref gtx_raw_data
/// @file aster_linear/gtx/raw_data.hpp
///
/// @see core (dependence)
///
/// @defgroup gtx_raw_data ASTER_LINEAR_GTX_raw_data
/// @ingroup gtx
///
/// Include <aster_linear/gtx/raw_data.hpp> to use the features of this extension.
///
/// Projection of a vector to other one

#pragma once

// Dependencies
#include "../ext/scalar_uint_sized.hpp"
#include "../detail/setup.hpp"

#if ASTER_LINEAR_MESSAGES == ASTER_LINEAR_ENABLE && !defined(ASTER_LINEAR_EXT_INCLUDED)
#	ifndef ASTER_LINEAR_ENABLE_EXPERIMENTAL
#		pragma message("ASTER_LINEAR: ASTER_LINEAR_GTX_raw_data is an experimental extension and may change in the future. Use #define ASTER_LINEAR_ENABLE_EXPERIMENTAL before including it, if you really want to use it.")
#	else
#		pragma message("ASTER_LINEAR: ASTER_LINEAR_GTX_raw_data extension included")
#	endif
#endif

namespace aster_linear
{
	/// @addtogroup gtx_raw_data
	/// @{

	//! Type for byte numbers.
	//! From ASTER_LINEAR_GTX_raw_data extension.
	typedef detail::uint8		byte;

	//! Type for word numbers.
	//! From ASTER_LINEAR_GTX_raw_data extension.
	typedef detail::uint16		word;

	//! Type for dword numbers.
	//! From ASTER_LINEAR_GTX_raw_data extension.
	typedef detail::uint32		dword;

	//! Type for qword numbers.
	//! From ASTER_LINEAR_GTX_raw_data extension.
	typedef detail::uint64		qword;

	/// @}
}// namespace aster_linear

#include "raw_data.inl"
