/// @ref gtx_extended_min_max

namespace aster_linear
{
	template<typename T>
	ASTER_LINEAR_FUNC_QUALIFIER T min(
		T const& x,
		T const& y,
		T const& z)
	{
		return aster_linear::min(aster_linear::min(x, y), z);
	}

	template<typename T, template<typename> class C>
	ASTER_LINEAR_FUNC_QUALIFIER C<T> min
	(
		C<T> const& x,
		typename C<T>::T const& y,
		typename C<T>::T const& z
	)
	{
		return aster_linear::min(aster_linear::min(x, y), z);
	}

	template<typename T, template<typename> class C>
	ASTER_LINEAR_FUNC_QUALIFIER C<T> min
	(
		C<T> const& x,
		C<T> const& y,
		C<T> const& z
	)
	{
		return aster_linear::min(aster_linear::min(x, y), z);
	}

	template<typename T>
	ASTER_LINEAR_FUNC_QUALIFIER T min
	(
		T const& x,
		T const& y,
		T const& z,
		T const& w
	)
	{
		return aster_linear::min(aster_linear::min(x, y), aster_linear::min(z, w));
	}

	template<typename T, template<typename> class C>
	ASTER_LINEAR_FUNC_QUALIFIER C<T> min
	(
		C<T> const& x,
		typename C<T>::T const& y,
		typename C<T>::T const& z,
		typename C<T>::T const& w
	)
	{
		return aster_linear::min(aster_linear::min(x, y), aster_linear::min(z, w));
	}

	template<typename T, template<typename> class C>
	ASTER_LINEAR_FUNC_QUALIFIER C<T> min
	(
		C<T> const& x,
		C<T> const& y,
		C<T> const& z,
		C<T> const& w
	)
	{
		return aster_linear::min(aster_linear::min(x, y), aster_linear::min(z, w));
	}

	template<typename T>
	ASTER_LINEAR_FUNC_QUALIFIER T max(
		T const& x,
		T const& y,
		T const& z)
	{
		return aster_linear::max(aster_linear::max(x, y), z);
	}

	template<typename T, template<typename> class C>
	ASTER_LINEAR_FUNC_QUALIFIER C<T> max
	(
		C<T> const& x,
		typename C<T>::T const& y,
		typename C<T>::T const& z
	)
	{
		return aster_linear::max(aster_linear::max(x, y), z);
	}

	template<typename T, template<typename> class C>
	ASTER_LINEAR_FUNC_QUALIFIER C<T> max
	(
		C<T> const& x,
		C<T> const& y,
		C<T> const& z
	)
	{
		return aster_linear::max(aster_linear::max(x, y), z);
	}

	template<typename T>
	ASTER_LINEAR_FUNC_QUALIFIER T max
	(
		T const& x,
		T const& y,
		T const& z,
		T const& w
	)
	{
		return aster_linear::max(aster_linear::max(x, y), aster_linear::max(z, w));
	}

	template<typename T, template<typename> class C>
	ASTER_LINEAR_FUNC_QUALIFIER C<T> max
	(
		C<T> const& x,
		typename C<T>::T const& y,
		typename C<T>::T const& z,
		typename C<T>::T const& w
	)
	{
		return aster_linear::max(aster_linear::max(x, y), aster_linear::max(z, w));
	}

	template<typename T, template<typename> class C>
	ASTER_LINEAR_FUNC_QUALIFIER C<T> max
	(
		C<T> const& x,
		C<T> const& y,
		C<T> const& z,
		C<T> const& w
	)
	{
		return aster_linear::max(aster_linear::max(x, y), aster_linear::max(z, w));
	}
}//namespace aster_linear
