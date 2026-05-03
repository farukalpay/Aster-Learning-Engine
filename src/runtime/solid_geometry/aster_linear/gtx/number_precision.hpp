/// @ref gtx_number_precision
/// @file aster_linear/gtx/number_precision.hpp
///
/// @see core (dependence)
/// @see gtc_type_precision (dependence)
/// @see gtc_quaternion (dependence)
///
/// @defgroup gtx_number_precision ASTER_LINEAR_GTX_number_precision
/// @ingroup gtx
///
/// Include <aster_linear/gtx/number_precision.hpp> to use the features of this extension.
///
/// Defined size types.

#pragma once

// Dependency:
#include "../aster_linear.hpp"
#include "../gtc/type_precision.hpp"

#if ASTER_LINEAR_MESSAGES == ASTER_LINEAR_ENABLE && !defined(ASTER_LINEAR_EXT_INCLUDED)
#	ifndef ASTER_LINEAR_ENABLE_EXPERIMENTAL
#		pragma message("ASTER_LINEAR: ASTER_LINEAR_GTX_number_precision is an experimental extension and may change in the future. Use #define ASTER_LINEAR_ENABLE_EXPERIMENTAL before including it, if you really want to use it.")
#	else
#		pragma message("ASTER_LINEAR: ASTER_LINEAR_GTX_number_precision extension included")
#	endif
#endif

namespace aster_linear{
namespace gtx
{
	/////////////////////////////
	// Unsigned int vector types

	/// @addtogroup gtx_number_precision
	/// @{

	typedef u8			u8vec1;		//!< \brief 8bit unsigned integer scalar. (from ASTER_LINEAR_GTX_number_precision extension)
	typedef u16			u16vec1;    //!< \brief 16bit unsigned integer scalar. (from ASTER_LINEAR_GTX_number_precision extension)
	typedef u32			u32vec1;    //!< \brief 32bit unsigned integer scalar. (from ASTER_LINEAR_GTX_number_precision extension)
	typedef u64			u64vec1;    //!< \brief 64bit unsigned integer scalar. (from ASTER_LINEAR_GTX_number_precision extension)

	//////////////////////
	// Float vector types

	typedef f32			f32vec1;    //!< \brief Single-qualifier floating-point scalar. (from ASTER_LINEAR_GTX_number_precision extension)
	typedef f64			f64vec1;    //!< \brief Single-qualifier floating-point scalar. (from ASTER_LINEAR_GTX_number_precision extension)

	//////////////////////
	// Float matrix types

	typedef f32			f32mat1;	//!< \brief Single-qualifier floating-point scalar. (from ASTER_LINEAR_GTX_number_precision extension)
	typedef f32			f32mat1x1;	//!< \brief Single-qualifier floating-point scalar. (from ASTER_LINEAR_GTX_number_precision extension)
	typedef f64			f64mat1;	//!< \brief Double-qualifier floating-point scalar. (from ASTER_LINEAR_GTX_number_precision extension)
	typedef f64			f64mat1x1;	//!< \brief Double-qualifier floating-point scalar. (from ASTER_LINEAR_GTX_number_precision extension)

	/// @}
}//namespace gtx
}//namespace aster_linear

#include "number_precision.inl"
