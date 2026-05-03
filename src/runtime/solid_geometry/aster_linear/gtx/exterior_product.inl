/// @ref gtx_exterior_product

#include <limits>

namespace aster_linear {
namespace detail
{
	template<typename T, qualifier Q, bool Aligned>
	struct compute_cross_vec2
	{
		ASTER_LINEAR_FUNC_QUALIFIER static T call(vec<2, T, Q> const& v, vec<2, T, Q> const& u)
		{
			ASTER_LINEAR_STATIC_ASSERT(std::numeric_limits<T>::is_iec559, "'cross' accepts only floating-point inputs");

			return v.x * u.y - u.x * v.y;
		}
	};
}//namespace detail

	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_QUALIFIER T cross(vec<2, T, Q> const& x, vec<2, T, Q> const& y)
	{
		return detail::compute_cross_vec2<T, Q, detail::is_aligned<Q>::value>::call(x, y);
	}
}//namespace aster_linear

