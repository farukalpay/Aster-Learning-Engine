/// @ref ext_scalar_ulp
/// @file aster_linear/ext/scalar_ulp.hpp
///
/// @defgroup ext_scalar_ulp ASTER_LINEAR_EXT_scalar_ulp
/// @ingroup ext
///
/// Allow the measurement of the accuracy of a function against a reference
/// implementation. This extension works on floating-point data and provide results
/// in ULP.
///
/// Include <aster_linear/ext/scalar_ulp.hpp> to use the features of this extension.
///
/// @see ext_vector_ulp
/// @see ext_scalar_relational

#pragma once

// Dependencies
#include "../ext/scalar_int_sized.hpp"
#include "../common.hpp"
#include "../detail/qualifier.hpp"

#if ASTER_LINEAR_MESSAGES == ASTER_LINEAR_ENABLE && !defined(ASTER_LINEAR_EXT_INCLUDED)
#	pragma message("ASTER_LINEAR: ASTER_LINEAR_EXT_scalar_ulp extension included")
#endif

namespace aster_linear
{
	/// Return the next ULP value(s) after the input value(s).
	///
	/// @tparam genType A floating-point scalar type.
	///
	/// @see ext_scalar_ulp
	template<typename genType>
	ASTER_LINEAR_FUNC_DECL genType nextFloat(genType x);

	/// Return the previous ULP value(s) before the input value(s).
	///
	/// @tparam genType A floating-point scalar type.
	///
	/// @see ext_scalar_ulp
	template<typename genType>
	ASTER_LINEAR_FUNC_DECL genType prevFloat(genType x);

	/// Return the value(s) ULP distance after the input value(s).
	///
	/// @tparam genType A floating-point scalar type.
	///
	/// @see ext_scalar_ulp
	template<typename genType>
	ASTER_LINEAR_FUNC_DECL genType nextFloat(genType x, int ULPs);

	/// Return the value(s) ULP distance before the input value(s).
	///
	/// @tparam genType A floating-point scalar type.
	///
	/// @see ext_scalar_ulp
	template<typename genType>
	ASTER_LINEAR_FUNC_DECL genType prevFloat(genType x, int ULPs);

	/// Return the distance in the number of ULP between 2 single-precision floating-point scalars.
	///
	/// @see ext_scalar_ulp
	ASTER_LINEAR_FUNC_DECL int floatDistance(float x, float y);

	/// Return the distance in the number of ULP between 2 double-precision floating-point scalars.
	///
	/// @see ext_scalar_ulp
	ASTER_LINEAR_FUNC_DECL int64 floatDistance(double x, double y);

	/// @}
}//namespace aster_linear

#include "scalar_ulp.inl"
