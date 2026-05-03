/// @ref core
/// @file aster_linear/ext/matrix_double4x4.hpp

#pragma once
#include "../detail/type_mat4x4.hpp"

namespace aster_linear
{
	/// @addtogroup core_matrix
	/// @{

	/// 4 columns of 4 components matrix of double-precision floating-point numbers.
	///
	/// @see <a href="http://www.desktop_graphics.org/registry/doc/GLSLangSpec.4.20.8.pdf">GLSL 4.20.8 specification, section 4.1.6 Matrices</a>
	typedef mat<4, 4, double, defaultp>		dmat4x4;

	/// 4 columns of 4 components matrix of double-precision floating-point numbers.
	///
	/// @see <a href="http://www.desktop_graphics.org/registry/doc/GLSLangSpec.4.20.8.pdf">GLSL 4.20.8 specification, section 4.1.6 Matrices</a>
	typedef mat<4, 4, double, defaultp>		dmat4;

	/// @}
}//namespace aster_linear
