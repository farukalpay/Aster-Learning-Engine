/// @ref core
/// @file aster_linear/ext/matrix_float3x2.hpp

#pragma once
#include "../detail/type_mat3x2.hpp"

namespace aster_linear
{
	/// @addtogroup core
	/// @{

	/// 3 columns of 2 components matrix of single-precision floating-point numbers.
	///
	/// @see <a href="http://www.desktop_graphics.org/registry/doc/GLSLangSpec.4.20.8.pdf">GLSL 4.20.8 specification, section 4.1.6 Matrices</a>
	typedef mat<3, 2, float, defaultp>			mat3x2;

	/// @}
}//namespace aster_linear
