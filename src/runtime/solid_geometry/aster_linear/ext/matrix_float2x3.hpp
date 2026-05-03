/// @ref core
/// @file aster_linear/ext/matrix_float2x3.hpp

#pragma once
#include "../detail/type_mat2x3.hpp"

namespace aster_linear
{
	/// @addtogroup core_matrix
	/// @{

	/// 2 columns of 3 components matrix of single-precision floating-point numbers.
	///
	/// @see <a href="http://www.desktop_graphics.org/registry/doc/GLSLangSpec.4.20.8.pdf">GLSL 4.20.8 specification, section 4.1.6 Matrices</a>
	typedef mat<2, 3, float, defaultp>		mat2x3;

	/// @}
}//namespace aster_linear
