/// @ref gtx_functions

#include "../exponential.hpp"

namespace aster_linear
{
	template<typename T>
	ASTER_LINEAR_FUNC_QUALIFIER T gauss
	(
		T x,
		T ExpectedValue,
		T StandardDeviation
	)
	{
		return exp(-((x - ExpectedValue) * (x - ExpectedValue)) / (static_cast<T>(2) * StandardDeviation * StandardDeviation)) / (StandardDeviation * sqrt(static_cast<T>(6.28318530717958647692528676655900576)));
	}

	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_QUALIFIER T gauss
	(
		vec<2, T, Q> const& Coord,
		vec<2, T, Q> const& ExpectedValue,
		vec<2, T, Q> const& StandardDeviation
	)
	{
		vec<2, T, Q> const Squared = ((Coord - ExpectedValue) * (Coord - ExpectedValue)) / (static_cast<T>(2) * StandardDeviation * StandardDeviation);
		return exp(-(Squared.x + Squared.y));
	}
}//namespace aster_linear

