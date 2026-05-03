/// @ref core
/// @file aster_linear/detail/type_vec4.hpp

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
	struct vec<4, T, Q>
	{
		// -- Implementation detail --

		typedef T value_type;
		typedef vec<4, T, Q> type;
		typedef vec<4, bool, Q> bool_type;

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
			T x, y, z, w;
#			if ASTER_LINEAR_CONFIG_SWIZZLE == ASTER_LINEAR_SWIZZLE_FUNCTION
			ASTER_LINEAR_SWIZZLE_GEN_VEC_FROM_VEC4_COMP(T, Q, x, y, z, w)
#			endif//ASTER_LINEAR_CONFIG_SWIZZLE
#		elif ASTER_LINEAR_CONFIG_ANONYMOUS_STRUCT == ASTER_LINEAR_ENABLE
			union
			{
				struct { T x, y, z, w; };
				struct { T r, g, b, a; };
				struct { T s, t, p, q; };

				typename detail::storage<4, T, detail::is_aligned<Q>::value>::type data;

#				if ASTER_LINEAR_CONFIG_SWIZZLE == ASTER_LINEAR_SWIZZLE_OPERATOR
					ASTER_LINEAR_SWIZZLE4_2_MEMBERS(T, Q, x, y, z, w)
					ASTER_LINEAR_SWIZZLE4_2_MEMBERS(T, Q, r, g, b, a)
					ASTER_LINEAR_SWIZZLE4_2_MEMBERS(T, Q, s, t, p, q)
					ASTER_LINEAR_SWIZZLE4_3_MEMBERS(T, Q, x, y, z, w)
					ASTER_LINEAR_SWIZZLE4_3_MEMBERS(T, Q, r, g, b, a)
					ASTER_LINEAR_SWIZZLE4_3_MEMBERS(T, Q, s, t, p, q)
					ASTER_LINEAR_SWIZZLE4_4_MEMBERS(T, Q, x, y, z, w)
					ASTER_LINEAR_SWIZZLE4_4_MEMBERS(T, Q, r, g, b, a)
					ASTER_LINEAR_SWIZZLE4_4_MEMBERS(T, Q, s, t, p, q)
#				endif
			};
#		else
			union { T x, r, s; };
			union { T y, g, t; };
			union { T z, b, p; };
			union { T w, a, q; };

#			if ASTER_LINEAR_CONFIG_SWIZZLE == ASTER_LINEAR_SWIZZLE_FUNCTION
				ASTER_LINEAR_SWIZZLE_GEN_VEC_FROM_VEC4(T, Q)
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

		/// Return the count of components of the vector
		ASTER_LINEAR_FUNC_DECL static ASTER_LINEAR_CONSTEXPR length_type length(){return 4;}

		ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR T & operator[](length_type i);
		ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR T const& operator[](length_type i) const;

		// -- Implicit basic constructors --

		ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec() ASTER_LINEAR_DEFAULT_CTOR;
		ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec(vec<4, T, Q> const& v) ASTER_LINEAR_DEFAULT;
		template<qualifier P>
		ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec(vec<4, T, P> const& v);

		// -- Explicit basic constructors --

		ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR explicit vec(T scalar);
		ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec(T x, T y, T z, T w);

		// -- Conversion scalar constructors --

		template<typename U, qualifier P>
		ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR explicit vec(vec<1, U, P> const& v);

		/// Explicit conversions (From section 5.4.1 Conversion and scalar constructors of GLSL 1.30.08 specification)
		template<typename X, typename Y, typename Z, typename W>
		ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec(X _x, Y _y, Z _z, W _w);
		template<typename X, typename Y, typename Z, typename W>
		ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec(vec<1, X, Q> const& _x, Y _y, Z _z, W _w);
		template<typename X, typename Y, typename Z, typename W>
		ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec(X _x, vec<1, Y, Q> const& _y, Z _z, W _w);
		template<typename X, typename Y, typename Z, typename W>
		ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec(vec<1, X, Q> const& _x, vec<1, Y, Q> const& _y, Z _z, W _w);
		template<typename X, typename Y, typename Z, typename W>
		ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec(X _x, Y _y, vec<1, Z, Q> const& _z, W _w);
		template<typename X, typename Y, typename Z, typename W>
		ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec(vec<1, X, Q> const& _x, Y _y, vec<1, Z, Q> const& _z, W _w);
		template<typename X, typename Y, typename Z, typename W>
		ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec(X _x, vec<1, Y, Q> const& _y, vec<1, Z, Q> const& _z, W _w);
		template<typename X, typename Y, typename Z, typename W>
		ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec(vec<1, X, Q> const& _x, vec<1, Y, Q> const& _y, vec<1, Z, Q> const& _z, W _w);
		template<typename X, typename Y, typename Z, typename W>
		ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec(vec<1, X, Q> const& _x, Y _y, Z _z, vec<1, W, Q> const& _w);
		template<typename X, typename Y, typename Z, typename W>
		ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec(X _x, vec<1, Y, Q> const& _y, Z _z, vec<1, W, Q> const& _w);
		template<typename X, typename Y, typename Z, typename W>
		ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec(vec<1, X, Q> const& _x, vec<1, Y, Q> const& _y, Z _z, vec<1, W, Q> const& _w);
		template<typename X, typename Y, typename Z, typename W>
		ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec(X _x, Y _y, vec<1, Z, Q> const& _z, vec<1, W, Q> const& _w);
		template<typename X, typename Y, typename Z, typename W>
		ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec(vec<1, X, Q> const& _x, Y _y, vec<1, Z, Q> const& _z, vec<1, W, Q> const& _w);
		template<typename X, typename Y, typename Z, typename W>
		ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec(X _x, vec<1, Y, Q> const& _y, vec<1, Z, Q> const& _z, vec<1, W, Q> const& _w);
		template<typename X, typename Y, typename Z, typename W>
		ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec(vec<1, X, Q> const& _x, vec<1, Y, Q> const& _Y, vec<1, Z, Q> const& _z, vec<1, W, Q> const& _w);

		// -- Conversion vector constructors --

		/// Explicit conversions (From section 5.4.1 Conversion and scalar constructors of GLSL 1.30.08 specification)
		template<typename A, typename B, typename C, qualifier P>
		ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec(vec<2, A, P> const& _xy, B _z, C _w);
		/// Explicit conversions (From section 5.4.1 Conversion and scalar constructors of GLSL 1.30.08 specification)
		template<typename A, typename B, typename C, qualifier P>
		ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec(vec<2, A, P> const& _xy, vec<1, B, P> const& _z, C _w);
		/// Explicit conversions (From section 5.4.1 Conversion and scalar constructors of GLSL 1.30.08 specification)
		template<typename A, typename B, typename C, qualifier P>
		ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec(vec<2, A, P> const& _xy, B _z, vec<1, C, P> const& _w);
		/// Explicit conversions (From section 5.4.1 Conversion and scalar constructors of GLSL 1.30.08 specification)
		template<typename A, typename B, typename C, qualifier P>
		ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec(vec<2, A, P> const& _xy, vec<1, B, P> const& _z, vec<1, C, P> const& _w);
		/// Explicit conversions (From section 5.4.1 Conversion and scalar constructors of GLSL 1.30.08 specification)
		template<typename A, typename B, typename C, qualifier P>
		ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec(A _x, vec<2, B, P> const& _yz, C _w);
		/// Explicit conversions (From section 5.4.1 Conversion and scalar constructors of GLSL 1.30.08 specification)
		template<typename A, typename B, typename C, qualifier P>
		ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec(vec<1, A, P> const& _x, vec<2, B, P> const& _yz, C _w);
		/// Explicit conversions (From section 5.4.1 Conversion and scalar constructors of GLSL 1.30.08 specification)
		template<typename A, typename B, typename C, qualifier P>
		ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec(A _x, vec<2, B, P> const& _yz, vec<1, C, P> const& _w);
		/// Explicit conversions (From section 5.4.1 Conversion and scalar constructors of GLSL 1.30.08 specification)
		template<typename A, typename B, typename C, qualifier P>
		ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec(vec<1, A, P> const& _x, vec<2, B, P> const& _yz, vec<1, C, P> const& _w);
		/// Explicit conversions (From section 5.4.1 Conversion and scalar constructors of GLSL 1.30.08 specification)
		template<typename A, typename B, typename C, qualifier P>
		ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec(A _x, B _y, vec<2, C, P> const& _zw);
		/// Explicit conversions (From section 5.4.1 Conversion and scalar constructors of GLSL 1.30.08 specification)
		template<typename A, typename B, typename C, qualifier P>
		ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec(vec<1, A, P> const& _x, B _y, vec<2, C, P> const& _zw);
		/// Explicit conversions (From section 5.4.1 Conversion and scalar constructors of GLSL 1.30.08 specification)
		template<typename A, typename B, typename C, qualifier P>
		ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec(A _x, vec<1, B, P> const& _y, vec<2, C, P> const& _zw);
		/// Explicit conversions (From section 5.4.1 Conversion and scalar constructors of GLSL 1.30.08 specification)
		template<typename A, typename B, typename C, qualifier P>
		ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec(vec<1, A, P> const& _x, vec<1, B, P> const& _y, vec<2, C, P> const& _zw);
		/// Explicit conversions (From section 5.4.1 Conversion and scalar constructors of GLSL 1.30.08 specification)
		template<typename A, typename B, qualifier P>
		ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec(vec<3, A, P> const& _xyz, B _w);
		/// Explicit conversions (From section 5.4.1 Conversion and scalar constructors of GLSL 1.30.08 specification)
		template<typename A, typename B, qualifier P>
		ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec(vec<3, A, P> const& _xyz, vec<1, B, P> const& _w);
		/// Explicit conversions (From section 5.4.1 Conversion and scalar constructors of GLSL 1.30.08 specification)
		template<typename A, typename B, qualifier P>
		ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec(A _x, vec<3, B, P> const& _yzw);
		/// Explicit conversions (From section 5.4.1 Conversion and scalar constructors of GLSL 1.30.08 specification)
		template<typename A, typename B, qualifier P>
		ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec(vec<1, A, P> const& _x, vec<3, B, P> const& _yzw);
		/// Explicit conversions (From section 5.4.1 Conversion and scalar constructors of GLSL 1.30.08 specification)
		template<typename A, typename B, qualifier P>
		ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec(vec<2, A, P> const& _xy, vec<2, B, P> const& _zw);

		/// Explicit conversions (From section 5.4.1 Conversion and scalar constructors of GLSL 1.30.08 specification)
		template<typename U, qualifier P>
		ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR ASTER_LINEAR_EXPLICIT vec(vec<4, U, P> const& v);

		// -- Swizzle constructors --
#		if ASTER_LINEAR_CONFIG_SWIZZLE == ASTER_LINEAR_SWIZZLE_OPERATOR
			template<int E0, int E1, int E2, int E3>
			ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec(detail::_swizzle<4, T, Q, E0, E1, E2, E3> const& that)
			{
				*this = that();
			}

			template<int E0, int E1, int F0, int F1>
			ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec(detail::_swizzle<2, T, Q, E0, E1, -1, -2> const& v, detail::_swizzle<2, T, Q, F0, F1, -1, -2> const& u)
			{
				*this = vec<4, T, Q>(v(), u());
			}

			template<int E0, int E1>
			ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec(T const& x, T const& y, detail::_swizzle<2, T, Q, E0, E1, -1, -2> const& v)
			{
				*this = vec<4, T, Q>(x, y, v());
			}

			template<int E0, int E1>
			ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec(T const& x, detail::_swizzle<2, T, Q, E0, E1, -1, -2> const& v, T const& w)
			{
				*this = vec<4, T, Q>(x, v(), w);
			}

			template<int E0, int E1>
			ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec(detail::_swizzle<2, T, Q, E0, E1, -1, -2> const& v, T const& z, T const& w)
			{
				*this = vec<4, T, Q>(v(), z, w);
			}

			template<int E0, int E1, int E2>
			ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec(detail::_swizzle<3, T, Q, E0, E1, E2, -1> const& v, T const& w)
			{
				*this = vec<4, T, Q>(v(), w);
			}

			template<int E0, int E1, int E2>
			ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec(T const& x, detail::_swizzle<3, T, Q, E0, E1, E2, -1> const& v)
			{
				*this = vec<4, T, Q>(x, v());
			}
#		endif//ASTER_LINEAR_CONFIG_SWIZZLE == ASTER_LINEAR_SWIZZLE_OPERATOR

		// -- Unary arithmetic operators --

		ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<4, T, Q>& operator=(vec<4, T, Q> const& v) ASTER_LINEAR_DEFAULT;

		template<typename U>
		ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<4, T, Q>& operator=(vec<4, U, Q> const& v);
		template<typename U>
		ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<4, T, Q>& operator+=(U scalar);
		template<typename U>
		ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<4, T, Q>& operator+=(vec<1, U, Q> const& v);
		template<typename U>
		ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<4, T, Q>& operator+=(vec<4, U, Q> const& v);
		template<typename U>
		ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<4, T, Q>& operator-=(U scalar);
		template<typename U>
		ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<4, T, Q>& operator-=(vec<1, U, Q> const& v);
		template<typename U>
		ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<4, T, Q>& operator-=(vec<4, U, Q> const& v);
		template<typename U>
		ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<4, T, Q>& operator*=(U scalar);
		template<typename U>
		ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<4, T, Q>& operator*=(vec<1, U, Q> const& v);
		template<typename U>
		ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<4, T, Q>& operator*=(vec<4, U, Q> const& v);
		template<typename U>
		ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<4, T, Q>& operator/=(U scalar);
		template<typename U>
		ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<4, T, Q>& operator/=(vec<1, U, Q> const& v);
		template<typename U>
		ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<4, T, Q>& operator/=(vec<4, U, Q> const& v);

		// -- Increment and decrement operators --

		ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<4, T, Q> & operator++();
		ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<4, T, Q> & operator--();
		ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<4, T, Q> operator++(int);
		ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<4, T, Q> operator--(int);

		// -- Unary bit operators --

		template<typename U>
		ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<4, T, Q> & operator%=(U scalar);
		template<typename U>
		ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<4, T, Q> & operator%=(vec<1, U, Q> const& v);
		template<typename U>
		ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<4, T, Q> & operator%=(vec<4, U, Q> const& v);
		template<typename U>
		ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<4, T, Q> & operator&=(U scalar);
		template<typename U>
		ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<4, T, Q> & operator&=(vec<1, U, Q> const& v);
		template<typename U>
		ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<4, T, Q> & operator&=(vec<4, U, Q> const& v);
		template<typename U>
		ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<4, T, Q> & operator|=(U scalar);
		template<typename U>
		ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<4, T, Q> & operator|=(vec<1, U, Q> const& v);
		template<typename U>
		ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<4, T, Q> & operator|=(vec<4, U, Q> const& v);
		template<typename U>
		ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<4, T, Q> & operator^=(U scalar);
		template<typename U>
		ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<4, T, Q> & operator^=(vec<1, U, Q> const& v);
		template<typename U>
		ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<4, T, Q> & operator^=(vec<4, U, Q> const& v);
		template<typename U>
		ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<4, T, Q> & operator<<=(U scalar);
		template<typename U>
		ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<4, T, Q> & operator<<=(vec<1, U, Q> const& v);
		template<typename U>
		ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<4, T, Q> & operator<<=(vec<4, U, Q> const& v);
		template<typename U>
		ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<4, T, Q> & operator>>=(U scalar);
		template<typename U>
		ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<4, T, Q> & operator>>=(vec<1, U, Q> const& v);
		template<typename U>
		ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<4, T, Q> & operator>>=(vec<4, U, Q> const& v);
	};

	// -- Unary operators --

	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<4, T, Q> operator+(vec<4, T, Q> const& v);

	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<4, T, Q> operator-(vec<4, T, Q> const& v);

	// -- Binary operators --

	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<4, T, Q> operator+(vec<4, T, Q> const& v, T const & scalar);

	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<4, T, Q> operator+(vec<4, T, Q> const& v1, vec<1, T, Q> const& v2);

	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<4, T, Q> operator+(T scalar, vec<4, T, Q> const& v);

	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<4, T, Q> operator+(vec<1, T, Q> const& v1, vec<4, T, Q> const& v2);

	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<4, T, Q> operator+(vec<4, T, Q> const& v1, vec<4, T, Q> const& v2);

	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<4, T, Q> operator-(vec<4, T, Q> const& v, T const & scalar);

	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<4, T, Q> operator-(vec<4, T, Q> const& v1, vec<1, T, Q> const& v2);

	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<4, T, Q> operator-(T scalar, vec<4, T, Q> const& v);

	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<4, T, Q> operator-(vec<1, T, Q> const& v1, vec<4, T, Q> const& v2);

	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<4, T, Q> operator-(vec<4, T, Q> const& v1, vec<4, T, Q> const& v2);

	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<4, T, Q> operator*(vec<4, T, Q> const& v, T const & scalar);

	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<4, T, Q> operator*(vec<4, T, Q> const& v1, vec<1, T, Q> const& v2);

	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<4, T, Q> operator*(T scalar, vec<4, T, Q> const& v);

	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<4, T, Q> operator*(vec<1, T, Q> const& v1, vec<4, T, Q> const& v2);

	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<4, T, Q> operator*(vec<4, T, Q> const& v1, vec<4, T, Q> const& v2);

	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<4, T, Q> operator/(vec<4, T, Q> const& v, T const & scalar);

	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<4, T, Q> operator/(vec<4, T, Q> const& v1, vec<1, T, Q> const& v2);

	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<4, T, Q> operator/(T scalar, vec<4, T, Q> const& v);

	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<4, T, Q> operator/(vec<1, T, Q> const& v1, vec<4, T, Q> const& v2);

	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<4, T, Q> operator/(vec<4, T, Q> const& v1, vec<4, T, Q> const& v2);

	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<4, T, Q> operator%(vec<4, T, Q> const& v, T scalar);

	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<4, T, Q> operator%(vec<4, T, Q> const& v, vec<1, T, Q> const& scalar);

	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<4, T, Q> operator%(T scalar, vec<4, T, Q> const& v);

	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<4, T, Q> operator%(vec<1, T, Q> const& scalar, vec<4, T, Q> const& v);

	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<4, T, Q> operator%(vec<4, T, Q> const& v1, vec<4, T, Q> const& v2);

	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<4, T, Q> operator&(vec<4, T, Q> const& v, T scalar);

	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<4, T, Q> operator&(vec<4, T, Q> const& v, vec<1, T, Q> const& scalar);

	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<4, T, Q> operator&(T scalar, vec<4, T, Q> const& v);

	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<4, T, Q> operator&(vec<1, T, Q> const& scalar, vec<4, T, Q> const& v);

	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<4, T, Q> operator&(vec<4, T, Q> const& v1, vec<4, T, Q> const& v2);

	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<4, T, Q> operator|(vec<4, T, Q> const& v, T scalar);

	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<4, T, Q> operator|(vec<4, T, Q> const& v, vec<1, T, Q> const& scalar);

	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<4, T, Q> operator|(T scalar, vec<4, T, Q> const& v);

	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<4, T, Q> operator|(vec<1, T, Q> const& scalar, vec<4, T, Q> const& v);

	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<4, T, Q> operator|(vec<4, T, Q> const& v1, vec<4, T, Q> const& v2);

	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<4, T, Q> operator^(vec<4, T, Q> const& v, T scalar);

	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<4, T, Q> operator^(vec<4, T, Q> const& v, vec<1, T, Q> const& scalar);

	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<4, T, Q> operator^(T scalar, vec<4, T, Q> const& v);

	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<4, T, Q> operator^(vec<1, T, Q> const& scalar, vec<4, T, Q> const& v);

	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<4, T, Q> operator^(vec<4, T, Q> const& v1, vec<4, T, Q> const& v2);

	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<4, T, Q> operator<<(vec<4, T, Q> const& v, T scalar);

	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<4, T, Q> operator<<(vec<4, T, Q> const& v, vec<1, T, Q> const& scalar);

	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<4, T, Q> operator<<(T scalar, vec<4, T, Q> const& v);

	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<4, T, Q> operator<<(vec<1, T, Q> const& scalar, vec<4, T, Q> const& v);

	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<4, T, Q> operator<<(vec<4, T, Q> const& v1, vec<4, T, Q> const& v2);

	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<4, T, Q> operator>>(vec<4, T, Q> const& v, T scalar);

	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<4, T, Q> operator>>(vec<4, T, Q> const& v, vec<1, T, Q> const& scalar);

	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<4, T, Q> operator>>(T scalar, vec<4, T, Q> const& v);

	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<4, T, Q> operator>>(vec<1, T, Q> const& scalar, vec<4, T, Q> const& v);

	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<4, T, Q> operator>>(vec<4, T, Q> const& v1, vec<4, T, Q> const& v2);

	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<4, T, Q> operator~(vec<4, T, Q> const& v);

	// -- Boolean operators --

	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR bool operator==(vec<4, T, Q> const& v1, vec<4, T, Q> const& v2);

	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR bool operator!=(vec<4, T, Q> const& v1, vec<4, T, Q> const& v2);

	template<qualifier Q>
	ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<4, bool, Q> operator&&(vec<4, bool, Q> const& v1, vec<4, bool, Q> const& v2);

	template<qualifier Q>
	ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR vec<4, bool, Q> operator||(vec<4, bool, Q> const& v1, vec<4, bool, Q> const& v2);
}//namespace aster_linear

#ifndef ASTER_LINEAR_EXTERNAL_TEMPLATE
#include "type_vec4.inl"
#endif//ASTER_LINEAR_EXTERNAL_TEMPLATE
