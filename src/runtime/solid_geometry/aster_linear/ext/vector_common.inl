#include "../detail/_vectorize.hpp"

namespace aster_linear
{
	template<length_t L, typename T, qualifier Q>
	ASTER_LINEAR_FUNC_QUALIFIER ASTER_LINEAR_CONSTEXPR vec<L, T, Q> min(vec<L, T, Q> const& x, vec<L, T, Q> const& y, vec<L, T, Q> const& z)
	{
		ASTER_LINEAR_STATIC_ASSERT(std::numeric_limits<T>::is_iec559 || std::numeric_limits<T>::is_integer, "'min' only accept floating-point or integer inputs");
		return aster_linear::min(aster_linear::min(x, y), z);
	}

	template<length_t L, typename T, qualifier Q>
	ASTER_LINEAR_FUNC_QUALIFIER ASTER_LINEAR_CONSTEXPR vec<L, T, Q> min(vec<L, T, Q> const& x, vec<L, T, Q> const& y, vec<L, T, Q> const& z, vec<L, T, Q> const& w)
	{
		ASTER_LINEAR_STATIC_ASSERT(std::numeric_limits<T>::is_iec559 || std::numeric_limits<T>::is_integer, "'min' only accept floating-point or integer inputs");
		return aster_linear::min(aster_linear::min(x, y), aster_linear::min(z, w));
	}

	template<length_t L, typename T, qualifier Q>
	ASTER_LINEAR_FUNC_QUALIFIER ASTER_LINEAR_CONSTEXPR vec<L, T, Q> max(vec<L, T, Q> const& x, vec<L, T, Q> const& y, vec<L, T, Q> const& z)
	{
		ASTER_LINEAR_STATIC_ASSERT(std::numeric_limits<T>::is_iec559 || std::numeric_limits<T>::is_integer, "'max' only accept floating-point or integer inputs");
		return aster_linear::max(aster_linear::max(x, y), z);
	}

	template<length_t L, typename T, qualifier Q>
	ASTER_LINEAR_FUNC_QUALIFIER ASTER_LINEAR_CONSTEXPR vec<L, T, Q> max(vec<L, T, Q> const& x, vec<L, T, Q> const& y, vec<L, T, Q> const& z, vec<L, T, Q> const& w)
	{
		ASTER_LINEAR_STATIC_ASSERT(std::numeric_limits<T>::is_iec559 || std::numeric_limits<T>::is_integer, "'max' only accept floating-point or integer inputs");
		return aster_linear::max(aster_linear::max(x, y), aster_linear::max(z, w));
	}

	template<length_t L, typename T, qualifier Q>
	ASTER_LINEAR_FUNC_QUALIFIER vec<L, T, Q> fmin(vec<L, T, Q> const& a, T b)
	{
		ASTER_LINEAR_STATIC_ASSERT(std::numeric_limits<T>::is_iec559, "'fmin' only accept floating-point inputs");
		return detail::functor2<vec, L, T, Q>::call(fmin, a, vec<L, T, Q>(b));
	}

	template<length_t L, typename T, qualifier Q>
	ASTER_LINEAR_FUNC_QUALIFIER vec<L, T, Q> fmin(vec<L, T, Q> const& a, vec<L, T, Q> const& b)
	{
		ASTER_LINEAR_STATIC_ASSERT(std::numeric_limits<T>::is_iec559, "'fmin' only accept floating-point inputs");
		return detail::functor2<vec, L, T, Q>::call(fmin, a, b);
	}

	template<length_t L, typename T, qualifier Q>
	ASTER_LINEAR_FUNC_QUALIFIER vec<L, T, Q> fmin(vec<L, T, Q> const& a, vec<L, T, Q> const& b, vec<L, T, Q> const& c)
	{
		ASTER_LINEAR_STATIC_ASSERT(std::numeric_limits<T>::is_iec559, "'fmin' only accept floating-point inputs");
		return fmin(fmin(a, b), c);
	}

	template<length_t L, typename T, qualifier Q>
	ASTER_LINEAR_FUNC_QUALIFIER vec<L, T, Q> fmin(vec<L, T, Q> const& a, vec<L, T, Q> const& b, vec<L, T, Q> const& c, vec<L, T, Q> const& d)
	{
		ASTER_LINEAR_STATIC_ASSERT(std::numeric_limits<T>::is_iec559, "'fmin' only accept floating-point inputs");
		return fmin(fmin(a, b), fmin(c, d));
	}

	template<length_t L, typename T, qualifier Q>
	ASTER_LINEAR_FUNC_QUALIFIER vec<L, T, Q> fmax(vec<L, T, Q> const& a, T b)
	{
		ASTER_LINEAR_STATIC_ASSERT(std::numeric_limits<T>::is_iec559, "'fmax' only accept floating-point inputs");
		return detail::functor2<vec, L, T, Q>::call(fmax, a, vec<L, T, Q>(b));
	}

