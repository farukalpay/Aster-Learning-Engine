/// @ref gtx_log_base

namespace aster_linear
{
	template<typename genType>
	ASTER_LINEAR_FUNC_QUALIFIER genType log(genType const& x, genType const& base)
	{
		return aster_linear::log(x) / aster_linear::log(base);
	}

	template<length_t L, typename T, qualifier Q>
	ASTER_LINEAR_FUNC_QUALIFIER vec<L, T, Q> log(vec<L, T, Q> const& x, vec<L, T, Q> const& base)
	{
		return aster_linear::log(x) / aster_linear::log(base);
	}
}//namespace aster_linear
