/// @ref core
/// @file aster_linear/detail/type_vec1.hpp

#pragma once

#include "qualifier.hpp"
#if ASTER_LINEAR_CONFIG_SWIZZLE == ASTER_LINEAR_SWIZZLE_OPERATOR
#	include "_swizzle.hpp"
#elif ASTER_LINEAR_CONFIG_SWIZZLE == ASTER_LINEAR_SWIZZLE_FUNCTION
#	include "_swizzle_func.hpp"
#endif
#include <cstddef>

namespace aster_linear
{
	template<typename T, qualifier Q>
	struct vec<1, T, Q>
	{
		// -- Implementation detail --

		typedef T value_type;
		typedef vec<1, T, Q> type;
		typedef vec<1, bool, Q> bool_type;

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

#		if ASTER_LINEAR_CONFIG_XYZW_ONLY
			T x;
#		elif ASTER_LINEAR_CONFIG_ANONYMOUS_STRUCT == ASTER_LINEAR_ENABLE
			union
			{
				T x;
				T r;
				T s;

				typename detail::storage<1, T, detail::is_aligned<Q>::value>::type data;
/*
#				if ASTER_LINEAR_CONFIG_SWIZZLE == ASTER_LINEAR_SWIZZLE_OPERATOR
					_ASTER_LINEAR_SWIZZLE1_2_MEMBERS(T, Q, x)
					_ASTER_LINEAR_SWIZZLE1_2_MEMBERS(T, Q, r)
					_ASTER_LINEAR_SWIZZLE1_2_MEMBERS(T, Q, s)
					_ASTER_LINEAR_SWIZZLE1_3_MEMBERS(T, Q, x)
					_ASTER_LINEAR_SWIZZLE1_3_MEMBERS(T, Q, r)
					_ASTER_LINEAR_SWIZZLE1_3_MEMBERS(T, Q, s)
					_ASTER_LINEAR_SWIZZLE1_4_MEMBERS(T, Q, x)
					_ASTER_LINEAR_SWIZZLE1_4_MEMBERS(T, Q, r)
					_ASTER_LINEAR_SWIZZLE1_4_MEMBERS(T, Q, s)
#				endif
*/
			};
#		else
			union {T x, r, s;};
/*
#			if ASTER_LINEAR_CONFIG_SWIZZLE == ASTER_LINEAR_SWIZZLE_FUNCTION
				ASTER_LINEAR_SWIZZLE_GEN_VEC_FROM_VEC1(T, Q)
#			endif
*/
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

		/// Return the count of components of the vector
		typedef length_t length_type;
		ASTER_LINEAR_FUNC_DECL static ASTER_LINEAR_CONSTEXPR length_type length(){return 1;}

		ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR T & operator[](length_type i);
		ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR T const& operator[](length_type i) const;

		// -- Implicit basic constructors --

		ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec() ASTER_LINEAR_DEFAULT_CTOR;
		ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec(vec const& v) ASTER_LINEAR_DEFAULT;
		template<qualifier P>
		ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec(vec<1, T, P> const& v);

		// -- Explicit basic constructors --

		ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR explicit vec(T scalar);

		// -- Conversion vector constructors --

		/// Explicit conversions (From section 5.4.1 Conversion and scalar constructors of GLSL 1.30.08 specification)
		template<typename U, qualifier P>
		ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR ASTER_LINEAR_EXPLICIT vec(vec<2, U, P> const& v);
		/// Explicit conversions (From section 5.4.1 Conversion and scalar constructors of GLSL 1.30.08 specification)
		template<typename U, qualifier P>
		ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR ASTER_LINEAR_EXPLICIT vec(vec<3, U, P> const& v);
		/// Explicit conversions (From section 5.4.1 Conversion and scalar constructors of GLSL 1.30.08 specification)
		template<typename U, qualifier P>
		ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR ASTER_LINEAR_EXPLICIT vec(vec<4, U, P> const& v);

		/// Explicit conversions (From section 5.4.1 Conversion and scalar constructors of GLSL 1.30.08 specification)
		template<typename U, qualifier P>
		ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR ASTER_LINEAR_EXPLICIT vec(vec<1, U, P> const& v);

		// -- Swizzle constructors --
/*
#		if ASTER_LINEAR_CONFIG_SWIZZLE == ASTER_LINEAR_SWIZZLE_OPERATOR
			template<int E0>
			ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec(detail::_swizzle<1, T, Q, E0, -1,-2,-3> const& that)
			{
				*this = that();
			}
#		endif//ASTER_LINEAR_CONFIG_SWIZZLE == ASTER_LINEAR_SWIZZLE_OPERATOR
*/
		// -- Unary arithmetic operators --

		ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<1, T, Q> & operator=(vec const& v) ASTER_LINEAR_DEFAULT;

		template<typename U>
		ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<1, T, Q> & operator=(vec<1, U, Q> const& v);
		template<typename U>
		ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<1, T, Q> & operator+=(U scalar);
		template<typename U>
		ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<1, T, Q> & operator+=(vec<1, U, Q> const& v);
		template<typename U>
		ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<1, T, Q> & operator-=(U scalar);
		template<typename U>
		ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<1, T, Q> & operator-=(vec<1, U, Q> const& v);
		template<typename U>
		ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<1, T, Q> & operator*=(U scalar);
		template<typename U>
		ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<1, T, Q> & operator*=(vec<1, U, Q> const& v);
		template<typename U>
		ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<1, T, Q> & operator/=(U scalar);
		template<typename U>
		ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<1, T, Q> & operator/=(vec<1, U, Q> const& v);

