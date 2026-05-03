/// @ref gtx_projection

namespace aster_linear
{
	template<typename genType>
	ASTER_LINEAR_FUNC_QUALIFIER genType proj(genType const& x, genType const& Normal)
	{
		return aster_linear::dot(x, Normal) / aster_linear::dot(Normal, Normal) * Normal;
	}
}//namespace aster_linear
