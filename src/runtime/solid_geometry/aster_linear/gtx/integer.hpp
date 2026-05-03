/// @ref gtx_integer
/// @file aster_linear/gtx/integer.hpp
///
/// @see core (dependence)
///
/// @defgroup gtx_integer ASTER_LINEAR_GTX_integer
/// @ingroup gtx
///
/// Include <aster_linear/gtx/integer.hpp> to use the features of this extension.
///
/// Add support for integer for core functions

#pragma once

// Dependency:
#include "../aster_linear.hpp"
#include "../gtc/integer.hpp"

#if ASTER_LINEAR_MESSAGES == ASTER_LINEAR_ENABLE && !defined(ASTER_LINEAR_EXT_INCLUDED)
#	ifndef ASTER_LINEAR_ENABLE_EXPERIMENTAL
#		pragma message("ASTER_LINEAR: ASTER_LINEAR_GTX_integer is an experimental extension and may change in the future. Use #define ASTER_LINEAR_ENABLE_EXPERIMENTAL before including it, if you really want to use it.")
#	else
#		pragma message("ASTER_LINEAR: ASTER_LINEAR_GTX_integer extension included")
#	endif
#endif

namespace aster_linear
{
	/// @addtogroup gtx_integer
	/// @{

	//! Returns x raised to the y power.
	//! From ASTER_LINEAR_GTX_integer extension.
	ASTER_LINEAR_FUNC_DECL int pow(int x, uint y);

	//! Returns the positive square root of x.
	//! From ASTER_LINEAR_GTX_integer extension.
	ASTER_LINEAR_FUNC_DECL int sqrt(int x);

	//! Returns the floor log2 of x.
	//! From ASTER_LINEAR_GTX_integer extension.
	ASTER_LINEAR_FUNC_DECL unsigned int floor_log2(unsigned int x);

	//! Modulus. Returns x - y * floor(x / y) for each component in x using the floating point value y.
	//! From ASTER_LINEAR_GTX_integer extension.
	ASTER_LINEAR_FUNC_DECL int mod(int x, int y);

	//! Return the factorial value of a number (!12 max, integer only)
	//! From ASTER_LINEAR_GTX_integer extension.
	template<typename genType>
	ASTER_LINEAR_FUNC_DECL genType factorial(genType const& x);

	//! 32bit signed integer.
	//! From ASTER_LINEAR_GTX_integer extension.
	typedef signed int					sint;

	//! Returns x raised to the y power.
	//! From ASTER_LINEAR_GTX_integer extension.
	ASTER_LINEAR_FUNC_DECL uint pow(uint x, uint y);

	//! Returns the positive square root of x.
	//! From ASTER_LINEAR_GTX_integer extension.
	ASTER_LINEAR_FUNC_DECL uint sqrt(uint x);

	//! Modulus. Returns x - y * floor(x / y) for each component in x using the floating point value y.
	//! From ASTER_LINEAR_GTX_integer extension.
	ASTER_LINEAR_FUNC_DECL uint mod(uint x, uint y);

	//! Returns the number of leading zeros.
	//! From ASTER_LINEAR_GTX_integer extension.
	ASTER_LINEAR_FUNC_DECL uint nlz(uint x);

	/// @}
}//namespace aster_linear

#include "integer.inl"
