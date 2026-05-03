/// @ref gtx_extended_min_max
/// @file aster_linear/gtx/extended_min_max.hpp
///
/// @see core (dependence)
///
/// @defgroup gtx_extended_min_max ASTER_LINEAR_GTX_extented_min_max
/// @ingroup gtx
///
/// Include <aster_linear/gtx/extented_min_max.hpp> to use the features of this extension.
///
/// Min and max functions for 3 to 4 parameters.

#pragma once

// Dependency:
#include "../aster_linear.hpp"
#include "../ext/vector_common.hpp"

#if ASTER_LINEAR_MESSAGES == ASTER_LINEAR_ENABLE && !defined(ASTER_LINEAR_EXT_INCLUDED)
#	ifndef ASTER_LINEAR_ENABLE_EXPERIMENTAL
#		pragma message("ASTER_LINEAR: ASTER_LINEAR_GTX_extented_min_max is an experimental extension and may change in the future. Use #define ASTER_LINEAR_ENABLE_EXPERIMENTAL before including it, if you really want to use it.")
#	else
#		pragma message("ASTER_LINEAR: ASTER_LINEAR_GTX_extented_min_max extension included")
#	endif
#endif

namespace aster_linear
{
	/// @addtogroup gtx_extended_min_max
	/// @{

	/// Return the minimum component-wise values of 3 inputs
	/// @see gtx_extented_min_max
	template<typename T>
	ASTER_LINEAR_FUNC_DECL T min(
		T const& x,
		T const& y,
		T const& z);

	/// Return the minimum component-wise values of 3 inputs
	/// @see gtx_extented_min_max
	template<typename T, template<typename> class C>
	ASTER_LINEAR_FUNC_DECL C<T> min(
		C<T> const& x,
		typename C<T>::T const& y,
		typename C<T>::T const& z);

	/// Return the minimum component-wise values of 3 inputs
	/// @see gtx_extented_min_max
	template<typename T, template<typename> class C>
	ASTER_LINEAR_FUNC_DECL C<T> min(
		C<T> const& x,
		C<T> const& y,
		C<T> const& z);

	/// Return the minimum component-wise values of 4 inputs
	/// @see gtx_extented_min_max
	template<typename T>
	ASTER_LINEAR_FUNC_DECL T min(
		T const& x,
		T const& y,
		T const& z,
		T const& w);

	/// Return the minimum component-wise values of 4 inputs
	/// @see gtx_extented_min_max
	template<typename T, template<typename> class C>
	ASTER_LINEAR_FUNC_DECL C<T> min(
		C<T> const& x,
		typename C<T>::T const& y,
		typename C<T>::T const& z,
		typename C<T>::T const& w);

	/// Return the minimum component-wise values of 4 inputs
	/// @see gtx_extented_min_max
	template<typename T, template<typename> class C>
	ASTER_LINEAR_FUNC_DECL C<T> min(
		C<T> const& x,
		C<T> const& y,
		C<T> const& z,
		C<T> const& w);

	/// Return the maximum component-wise values of 3 inputs
	/// @see gtx_extented_min_max
	template<typename T>
	ASTER_LINEAR_FUNC_DECL T max(
		T const& x,
		T const& y,
		T const& z);

	/// Return the maximum component-wise values of 3 inputs
	/// @see gtx_extented_min_max
	template<typename T, template<typename> class C>
	ASTER_LINEAR_FUNC_DECL C<T> max(
		C<T> const& x,
		typename C<T>::T const& y,
		typename C<T>::T const& z);

	/// Return the maximum component-wise values of 3 inputs
	/// @see gtx_extented_min_max
	template<typename T, template<typename> class C>
	ASTER_LINEAR_FUNC_DECL C<T> max(
		C<T> const& x,
		C<T> const& y,
		C<T> const& z);

	/// Return the maximum component-wise values of 4 inputs
	/// @see gtx_extented_min_max
	template<typename T>
	ASTER_LINEAR_FUNC_DECL T max(
		T const& x,
		T const& y,
		T const& z,
		T const& w);

	/// Return the maximum component-wise values of 4 inputs
	/// @see gtx_extented_min_max
	template<typename T, template<typename> class C>
	ASTER_LINEAR_FUNC_DECL C<T> max(
		C<T> const& x,
		typename C<T>::T const& y,
		typename C<T>::T const& z,
		typename C<T>::T const& w);

	/// Return the maximum component-wise values of 4 inputs
	/// @see gtx_extented_min_max
	template<typename T, template<typename> class C>
	ASTER_LINEAR_FUNC_DECL C<T> max(
		C<T> const& x,
		C<T> const& y,
		C<T> const& z,
		C<T> const& w);

	/// @}
}//namespace aster_linear

#include "extended_min_max.inl"
