/// @ref gtx_perpendicular

namespace aster_linear
{
	template<typename genType>
	ASTER_LINEAR_FUNC_QUALIFIER genType perp(genType const& x, genType const& Normal)
	{
		return x - proj(x, Normal);
	}
}//namespace aster_linear
