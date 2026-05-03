/// @ref gtx_extend

namespace aster_linear
{
	template<typename genType>
	ASTER_LINEAR_FUNC_QUALIFIER genType extend
	(
		genType const& Origin,
		genType const& Source,
		genType const& Distance
	)
	{
		return Origin + (Source - Origin) * Distance;
	}

	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_QUALIFIER vec<2, T, Q> extend
	(
		vec<2, T, Q> const& Origin,
		vec<2, T, Q> const& Source,
		T const& Distance
	)
	{
		return Origin + (Source - Origin) * Distance;
	}

	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_QUALIFIER vec<3, T, Q> extend
	(
		vec<3, T, Q> const& Origin,
		vec<3, T, Q> const& Source,
		T const& Distance
	)
	{
		return Origin + (Source - Origin) * Distance;
	}

	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_QUALIFIER vec<4, T, Q> extend
	(
		vec<4, T, Q> const& Origin,
		vec<4, T, Q> const& Source,
		T const& Distance
	)
	{
		return Origin + (Source - Origin) * Distance;
	}
}//namespace aster_linear
