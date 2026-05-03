/// @ref gtx_normalize_dot

namespace aster_linear
{
	template<length_t L, typename T, qualifier Q>
	ASTER_LINEAR_FUNC_QUALIFIER T normalizeDot(vec<L, T, Q> const& x, vec<L, T, Q> const& y)
	{
		return aster_linear::dot(x, y) * aster_linear::inversesqrt(aster_linear::dot(x, x) * aster_linear::dot(y, y));
	}

	template<length_t L, typename T, qualifier Q>
	ASTER_LINEAR_FUNC_QUALIFIER T fastNormalizeDot(vec<L, T, Q> const& x, vec<L, T, Q> const& y)
	{
		return aster_linear::dot(x, y) * aster_linear::fastInverseSqrt(aster_linear::dot(x, x) * aster_linear::dot(y, y));
	}
}//namespace aster_linear
