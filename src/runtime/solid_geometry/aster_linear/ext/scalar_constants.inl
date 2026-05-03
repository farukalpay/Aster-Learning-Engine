#include <limits>

namespace aster_linear
{
	template<typename genType>
	ASTER_LINEAR_FUNC_QUALIFIER ASTER_LINEAR_CONSTEXPR genType epsilon()
	{
		ASTER_LINEAR_STATIC_ASSERT(std::numeric_limits<genType>::is_iec559, "'epsilon' only accepts floating-point inputs");
		return std::numeric_limits<genType>::epsilon();
	}

	template<typename genType>
	ASTER_LINEAR_FUNC_QUALIFIER ASTER_LINEAR_CONSTEXPR genType pi()
	{
		ASTER_LINEAR_STATIC_ASSERT(std::numeric_limits<genType>::is_iec559, "'pi' only accepts floating-point inputs");
		return static_cast<genType>(3.14159265358979323846264338327950288);
	}

	template<typename genType>
	ASTER_LINEAR_FUNC_QUALIFIER ASTER_LINEAR_CONSTEXPR genType cos_one_over_two()
	{
		return genType(0.877582561890372716130286068203503191);
	}
} //namespace aster_linear
