/// @ref core
/// @file aster_linear/detail/type_quat.hpp

#pragma once

// Dependency:
#include "../detail/type_mat3x3.hpp"
#include "../detail/type_mat4x4.hpp"
#include "../detail/type_vec3.hpp"
#include "../detail/type_vec4.hpp"
#include "../ext/vector_relational.hpp"
#include "../ext/quaternion_relational.hpp"
#include "../gtc/constants.hpp"
#include "../gtc/matrix_transform.hpp"

namespace aster_linear
{
	template<typename T, qualifier Q>
	struct qua
	{
		// -- Implementation detail --

		typedef qua<T, Q> type;
		typedef T value_type;

		// -- Data --

#		if ASTER_LINEAR_SILENT_WARNINGS == ASTER_LINEAR_ENABLE
#			if ASTER_LINEAR_COMPILER & ASTER_LINEAR_COMPILER_GCC
#				pragma GCC diagnostic push
#				pragma GCC diagnostic ignored "-Wpedantic"
#			elif ASTER_LINEAR_COMPILER & ASTER_LINEAR_COMPILER_CLANG
#				pragma clang diagnostic push
#				pragma clang diagnostic ignored "-Wgnu-anonymous-struct"
#				pragma clang diagnostic ignored "-Wnested-anon-types"
#			elif ASTER_LINEAR_COMPILER & ASTER_LINEAR_COMPILER_VC
#				pragma warning(push)
#				pragma warning(disable: 4201)  // nonstandard extension used : nameless struct/union
#			endif
#		endif

#		if ASTER_LINEAR_LANG & ASTER_LINEAR_LANG_CXXMS_FLAG
			union
			{
#				ifdef ASTER_LINEAR_FORCE_QUAT_DATA_XYZW
					struct { T x, y, z, w; };
#				else
					struct { T w, x, y, z; };
#				endif

				typename detail::storage<4, T, detail::is_aligned<Q>::value>::type data;
			};
#		else
#			ifdef ASTER_LINEAR_FORCE_QUAT_DATA_XYZW
				T x, y, z, w;
#			else
				T w, x, y, z;
#			endif
#		endif

#		if ASTER_LINEAR_SILENT_WARNINGS == ASTER_LINEAR_ENABLE
#			if ASTER_LINEAR_COMPILER & ASTER_LINEAR_COMPILER_CLANG
#				pragma clang diagnostic pop
#			elif ASTER_LINEAR_COMPILER & ASTER_LINEAR_COMPILER_GCC
#				pragma GCC diagnostic pop
#			elif ASTER_LINEAR_COMPILER & ASTER_LINEAR_COMPILER_VC
#				pragma warning(pop)
#			endif
#		endif

		// -- Component accesses --

		typedef length_t length_type;

		/// Return the count of components of a quaternion
		ASTER_LINEAR_FUNC_DECL static ASTER_LINEAR_CONSTEXPR length_type length(){return 4;}

		ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR T & operator[](length_type i);
		ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR T const& operator[](length_type i) const;

		// -- Implicit basic constructors --

		ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR qua() ASTER_LINEAR_DEFAULT_CTOR;
		ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR qua(qua<T, Q> const& q) ASTER_LINEAR_DEFAULT;
		template<qualifier P>
		ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR qua(qua<T, P> const& q);

		// -- Explicit basic constructors --

		ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR qua(T s, vec<3, T, Q> const& v);

#		ifdef ASTER_LINEAR_FORCE_QUAT_DATA_XYZW
		ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR qua(T x, T y, T z, T w);
#		else
		ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR qua(T w, T x, T y, T z);
#		endif

		// -- Conversion constructors --

		template<typename U, qualifier P>
		ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR ASTER_LINEAR_EXPLICIT qua(qua<U, P> const& q);

		/// Explicit conversion operators
#		if ASTER_LINEAR_HAS_EXPLICIT_CONVERSION_OPERATORS
			ASTER_LINEAR_FUNC_DECL explicit operator mat<3, 3, T, Q>() const;
			ASTER_LINEAR_FUNC_DECL explicit operator mat<4, 4, T, Q>() const;
#		endif

		/// Create a quaternion from two normalized axis
		///
		/// @param u A first normalized axis
		/// @param v A second normalized axis
		/// @see gtc_quaternion
		/// @see http://lolengine.net/blog/2013/09/18/beautiful-maths-quaternion-from-vectors
		ASTER_LINEAR_FUNC_DECL qua(vec<3, T, Q> const& u, vec<3, T, Q> const& v);

		/// Build a quaternion from euler angles (pitch, yaw, roll), in radians.
		ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR ASTER_LINEAR_EXPLICIT qua(vec<3, T, Q> const& eulerAngles);
		ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_EXPLICIT qua(mat<3, 3, T, Q> const& q);
		ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_EXPLICIT qua(mat<4, 4, T, Q> const& q);

		// -- Unary arithmetic operators --

		ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR qua<T, Q>& operator=(qua<T, Q> const& q) ASTER_LINEAR_DEFAULT;

		template<typename U>
		ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR qua<T, Q>& operator=(qua<U, Q> const& q);
		template<typename U>
		ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR qua<T, Q>& operator+=(qua<U, Q> const& q);
		template<typename U>
		ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR qua<T, Q>& operator-=(qua<U, Q> const& q);
		template<typename U>
		ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR qua<T, Q>& operator*=(qua<U, Q> const& q);
		template<typename U>
		ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR qua<T, Q>& operator*=(U s);
		template<typename U>
		ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR qua<T, Q>& operator/=(U s);
	};

	// -- Unary bit operators --

	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR qua<T, Q> operator+(qua<T, Q> const& q);

	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR qua<T, Q> operator-(qua<T, Q> const& q);

	// -- Binary operators --

	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR qua<T, Q> operator+(qua<T, Q> const& q, qua<T, Q> const& p);

	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR qua<T, Q> operator-(qua<T, Q> const& q, qua<T, Q> const& p);

	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR qua<T, Q> operator*(qua<T, Q> const& q, qua<T, Q> const& p);

	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<3, T, Q> operator*(qua<T, Q> const& q, vec<3, T, Q> const& v);

	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<3, T, Q> operator*(vec<3, T, Q> const& v, qua<T, Q> const& q);

	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<4, T, Q> operator*(qua<T, Q> const& q, vec<4, T, Q> const& v);

	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<4, T, Q> operator*(vec<4, T, Q> const& v, qua<T, Q> const& q);

	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR qua<T, Q> operator*(qua<T, Q> const& q, T const& s);

	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR qua<T, Q> operator*(T const& s, qua<T, Q> const& q);

	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR qua<T, Q> operator/(qua<T, Q> const& q, T const& s);

	// -- Boolean operators --

	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR bool operator==(qua<T, Q> const& q1, qua<T, Q> const& q2);

	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR bool operator!=(qua<T, Q> const& q1, qua<T, Q> const& q2);
} //namespace aster_linear

#ifndef ASTER_LINEAR_EXTERNAL_TEMPLATE
#include "type_quat.inl"
#endif//ASTER_LINEAR_EXTERNAL_TEMPLATE
