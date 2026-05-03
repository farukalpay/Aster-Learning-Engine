/// @ref gtx_mixed_product

namespace aster_linear
{
	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_QUALIFIER T mixedProduct
	(
		vec<3, T, Q> const& v1,
		vec<3, T, Q> const& v2,
		vec<3, T, Q> const& v3
	)
	{
		return dot(cross(v1, v2), v3);
	}
}//namespace aster_linear
