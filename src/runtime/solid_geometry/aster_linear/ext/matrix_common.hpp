/// @ref ext_matrix_common
/// @file aster_linear/ext/matrix_common.hpp
///
/// @defgroup ext_matrix_common ASTER_LINEAR_EXT_matrix_common
/// @ingroup ext
///
/// Defines functions for common matrix operations.
///
/// Include <aster_linear/ext/matrix_common.hpp> to use the features of this extension.
///
/// @see ext_matrix_common

#pragma once

#include "../detail/qualifier.hpp"
#include "../detail/_fixes.hpp"

#if ASTER_LINEAR_MESSAGES == ASTER_LINEAR_ENABLE && !defined(ASTER_LINEAR_EXT_INCLUDED)
#	pragma message("ASTER_LINEAR: ASTER_LINEAR_EXT_matrix_transform extension included")
#endif

namespace aster_linear
{
	/// @addtogroup ext_matrix_common
	/// @{

	template<length_t C, length_t R, typename T, typename U, qualifier Q>
	ASTER_LINEAR_FUNC_DECL mat<C, R, T, Q> mix(mat<C, R, T, Q> const& x, mat<C, R, T, Q> const& y, mat<C, R, U, Q> const& a);

	template<length_t C, length_t R, typename T, typename U, qualifier Q>
	ASTER_LINEAR_FUNC_DECL mat<C, R, T, Q> mix(mat<C, R, T, Q> const& x, mat<C, R, T, Q> const& y, U a);

	/// @}
}//namespace aster_linear

#include "matrix_common.inl"
