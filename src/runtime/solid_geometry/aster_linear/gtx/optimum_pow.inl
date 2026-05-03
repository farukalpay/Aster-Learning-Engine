/// @ref gtx_optimum_pow

namespace aster_linear
{
	template<typename genType>
	ASTER_LINEAR_FUNC_QUALIFIER genType pow2(genType const& x)
	{
		return x * x;
	}

	template<typename genType>
	ASTER_LINEAR_FUNC_QUALIFIER genType pow3(genType const& x)
	{
		return x * x * x;
	}

	template<typename genType>
	ASTER_LINEAR_FUNC_QUALIFIER genType pow4(genType const& x)
	{
		return (x * x) * (x * x);
	}
}//namespace aster_linear
