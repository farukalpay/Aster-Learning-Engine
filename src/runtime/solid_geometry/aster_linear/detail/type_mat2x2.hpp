/// @ref core
/// @file aster_linear/detail/type_mat2x2.hpp

#pragma once

#include "type_vec2.hpp"
#include <limits>
#include <cstddef>

namespace aster_linear
{
	template<typename T, qualifier Q>
	struct mat<2, 2, T, Q>
	{
		typedef vec<2, T, Q> col_type;
		typedef vec<2, T, Q> row_type;
		typedef mat<2, 2, T, Q> type;
		typedef mat<2, 2, T, Q> transpose_type;
		typedef T value_type;

	private:
		col_type value[2];

	public:
		// -- Accesses --

		typedef length_t length_type;
		ASTER_LINEAR_FUNC_DECL static ASTER_LINEAR_CONSTEXPR length_type length() { return 2; }

		ASTER_LINEAR_FUNC_DECL col_type & operator[](length_type i);
		ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR col_type const& operator[](length_type i) const;

		// -- Constructors --

		ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR mat() ASTER_LINEAR_DEFAULT_CTOR;
		template<qualifier P>
		ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR mat(mat<2, 2, T, P> const& m);

		ASTER_LINEAR_FUNC_DECL explicit ASTER_LINEAR_CONSTEXPR mat(T scalar);
		ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR mat(
			T const& x1, T const& y1,
			T const& x2, T const& y2);
		ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR mat(
			col_type const& v1,
			col_type const& v2);

		// -- Conversions --

		template<typename U, typename V, typename M, typename N>
		ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR mat(
			U const& x1, V const& y1,
			M const& x2, N const& y2);

		template<typename U, typename V>
		ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_CONSTEXPR mat(
			vec<2, U, Q> const& v1,
			vec<2, V, Q> const& v2);

		// -- Matrix conversions --

		template<typename U, qualifier P>
		ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_EXPLICIT ASTER_LINEAR_CONSTEXPR mat(mat<2, 2, U, P> const& m);

		ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_EXPLICIT ASTER_LINEAR_CONSTEXPR mat(mat<3, 3, T, Q> const& x);
		ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_EXPLICIT ASTER_LINEAR_CONSTEXPR mat(mat<4, 4, T, Q> const& x);
		ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_EXPLICIT ASTER_LINEAR_CONSTEXPR mat(mat<2, 3, T, Q> const& x);
		ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_EXPLICIT ASTER_LINEAR_CONSTEXPR mat(mat<3, 2, T, Q> const& x);
		ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_EXPLICIT ASTER_LINEAR_CONSTEXPR mat(mat<2, 4, T, Q> const& x);
		ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_EXPLICIT ASTER_LINEAR_CONSTEXPR mat(mat<4, 2, T, Q> const& x);
		ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_EXPLICIT ASTER_LINEAR_CONSTEXPR mat(mat<3, 4, T, Q> const& x);
		ASTER_LINEAR_FUNC_DECL ASTER_LINEAR_EXPLICIT ASTER_LINEAR_CONSTEXPR mat(mat<4, 3, T, Q> const& x);

		// -- Unary arithmetic operators --

		template<typename U>
		ASTER_LINEAR_FUNC_DECL mat<2, 2, T, Q> & operator=(mat<2, 2, U, Q> const& m);
		template<typename U>
		ASTER_LINEAR_FUNC_DECL mat<2, 2, T, Q> & operator+=(U s);
		template<typename U>
		ASTER_LINEAR_FUNC_DECL mat<2, 2, T, Q> & operator+=(mat<2, 2, U, Q> const& m);
		template<typename U>
		ASTER_LINEAR_FUNC_DECL mat<2, 2, T, Q> & operator-=(U s);
		template<typename U>
		ASTER_LINEAR_FUNC_DECL mat<2, 2, T, Q> & operator-=(mat<2, 2, U, Q> const& m);
		template<typename U>
		ASTER_LINEAR_FUNC_DECL mat<2, 2, T, Q> & operator*=(U s);
		template<typename U>
		ASTER_LINEAR_FUNC_DECL mat<2, 2, T, Q> & operator*=(mat<2, 2, U, Q> const& m);
		template<typename U>
		ASTER_LINEAR_FUNC_DECL mat<2, 2, T, Q> & operator/=(U s);
		template<typename U>
		ASTER_LINEAR_FUNC_DECL mat<2, 2, T, Q> & operator/=(mat<2, 2, U, Q> const& m);

