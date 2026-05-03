#if ASTER_LINEAR_ARCH & ASTER_LINEAR_ARCH_SSE2_BIT

namespace aster_linear{
namespace detail
{
	template<qualifier Q>
	struct compute_dot<qua<float, Q>, float, true>
	{
		static ASTER_LINEAR_FUNC_QUALIFIER float call(qua<float, Q> const& x, qua<float, Q> const& y)
		{
			return _mm_cvtss_f32(alinear_vec1_dot(x.data, y.data));
		}
	};
}//namespace detail
}//namespace aster_linear

#endif//ASTER_LINEAR_ARCH & ASTER_LINEAR_ARCH_SSE2_BIT

