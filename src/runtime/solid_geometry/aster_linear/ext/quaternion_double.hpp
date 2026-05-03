/// @ref ext_quaternion_double
/// @file aster_linear/ext/quaternion_double.hpp
///
/// @defgroup ext_quaternion_double ASTER_LINEAR_EXT_quaternion_double
/// @ingroup ext
///
/// Exposes double-precision floating point quaternion type.
///
/// Include <aster_linear/ext/quaternion_double.hpp> to use the features of this extension.
///
/// @see ext_quaternion_float
/// @see ext_quaternion_double_precision
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
#	pragma message("ASTER_LINEAR: ASTER_LINEAR_EXT_quaternion_double extension included")
#endif

namespace aster_linear
{
	/// @addtogroup ext_quaternion_double
	/// @{

	/// Quaternion of double-precision floating-point numbers.
	typedef qua<double, defaultp>		dquat;

	/// @}
} //namespace aster_linear

