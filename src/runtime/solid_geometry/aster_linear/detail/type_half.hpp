#pragma once

#include "setup.hpp"

namespace aster_linear{
namespace detail
{
	typedef short hdata;

	ASTER_LINEAR_FUNC_DECL float toFloat32(hdata value);
	ASTER_LINEAR_FUNC_DECL hdata toFloat16(float const& value);

}//namespace detail
}//namespace aster_linear

#include "type_half.inl"
