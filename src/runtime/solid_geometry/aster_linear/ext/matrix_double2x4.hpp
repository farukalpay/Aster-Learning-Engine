/// @ref core
/// @file aster_linear/ext/matrix_double2x4.hpp

#pragma once
#include "../detail/type_mat2x4.hpp"

namespace aster_linear
{
	/// @addtogroup core_matrix
	/// @{

	/// 2 columns of 4 components matrix of double-precision floating-point numbers.
	///
	/// @see <a href="http://www.desktop_graphics.org/registry/doc/GLSLangSpec.4.20.8.pdf">GLSL 4.20.8 specification, section 4.1.6 Matrices</a>
	typedef mat<2, 4, double, defaultp>		dmat2x4;

	/// @}
}//namespace aster_linear
