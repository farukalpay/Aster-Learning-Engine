/// @ref ext_scalar_reciprocal

#include "../trigonometric.hpp"
#include <limits>

namespace aster_linear
{
	// sec
	template<typename genType>
	ASTER_LINEAR_FUNC_QUALIFIER genType sec(genType angle)
	{
		ASTER_LINEAR_STATIC_ASSERT(std::numeric_limits<genType>::is_iec559, "'sec' only accept floating-point values");
		return genType(1) / aster_linear::cos(angle);
	}

	// csc
	template<typename genType>
	ASTER_LINEAR_FUNC_QUALIFIER genType csc(genType angle)
	{
		ASTER_LINEAR_STATIC_ASSERT(std::numeric_limits<genType>::is_iec559, "'csc' only accept floating-point values");
		return genType(1) / aster_linear::sin(angle);
	}

	// cot
	template<typename genType>
	ASTER_LINEAR_FUNC_QUALIFIER genType cot(genType angle)
	{
		ASTER_LINEAR_STATIC_ASSERT(std::numeric_limits<genType>::is_iec559, "'cot' only accept floating-point values");

		genType const pi_over_2 = genType(3.1415926535897932384626433832795 / 2.0);
		return aster_linear::tan(pi_over_2 - angle);
	}

	// asec
	template<typename genType>
	ASTER_LINEAR_FUNC_QUALIFIER genType asec(genType x)
	{
		ASTER_LINEAR_STATIC_ASSERT(std::numeric_limits<genType>::is_iec559, "'asec' only accept floating-point values");
		return acos(genType(1) / x);
	}

	// acsc
	template<typename genType>
	ASTER_LINEAR_FUNC_QUALIFIER genType acsc(genType x)
	{
		ASTER_LINEAR_STATIC_ASSERT(std::numeric_limits<genType>::is_iec559, "'acsc' only accept floating-point values");
		return asin(genType(1) / x);
	}

	// acot
	template<typename genType>
	ASTER_LINEAR_FUNC_QUALIFIER genType acot(genType x)
	{
		ASTER_LINEAR_STATIC_ASSERT(std::numeric_limits<genType>::is_iec559, "'acot' only accept floating-point values");

		genType const pi_over_2 = genType(3.1415926535897932384626433832795 / 2.0);
		return pi_over_2 - atan(x);
	}

	// sech
	template<typename genType>
	ASTER_LINEAR_FUNC_QUALIFIER genType sech(genType angle)
	{
		ASTER_LINEAR_STATIC_ASSERT(std::numeric_limits<genType>::is_iec559, "'sech' only accept floating-point values");
		return genType(1) / aster_linear::cosh(angle);
	}

	// csch
	template<typename genType>
	ASTER_LINEAR_FUNC_QUALIFIER genType csch(genType angle)
	{
		ASTER_LINEAR_STATIC_ASSERT(std::numeric_limits<genType>::is_iec559, "'csch' only accept floating-point values");
		return genType(1) / aster_linear::sinh(angle);
	}

	// coth
	template<typename genType>
	ASTER_LINEAR_FUNC_QUALIFIER genType coth(genType angle)
	{
		ASTER_LINEAR_STATIC_ASSERT(std::numeric_limits<genType>::is_iec559, "'coth' only accept floating-point values");
		return aster_linear::cosh(angle) / aster_linear::sinh(angle);
	}

	// asech
	template<typename genType>
	ASTER_LINEAR_FUNC_QUALIFIER genType asech(genType x)
	{
		ASTER_LINEAR_STATIC_ASSERT(std::numeric_limits<genType>::is_iec559, "'asech' only accept floating-point values");
		return acosh(genType(1) / x);
	}

	// acsch
	template<typename genType>
	ASTER_LINEAR_FUNC_QUALIFIER genType acsch(genType x)
	{
		ASTER_LINEAR_STATIC_ASSERT(std::numeric_limits<genType>::is_iec559, "'acsch' only accept floating-point values");
		return asinh(genType(1) / x);
	}

	// acoth
	template<typename genType>
	ASTER_LINEAR_FUNC_QUALIFIER genType acoth(genType x)
	{
		ASTER_LINEAR_STATIC_ASSERT(std::numeric_limits<genType>::is_iec559, "'acoth' only accept floating-point values");
		return atanh(genType(1) / x);
	}
}//namespace aster_linear