		// -- Increment and decrement operators --

		ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<1, T, Q> & operator++();
		ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<1, T, Q> & operator--();
		ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<1, T, Q> operator++(int);
		ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<1, T, Q> operator--(int);

		// -- Unary bit operators --

		template<typename U>
		ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<1, T, Q> & operator%=(U scalar);
		template<typename U>
		ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<1, T, Q> & operator%=(vec<1, U, Q> const& v);
		template<typename U>
		ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<1, T, Q> & operator&=(U scalar);
		template<typename U>
		ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<1, T, Q> & operator&=(vec<1, U, Q> const& v);
		template<typename U>
		ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<1, T, Q> & operator|=(U scalar);
		template<typename U>
		ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<1, T, Q> & operator|=(vec<1, U, Q> const& v);
		template<typename U>
		ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<1, T, Q> & operator^=(U scalar);
		template<typename U>
		ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<1, T, Q> & operator^=(vec<1, U, Q> const& v);
		template<typename U>
		ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<1, T, Q> & operator<<=(U scalar);
		template<typename U>
		ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<1, T, Q> & operator<<=(vec<1, U, Q> const& v);
		template<typename U>
		ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<1, T, Q> & operator>>=(U scalar);
		template<typename U>
		ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<1, T, Q> & operator>>=(vec<1, U, Q> const& v);
	};

	// -- Unary operators --

	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<1, T, Q> operator+(vec<1, T, Q> const& v);

	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<1, T, Q> operator-(vec<1, T, Q> const& v);

	// -- Binary operators --

	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<1, T, Q> operator+(vec<1, T, Q> const& v, T scalar);

	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<1, T, Q> operator+(T scalar, vec<1, T, Q> const& v);

	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<1, T, Q> operator+(vec<1, T, Q> const& v1, vec<1, T, Q> const& v2);

	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<1, T, Q> operator-(vec<1, T, Q> const& v, T scalar);

	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<1, T, Q> operator-(T scalar, vec<1, T, Q> const& v);

	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<1, T, Q> operator-(vec<1, T, Q> const& v1, vec<1, T, Q> const& v2);

	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<1, T, Q> operator*(vec<1, T, Q> const& v, T scalar);

	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<1, T, Q> operator*(T scalar, vec<1, T, Q> const& v);

	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<1, T, Q> operator*(vec<1, T, Q> const& v1, vec<1, T, Q> const& v2);

	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<1, T, Q> operator/(vec<1, T, Q> const& v, T scalar);

	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<1, T, Q> operator/(T scalar, vec<1, T, Q> const& v);

	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<1, T, Q> operator/(vec<1, T, Q> const& v1, vec<1, T, Q> const& v2);

	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<1, T, Q> operator%(vec<1, T, Q> const& v, T scalar);

	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<1, T, Q> operator%(T scalar, vec<1, T, Q> const& v);

	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<1, T, Q> operator%(vec<1, T, Q> const& v1, vec<1, T, Q> const& v2);

	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<1, T, Q> operator&(vec<1, T, Q> const& v, T scalar);

	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<1, T, Q> operator&(T scalar, vec<1, T, Q> const& v);

	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<1, T, Q> operator&(vec<1, T, Q> const& v1, vec<1, T, Q> const& v2);

	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<1, T, Q> operator|(vec<1, T, Q> const& v, T scalar);

	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<1, T, Q> operator|(T scalar, vec<1, T, Q> const& v);

	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<1, T, Q> operator|(vec<1, T, Q> const& v1, vec<1, T, Q> const& v2);

	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<1, T, Q> operator^(vec<1, T, Q> const& v, T scalar);

	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<1, T, Q> operator^(T scalar, vec<1, T, Q> const& v);

	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<1, T, Q> operator^(vec<1, T, Q> const& v1, vec<1, T, Q> const& v2);

	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<1, T, Q> operator<<(vec<1, T, Q> const& v, T scalar);

	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<1, T, Q> operator<<(T scalar, vec<1, T, Q> const& v);

	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<1, T, Q> operator<<(vec<1, T, Q> const& v1, vec<1, T, Q> const& v2);

	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<1, T, Q> operator>>(vec<1, T, Q> const& v, T scalar);

	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<1, T, Q> operator>>(T scalar, vec<1, T, Q> const& v);

	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<1, T, Q> operator>>(vec<1, T, Q> const& v1, vec<1, T, Q> const& v2);

	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<1, T, Q> operator~(vec<1, T, Q> const& v);

	// -- Boolean operators --

	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR bool operator==(vec<1, T, Q> const& v1, vec<1, T, Q> const& v2);

	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR bool operator!=(vec<1, T, Q> const& v1, vec<1, T, Q> const& v2);

	template<qualifier Q>
	ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<1, bool, Q> operator&&(vec<1, bool, Q> const& v1, vec<1, bool, Q> const& v2);

	template<qualifier Q>
	ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<1, bool, Q> operator||(vec<1, bool, Q> const& v1, vec<1, bool, Q> const& v2);
}//namespace aster_linear

#ifndef ASTER_LINEAR_EXTERNAL_TEMPLATE
#include "type_vec1.inl"
#endif//ASTER_LINEAR_EXTERNAL_TEMPLATE
