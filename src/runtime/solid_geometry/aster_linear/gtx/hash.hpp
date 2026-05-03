/// @ref gtx_hash
/// @file aster_linear/gtx/hash.hpp
///
/// @see core (dependence)
///
/// @defgroup gtx_hash ASTER_LINEAR_GTX_hash
/// @ingroup gtx
///
/// Include <aster_linear/gtx/hash.hpp> to use the features of this extension.
///
/// Add std::hash support for aster_linear types

#pragma once

#if ASTER_LINEAR_MESSAGES == ASTER_LINEAR_ENABLE && !defined(ASTER_LINEAR_EXT_INCLUDED)
#	ifndef ASTER_LINEAR_ENABLE_EXPERIMENTAL
#		pragma message("ASTER_LINEAR: ASTER_LINEAR_GTX_hash is an experimental extension and may change in the future. Use #define ASTER_LINEAR_ENABLE_EXPERIMENTAL before including it, if you really want to use it.")
#	else
#		pragma message("ASTER_LINEAR: ASTER_LINEAR_GTX_hash extension included")
#	endif
#endif

#include <functional>

#include "../vec2.hpp"
#include "../vec3.hpp"
#include "../vec4.hpp"
#include "../gtc/vec1.hpp"

#include "../gtc/quaternion.hpp"
#include "../gtx/dual_quaternion.hpp"

#include "../mat2x2.hpp"
#include "../mat2x3.hpp"
#include "../mat2x4.hpp"

#include "../mat3x2.hpp"
#include "../mat3x3.hpp"
#include "../mat3x4.hpp"

#include "../mat4x2.hpp"
#include "../mat4x3.hpp"
#include "../mat4x4.hpp"

#if !ASTER_LINEAR_HAS_CXX11_STL
#	error "ASTER_LINEAR_GTX_hash requires C++11 standard library support"
#endif

namespace std
{
	template<typename T, aster_linear::qualifier Q>
	struct hash<aster_linear::vec<1, T,Q> >
	{
		ASTER_LINEAR_FUNC_DECL size_t operator()(aster_linear::vec<1, T, Q> const& v) const;
	};

	template<typename T, aster_linear::qualifier Q>
	struct hash<aster_linear::vec<2, T,Q> >
	{
		ASTER_LINEAR_FUNC_DECL size_t operator()(aster_linear::vec<2, T, Q> const& v) const;
	};

	template<typename T, aster_linear::qualifier Q>
	struct hash<aster_linear::vec<3, T,Q> >
	{
		ASTER_LINEAR_FUNC_DECL size_t operator()(aster_linear::vec<3, T, Q> const& v) const;
	};

	template<typename T, aster_linear::qualifier Q>
	struct hash<aster_linear::vec<4, T,Q> >
	{
		ASTER_LINEAR_FUNC_DECL size_t operator()(aster_linear::vec<4, T, Q> const& v) const;
	};

	template<typename T, aster_linear::qualifier Q>
	struct hash<aster_linear::qua<T,Q>>
	{
		ASTER_LINEAR_FUNC_DECL size_t operator()(aster_linear::qua<T, Q> const& q) const;
	};

	template<typename T, aster_linear::qualifier Q>
	struct hash<aster_linear::tdualquat<T,Q> >
	{
		ASTER_LINEAR_FUNC_DECL size_t operator()(aster_linear::tdualquat<T,Q> const& q) const;
	};

	template<typename T, aster_linear::qualifier Q>
	struct hash<aster_linear::mat<2, 2, T,Q> >
	{
		ASTER_LINEAR_FUNC_DECL size_t operator()(aster_linear::mat<2, 2, T,Q> const& m) const;
	};

	template<typename T, aster_linear::qualifier Q>
	struct hash<aster_linear::mat<2, 3, T,Q> >
	{
		ASTER_LINEAR_FUNC_DECL size_t operator()(aster_linear::mat<2, 3, T,Q> const& m) const;
	};

	template<typename T, aster_linear::qualifier Q>
	struct hash<aster_linear::mat<2, 4, T,Q> >
	{
		ASTER_LINEAR_FUNC_DECL size_t operator()(aster_linear::mat<2, 4, T,Q> const& m) const;
	};

	template<typename T, aster_linear::qualifier Q>
	struct hash<aster_linear::mat<3, 2, T,Q> >
	{
		ASTER_LINEAR_FUNC_DECL size_t operator()(aster_linear::mat<3, 2, T,Q> const& m) const;
	};

	template<typename T, aster_linear::qualifier Q>
	struct hash<aster_linear::mat<3, 3, T,Q> >
	{
		ASTER_LINEAR_FUNC_DECL size_t operator()(aster_linear::mat<3, 3, T,Q> const& m) const;
	};

	template<typename T, aster_linear::qualifier Q>
	struct hash<aster_linear::mat<3, 4, T,Q> >
	{
		ASTER_LINEAR_FUNC_DECL size_t operator()(aster_linear::mat<3, 4, T,Q> const& m) const;
	};

	template<typename T, aster_linear::qualifier Q>
	struct hash<aster_linear::mat<4, 2, T,Q> >
	{
		ASTER_LINEAR_FUNC_DECL size_t operator()(aster_linear::mat<4, 2, T,Q> const& m) const;
	};

	template<typename T, aster_linear::qualifier Q>
	struct hash<aster_linear::mat<4, 3, T,Q> >
	{
		ASTER_LINEAR_FUNC_DECL size_t operator()(aster_linear::mat<4, 3, T,Q> const& m) const;
	};

	template<typename T, aster_linear::qualifier Q>
	struct hash<aster_linear::mat<4, 4, T,Q> >
	{
		ASTER_LINEAR_FUNC_DECL size_t operator()(aster_linear::mat<4, 4, T,Q> const& m) const;
	};
} // namespace std

#include "hash.inl"
