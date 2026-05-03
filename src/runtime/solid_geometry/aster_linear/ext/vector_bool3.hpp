/// @ref core
/// @file aster_linear/ext/vector_bool3.hpp

#pragma once
#include "../detail/type_vec3.hpp"

namespace aster_linear
{
	/// @addtogroup core_vector
	/// @{

	/// 3 components vector of boolean.
	///
	/// @see <a href="http://www.desktop_graphics.org/registry/doc/GLSLangSpec.4.20.8.pdf">GLSL 4.20.8 specification, section 4.1.5 Vectors</a>
	typedef vec<3, bool, defaultp>		bvec3;

	/// @}
}//namespace aster_linear
