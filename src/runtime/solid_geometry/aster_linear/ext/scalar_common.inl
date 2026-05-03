namespace aster_linear
{
	template<typename T>
	ASTER_LINEAR_FUNC_QUALIFIER T min(T a, T b, T c)
	{
		return aster_linear::min(aster_linear::min(a, b), c);
	}

	template<typename T>
	ASTER_LINEAR_FUNC_QUALIFIER T min(T a, T b, T c, T d)
	{
		return aster_linear::min(aster_linear::min(a, b), aster_linear::min(c, d));
	}

	template<typename T>
	ASTER_LINEAR_FUNC_QUALIFIER T max(T a, T b, T c)
	{
		return aster_linear::max(aster_linear::max(a, b), c);
	}

	template<typename T>
	ASTER_LINEAR_FUNC_QUALIFIER T max(T a, T b, T c, T d)
	{
		return aster_linear::max(aster_linear::max(a, b), aster_linear::max(c, d));
	}

#	if ASTER_LINEAR_HAS_CXX11_STL
		using std::fmin;
#	else
		template<typename T>
		ASTER_LINEAR_FUNC_QUALIFIER T fmin(T a, T b)
		{
			ASTER_LINEAR_STATIC_ASSERT(std::numeric_limits<T>::is_iec559, "'fmin' only accept floating-point input");

			if (isnan(a))
				return b;
			return min(a, b);
		}
#	endif

	template<typename T>
	ASTER_LINEAR_FUNC_QUALIFIER T fmin(T a, T b, T c)
	{
		ASTER_LINEAR_STATIC_ASSERT(std::numeric_limits<T>::is_iec559, "'fmin' only accept floating-point input");

		if (isnan(a))
			return fmin(b, c);
		if (isnan(b))
			return fmin(a, c);
		if (isnan(c))
			return min(a, b);
		return min(a, b, c);
	}

	template<typename T>
	ASTER_LINEAR_FUNC_QUALIFIER T fmin(T a, T b, T c, T d)
	{
		ASTER_LINEAR_STATIC_ASSERT(std::numeric_limits<T>::is_iec559, "'fmin' only accept floating-point input");

		if (isnan(a))
			return fmin(b, c, d);
		if (isnan(b))
			return min(a, fmin(c, d));
		if (isnan(c))
			return fmin(min(a, b), d);
		if (isnan(d))
			return min(a, b, c);
		return min(a, b, c, d);
	}


#	if ASTER_LINEAR_HAS_CXX11_STL
		using std::fmax;
#	else
		template<typename T>
		ASTER_LINEAR_FUNC_QUALIFIER T fmax(T a, T b)
		{
			ASTER_LINEAR_STATIC_ASSERT(std::numeric_limits<T>::is_iec559, "'fmax' only accept floating-point input");

			if (isnan(a))
				return b;
			return max(a, b);
		}
#	endif

	template<typename T>
	ASTER_LINEAR_FUNC_QUALIFIER T fmax(T a, T b, T c)
	{
		ASTER_LINEAR_STATIC_ASSERT(std::numeric_limits<T>::is_iec559, "'fmax' only accept floating-point input");

		if (isnan(a))
			return fmax(b, c);
		if (isnan(b))
			return fmax(a, c);
		if (isnan(c))
			return max(a, b);
		return max(a, b, c);
	}

	template<typename T>
	ASTER_LINEAR_FUNC_QUALIFIER T fmax(T a, T b, T c, T d)
	{
		ASTER_LINEAR_STATIC_ASSERT(std::numeric_limits<T>::is_iec559, "'fmax' only accept floating-point input");

		if (isnan(a))
			return fmax(b, c, d);
		if (isnan(b))
			return max(a, fmax(c, d));
		if (isnan(c))
			return fmax(max(a, b), d);
		if (isnan(d))
			return max(a, b, c);
		return max(a, b, c, d);
	}

	// fclamp
	template<typename genType>
	ASTER_LINEAR_FUNC_QUALIFIER genType fclamp(genType x, genType minVal, genType maxVal)
	{
		ASTER_LINEAR_STATIC_ASSERT(std::numeric_limits<genType>::is_iec559, "'fclamp' only accept floating-point or integer inputs");
		return fmin(fmax(x, minVal), maxVal);
	}

	template<typename genType>
	ASTER_LINEAR_FUNC_QUALIFIER genType clamp(genType const& Texcoord)
	{
		return aster_linear::clamp(Texcoord, static_cast<genType>(0), static_cast<genType>(1));
	}

	template<typename genType>
	ASTER_LINEAR_FUNC_QUALIFIER genType repeat(genType const& Texcoord)
	{
		return aster_linear::fract(Texcoord);
	}

	template<typename genType>
	ASTER_LINEAR_FUNC_QUALIFIER genType mirrorClamp(genType const& Texcoord)
	{
		return aster_linear::fract(aster_linear::abs(Texcoord));
	}

	template<typename genType>
	ASTER_LINEAR_FUNC_QUALIFIER genType mirrorRepeat(genType const& Texcoord)
	{
		genType const Abs = aster_linear::abs(Texcoord);
		genType const Clamp = aster_linear::mod(aster_linear::floor(Abs), static_cast<genType>(2));
		genType const Floor = aster_linear::floor(Abs);
		genType const Rest = Abs - Floor;
		genType const Mirror = Clamp + Rest;
		return mix(Rest, static_cast<genType>(1) - Rest, Mirror >= static_cast<genType>(1));
	}

	template<typename genType>
	ASTER_LINEAR_FUNC_QUALIFIER int iround(genType const& x)
	{
		ASTER_LINEAR_STATIC_ASSERT(std::numeric_limits<genType>::is_iec559, "'iround' only accept floating-point inputs");
		assert(static_cast<genType>(0.0) <= x);

		return static_cast<int>(x + static_cast<genType>(0.5));
	}

	template<typename genType>
	ASTER_LINEAR_FUNC_QUALIFIER uint uround(genType const& x)
	{
		ASTER_LINEAR_STATIC_ASSERT(std::numeric_limits<genType>::is_iec559, "'uround' only accept floating-point inputs");
		assert(static_cast<genType>(0.0) <= x);

		return static_cast<uint>(x + static_cast<genType>(0.5));
	}
}//namespace aster_linear
