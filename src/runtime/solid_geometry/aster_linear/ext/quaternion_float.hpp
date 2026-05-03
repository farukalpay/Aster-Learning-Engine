/// @ref ext_quaternion_float
/// @file aster_linear/ext/quaternion_float.hpp
///
/// @defgroup ext_quaternion_float ASTER_LINEAR_EXT_quaternion_float
/// @ingroup ext
///
/// Exposes single-precision floating point quaternion type.
///
/// Include <aster_linear/ext/quaternion_float.hpp> to use the features of this extension.
///
/// @see ext_quaternion_double
/// @see ext_quaternion_float_precision
/// @see ext_quaternion_common
/// @see ext_quaternion_exponential
/// @see ext_quaternion_geometric
/// @see ext_quaternion_relational
/// @see ext_quaternion_transform
/// @see ext_quaternion_trigonometric

#pragma once

// Dependency:
#include "../detail/type_quat.hpp"

#if ASTER_LINEAR_MESSAGES == ASTER_LINEAR_ENABLE && !defined(ASTER_LINEAR_EXT_INCLUDED)
#	pragma message("ASTER_LINEAR: ASTER_LINEAR_EXT_quaternion_float extension included")
#endif

namespace aster_linear
{
	/// @addtogroup ext_quaternion_float
	/// @{

	/// Quaternion of single-precision floating-point numbers.
	typedef qua<float, defaultp>		quat;

	/// @}
} //namespace aster_linear