		// -- Increment and decrement operators --

		ASTER_LINEAR_FUNC_DECL mat<2, 2, T, Q> & operator++ ();
		ASTER_LINEAR_FUNC_DECL mat<2, 2, T, Q> & operator-- ();
		ASTER_LINEAR_FUNC_DECL mat<2, 2, T, Q> operator++(int);
		ASTER_LINEAR_FUNC_DECL mat<2, 2, T, Q> operator--(int);
	};

	// -- Unary operators --

	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL mat<2, 2, T, Q> operator+(mat<2, 2, T, Q> const& m);

	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL mat<2, 2, T, Q> operator-(mat<2, 2, T, Q> const& m);

	// -- Binary operators --

	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL mat<2, 2, T, Q> operator+(mat<2, 2, T, Q> const& m, T scalar);

	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL mat<2, 2, T, Q> operator+(T scalar, mat<2, 2, T, Q> const& m);

	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL mat<2, 2, T, Q> operator+(mat<2, 2, T, Q> const& m1, mat<2, 2, T, Q> const& m2);

	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL mat<2, 2, T, Q> operator-(mat<2, 2, T, Q> const& m, T scalar);

	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL mat<2, 2, T, Q> operator-(T scalar, mat<2, 2, T, Q> const& m);

	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL mat<2, 2, T, Q> operator-(mat<2, 2, T, Q> const& m1, mat<2, 2, T, Q> const& m2);

	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL mat<2, 2, T, Q> operator*(mat<2, 2, T, Q> const& m, T scalar);

	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL mat<2, 2, T, Q> operator*(T scalar, mat<2, 2, T, Q> const& m);

	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL typename mat<2, 2, T, Q>::col_type operator*(mat<2, 2, T, Q> const& m, typename mat<2, 2, T, Q>::row_type const& v);

	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL typename mat<2, 2, T, Q>::row_type operator*(typename mat<2, 2, T, Q>::col_type const& v, mat<2, 2, T, Q> const& m);

	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL mat<2, 2, T, Q> operator*(mat<2, 2, T, Q> const& m1, mat<2, 2, T, Q> const& m2);

	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL mat<3, 2, T, Q> operator*(mat<2, 2, T, Q> const& m1, mat<3, 2, T, Q> const& m2);

	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL mat<4, 2, T, Q> operator*(mat<2, 2, T, Q> const& m1, mat<4, 2, T, Q> const& m2);

	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL mat<2, 2, T, Q> operator/(mat<2, 2, T, Q> const& m, T scalar);

	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL mat<2, 2, T, Q> operator/(T scalar, mat<2, 2, T, Q> const& m);

	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL typename mat<2, 2, T, Q>::col_type operator/(mat<2, 2, T, Q> const& m, typename mat<2, 2, T, Q>::row_type const& v);

	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL typename mat<2, 2, T, Q>::row_type operator/(typename mat<2, 2, T, Q>::col_type const& v, mat<2, 2, T, Q> const& m);

	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL mat<2, 2, T, Q> operator/(mat<2, 2, T, Q> const& m1, mat<2, 2, T, Q> const& m2);

	// -- Boolean operators --

	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL bool operator==(mat<2, 2, T, Q> const& m1, mat<2, 2, T, Q> const& m2);

	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_DECL bool operator!=(mat<2, 2, T, Q> const& m1, mat<2, 2, T, Q> const& m2);
} //namespace aster_linear

#ifndef ASTER_LINEAR_EXTERNAL_TEMPLATE
#include "type_mat2x2.inl"
#endif