	template<length_t L, typename T, qualifier Q>
	ASTER_LINEAR_FUNC_QUALIFIER vec<L, T, Q> fmax(vec<L, T, Q> const& a, vec<L, T, Q> const& b)
	{
		ASTER_LINEAR_STATIC_ASSERT(std::numeric_limits<T>::is_iec559, "'fmax' only accept floating-point inputs");
		return detail::functor2<vec, L, T, Q>::call(fmax, a, b);
	}

	template<length_t L, typename T, qualifier Q>
	ASTER_LINEAR_FUNC_QUALIFIER vec<L, T, Q> fmax(vec<L, T, Q> const& a, vec<L, T, Q> const& b, vec<L, T, Q> const& c)
	{
		ASTER_LINEAR_STATIC_ASSERT(std::numeric_limits<T>::is_iec559, "'fmax' only accept floating-point inputs");
		return fmax(fmax(a, b), c);
	}

	template<length_t L, typename T, qualifier Q>
	ASTER_LINEAR_FUNC_QUALIFIER vec<L, T, Q> fmax(vec<L, T, Q> const& a, vec<L, T, Q> const& b, vec<L, T, Q> const& c, vec<L, T, Q> const& d)
	{
		ASTER_LINEAR_STATIC_ASSERT(std::numeric_limits<T>::is_iec559, "'fmax' only accept floating-point inputs");
		return fmax(fmax(a, b), fmax(c, d));
	}

	template<length_t L, typename T, qualifier Q>
	ASTER_LINEAR_FUNC_QUALIFIER vec<L, T, Q> fclamp(vec<L, T, Q> const& x, T minVal, T maxVal)
	{
		return fmin(fmax(x, vec<L, T, Q>(minVal)), vec<L, T, Q>(maxVal));
	}

	template<length_t L, typename T, qualifier Q>
	ASTER_LINEAR_FUNC_QUALIFIER vec<L, T, Q> fclamp(vec<L, T, Q> const& x, vec<L, T, Q> const& minVal, vec<L, T, Q> const& maxVal)
	{
		return fmin(fmax(x, minVal), maxVal);
	}

	template<length_t L, typename T, qualifier Q>
	ASTER_LINEAR_FUNC_QUALIFIER vec<L, T, Q> clamp(vec<L, T, Q> const& Texcoord)
	{
		return aster_linear::clamp(Texcoord, vec<L, T, Q>(0), vec<L, T, Q>(1));
	}

	template<length_t L, typename T, qualifier Q>
	ASTER_LINEAR_FUNC_QUALIFIER vec<L, T, Q> repeat(vec<L, T, Q> const& Texcoord)
	{
		return aster_linear::fract(Texcoord);
	}

	template<length_t L, typename T, qualifier Q>
	ASTER_LINEAR_FUNC_QUALIFIER vec<L, T, Q> mirrorClamp(vec<L, T, Q> const& Texcoord)
	{
		return aster_linear::fract(aster_linear::abs(Texcoord));
	}

	template<length_t L, typename T, qualifier Q>
	ASTER_LINEAR_FUNC_QUALIFIER vec<L, T, Q> mirrorRepeat(vec<L, T, Q> const& Texcoord)
	{
		vec<L, T, Q> const Abs = aster_linear::abs(Texcoord);
		vec<L, T, Q> const Clamp = aster_linear::mod(aster_linear::floor(Abs), vec<L, T, Q>(2));
		vec<L, T, Q> const Floor = aster_linear::floor(Abs);
		vec<L, T, Q> const Rest = Abs - Floor;
		vec<L, T, Q> const Mirror = Clamp + Rest;
		return mix(Rest, vec<L, T, Q>(1) - Rest, aster_linear::greaterThanEqual(Mirror, vec<L, T, Q>(1)));
	}

	template<length_t L, typename T, qualifier Q>
	ASTER_LINEAR_FUNC_QUALIFIER vec<L, int, Q> iround(vec<L, T, Q> const& x)
	{
		ASTER_LINEAR_STATIC_ASSERT(std::numeric_limits<T>::is_iec559, "'iround' only accept floating-point inputs");
		assert(all(lessThanEqual(vec<L, T, Q>(0), x)));

		return vec<L, int, Q>(x + static_cast<T>(0.5));
	}

	template<length_t L, typename T, qualifier Q>
	ASTER_LINEAR_FUNC_QUALIFIER vec<L, uint, Q> uround(vec<L, T, Q> const& x)
	{
		ASTER_LINEAR_STATIC_ASSERT(std::numeric_limits<T>::is_iec559, "'uround' only accept floating-point inputs");
		assert(all(lessThanEqual(vec<L, T, Q>(0), x)));

		return vec<L, uint, Q>(x + static_cast<T>(0.5));
	}
}//namespace aster_linear
