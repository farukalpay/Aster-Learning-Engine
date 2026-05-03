/// @ref gtx
/// @file aster_linear/gtx/scalar_multiplication.hpp
/// @author Joshua Moerman
///
/// Include <aster_linear/gtx/scalar_multiplication.hpp> to use the features of this extension.
///
/// Enables scalar multiplication for all types
///
/// Since GLSL is very strict about types, the following (often used) combinations do not work:
///    double * vec4
///    int * vec4
///    vec4 / int
/// So we'll fix that! Of course "float * vec4" should remain the same (hence the enable_if magic)

#pragma once

#include "../detail/setup.hpp"

#if ASTER_LINEAR_MESSAGES == ASTER_LINEAR_ENABLE && !defined(ASTER_LINEAR_EXT_INCLUDED)
#	ifndef ASTER_LINEAR_ENABLE_EXPERIMENTAL
#		pragma message("ASTER_LINEAR: ASTER_LINEAR_GTX_scalar_multiplication is an experimental extension and may change in the future. Use #define ASTER_LINEAR_ENABLE_EXPERIMENTAL before including it, if you really want to use it.")
#	else
#		pragma message("ASTER_LINEAR: ASTER_LINEAR_GTX_scalar_multiplication extension included")
#	endif
#endif

#include "../vec2.hpp"
#include "../vec3.hpp"
#include "../vec4.hpp"
#include "../mat2x2.hpp"
#include <type_traits>

namespace aster_linear
{
	template<typename T, typename Vec>
	using return_type_scalar_multiplication = typename std::enable_if<
		!std::is_same<T, float>::value       // T may not be a float
		&& std::is_arithmetic<T>::value, Vec // But it may be an int or double (no vec3 or mat3, ...)
	>::type;

#define ASTER_LINEAR_IMPLEMENT_SCAL_MULT(Vec) \
	template<typename T> \
	return_type_scalar_multiplication<T, Vec> \
	operator*(T const& s, Vec rh){ \
		return rh *= static_cast<float>(s); \
	} \
	 \
	template<typename T> \
	return_type_scalar_multiplication<T, Vec> \
	operator*(Vec lh, T const& s){ \
		return lh *= static_cast<float>(s); \
	} \
	 \
	template<typename T> \
	return_type_scalar_multiplication<T, Vec> \
	operator/(Vec lh, T const& s){ \
		return lh *= 1.0f / static_cast<float>(s); \
	}

ASTER_LINEAR_IMPLEMENT_SCAL_MULT(vec2)
ASTER_LINEAR_IMPLEMENT_SCAL_MULT(vec3)
ASTER_LINEAR_IMPLEMENT_SCAL_MULT(vec4)

ASTER_LINEAR_IMPLEMENT_SCAL_MULT(mat2)
ASTER_LINEAR_IMPLEMENT_SCAL_MULT(mat2x3)
ASTER_LINEAR_IMPLEMENT_SCAL_MULT(mat2x4)
ASTER_LINEAR_IMPLEMENT_SCAL_MULT(mat3x2)
ASTER_LINEAR_IMPLEMENT_SCAL_MULT(mat3)
ASTER_LINEAR_IMPLEMENT_SCAL_MULT(mat3x4)
ASTER_LINEAR_IMPLEMENT_SCAL_MULT(mat4x2)
ASTER_LINEAR_IMPLEMENT_SCAL_MULT(mat4x3)
ASTER_LINEAR_IMPLEMENT_SCAL_MULT(mat4)

#undef ASTER_LINEAR_IMPLEMENT_SCAL_MULT
} // namespace aster_linear
