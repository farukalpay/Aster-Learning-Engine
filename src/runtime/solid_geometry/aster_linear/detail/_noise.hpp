#pragma once

#include "../common.hpp"

namespace aster_linear{
namespace detail
{
	template<typename T>
	ASTER_LINEAR_FUNC_QUALIFIER T mod289(T const& x)
	{
		return x - floor(x * (static_cast<T>(1.0) / static_cast<T>(289.0))) * static_cast<T>(289.0);
	}

	template<typename T>
	ASTER_LINEAR_FUNC_QUALIFIER T permute(T const& x)
	{
		return mod289(((x * static_cast<T>(34)) + static_cast<T>(1)) * x);
	}

	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_QUALIFIER vec<2, T, Q> permute(vec<2, T, Q> const& x)
	{
		return mod289(((x * static_cast<T>(34)) + static_cast<T>(1)) * x);
	}

	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_QUALIFIER vec<3, T, Q> permute(vec<3, T, Q> const& x)
	{
		return mod289(((x * static_cast<T>(34)) + static_cast<T>(1)) * x);
	}

	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_QUALIFIER vec<4, T, Q> permute(vec<4, T, Q> const& x)
	{
		return mod289(((x * static_cast<T>(34)) + static_cast<T>(1)) * x);
	}

	template<typename T>
	ASTER_LINEAR_FUNC_QUALIFIER T taylorInvSqrt(T const& r)
	{
		return static_cast<T>(1.79284291400159) - static_cast<T>(0.85373472095314) * r;
	}

	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_QUALIFIER vec<2, T, Q> taylorInvSqrt(vec<2, T, Q> const& r)
	{
		return static_cast<T>(1.79284291400159) - static_cast<T>(0.85373472095314) * r;
	}

	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_QUALIFIER vec<3, T, Q> taylorInvSqrt(vec<3, T, Q> const& r)
	{
		return static_cast<T>(1.79284291400159) - static_cast<T>(0.85373472095314) * r;
	}

	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_QUALIFIER vec<4, T, Q> taylorInvSqrt(vec<4, T, Q> const& r)
	{
		return static_cast<T>(1.79284291400159) - static_cast<T>(0.85373472095314) * r;
	}

	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_QUALIFIER vec<2, T, Q> fade(vec<2, T, Q> const& t)
	{
		return (t * t * t) * (t * (t * static_cast<T>(6) - static_cast<T>(15)) + static_cast<T>(10));
	}

	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_QUALIFIER vec<3, T, Q> fade(vec<3, T, Q> const& t)
	{
		return (t * t * t) * (t * (t * static_cast<T>(6) - static_cast<T>(15)) + static_cast<T>(10));
	}

	template<typename T, qualifier Q>
	ASTER_LINEAR_FUNC_QUALIFIER vec<4, T, Q> fade(vec<4, T, Q> const& t)
	{
		return (t * t * t) * (t * (t * static_cast<T>(6) - static_cast<T>(15)) + static_cast<T>(10));
	}
}//namespace detail
}//namespace aster_linear

