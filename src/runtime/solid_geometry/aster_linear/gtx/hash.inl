/// @ref gtx_hash
///
/// @see core (dependence)
///
/// @defgroup gtx_hash ASTER_LINEAR_GTX_hash
/// @ingroup gtx
///
/// @brief Add std::hash support for aster_linear types
///
/// <aster_linear/gtx/hash.inl> need to be included to use the features of this extension.

namespace aster_linear {
namespace detail
{
	ASTER_LINEAR_INLINE void hash_combine(size_t &seed, size_t hash)
	{
		hash += 0x9e3779b9 + (seed << 6) + (seed >> 2);
		seed ^= hash;
	}
}}

namespace std
{
	template<typename T, aster_linear::qualifier Q>
	ASTER_LINEAR_FUNC_QUALIFIER size_t hash<aster_linear::vec<1, T, Q>>::operator()(aster_linear::vec<1, T, Q> const& v) const
	{
		hash<T> hasher;
		return hasher(v.x);
	}

	template<typename T, aster_linear::qualifier Q>
	ASTER_LINEAR_FUNC_QUALIFIER size_t hash<aster_linear::vec<2, T, Q>>::operator()(aster_linear::vec<2, T, Q> const& v) const
	{
		size_t seed = 0;
		hash<T> hasher;
		aster_linear::detail::hash_combine(seed, hasher(v.x));
		aster_linear::detail::hash_combine(seed, hasher(v.y));
		return seed;
	}

	template<typename T, aster_linear::qualifier Q>
	ASTER_LINEAR_FUNC_QUALIFIER size_t hash<aster_linear::vec<3, T, Q>>::operator()(aster_linear::vec<3, T, Q> const& v) const
	{
		size_t seed = 0;
		hash<T> hasher;
		aster_linear::detail::hash_combine(seed, hasher(v.x));
		aster_linear::detail::hash_combine(seed, hasher(v.y));
		aster_linear::detail::hash_combine(seed, hasher(v.z));
		return seed;
	}

	template<typename T, aster_linear::qualifier Q>
	ASTER_LINEAR_FUNC_QUALIFIER size_t hash<aster_linear::vec<4, T, Q>>::operator()(aster_linear::vec<4, T, Q> const& v) const
	{
		size_t seed = 0;
		hash<T> hasher;
		aster_linear::detail::hash_combine(seed, hasher(v.x));
		aster_linear::detail::hash_combine(seed, hasher(v.y));
		aster_linear::detail::hash_combine(seed, hasher(v.z));
		aster_linear::detail::hash_combine(seed, hasher(v.w));
		return seed;
	}

	template<typename T, aster_linear::qualifier Q>
	ASTER_LINEAR_FUNC_QUALIFIER size_t hash<aster_linear::qua<T, Q>>::operator()(aster_linear::qua<T,Q> const& q) const
	{
		size_t seed = 0;
		hash<T> hasher;
		aster_linear::detail::hash_combine(seed, hasher(q.x));
		aster_linear::detail::hash_combine(seed, hasher(q.y));
		aster_linear::detail::hash_combine(seed, hasher(q.z));
		aster_linear::detail::hash_combine(seed, hasher(q.w));
		return seed;
	}

	template<typename T, aster_linear::qualifier Q>
	ASTER_LINEAR_FUNC_QUALIFIER size_t hash<aster_linear::tdualquat<T, Q>>::operator()(aster_linear::tdualquat<T, Q> const& q) const
	{
		size_t seed = 0;
		hash<aster_linear::qua<T, Q>> hasher;
		aster_linear::detail::hash_combine(seed, hasher(q.real));
		aster_linear::detail::hash_combine(seed, hasher(q.dual));
		return seed;
	}

	template<typename T, aster_linear::qualifier Q>
	ASTER_LINEAR_FUNC_QUALIFIER size_t hash<aster_linear::mat<2, 2, T, Q>>::operator()(aster_linear::mat<2, 2, T, Q> const& m) const
	{
		size_t seed = 0;
		hash<aster_linear::vec<2, T, Q>> hasher;
		aster_linear::detail::hash_combine(seed, hasher(m[0]));
		aster_linear::detail::hash_combine(seed, hasher(m[1]));
		return seed;
	}

	template<typename T, aster_linear::qualifier Q>
	ASTER_LINEAR_FUNC_QUALIFIER size_t hash<aster_linear::mat<2, 3, T, Q>>::operator()(aster_linear::mat<2, 3, T, Q> const& m) const
	{
		size_t seed = 0;
		hash<aster_linear::vec<3, T, Q>> hasher;
		aster_linear::detail::hash_combine(seed, hasher(m[0]));
		aster_linear::detail::hash_combine(seed, hasher(m[1]));
		return seed;
	}

	template<typename T, aster_linear::qualifier Q>
	ASTER_LINEAR_FUNC_QUALIFIER size_t hash<aster_linear::mat<2, 4, T, Q>>::operator()(aster_linear::mat<2, 4, T, Q> const& m) const
	{
		size_t seed = 0;
		hash<aster_linear::vec<4, T, Q>> hasher;
		aster_linear::detail::hash_combine(seed, hasher(m[0]));
		aster_linear::detail::hash_combine(seed, hasher(m[1]));
		return seed;
	}

	template<typename T, aster_linear::qualifier Q>
	ASTER_LINEAR_FUNC_QUALIFIER size_t hash<aster_linear::mat<3, 2, T, Q>>::operator()(aster_linear::mat<3, 2, T, Q> const& m) const
	{
		size_t seed = 0;
		hash<aster_linear::vec<2, T, Q>> hasher;
		aster_linear::detail::hash_combine(seed, hasher(m[0]));
		aster_linear::detail::hash_combine(seed, hasher(m[1]));
		aster_linear::detail::hash_combine(seed, hasher(m[2]));
		return seed;
	}

	template<typename T, aster_linear::qualifier Q>
	ASTER_LINEAR_FUNC_QUALIFIER size_t hash<aster_linear::mat<3, 3, T, Q>>::operator()(aster_linear::mat<3, 3, T, Q> const& m) const
	{
		size_t seed = 0;
		hash<aster_linear::vec<3, T, Q>> hasher;
		aster_linear::detail::hash_combine(seed, hasher(m[0]));
		aster_linear::detail::hash_combine(seed, hasher(m[1]));
		aster_linear::detail::hash_combine(seed, hasher(m[2]));
		return seed;
	}

	template<typename T, aster_linear::qualifier Q>
	ASTER_LINEAR_FUNC_QUALIFIER size_t hash<aster_linear::mat<3, 4, T, Q>>::operator()(aster_linear::mat<3, 4, T, Q> const& m) const
	{
		size_t seed = 0;
		hash<aster_linear::vec<4, T, Q>> hasher;
		aster_linear::detail::hash_combine(seed, hasher(m[0]));
		aster_linear::detail::hash_combine(seed, hasher(m[1]));
		aster_linear::detail::hash_combine(seed, hasher(m[2]));
		return seed;
	}

	template<typename T, aster_linear::qualifier Q>
	ASTER_LINEAR_FUNC_QUALIFIER size_t hash<aster_linear::mat<4, 2, T,Q>>::operator()(aster_linear::mat<4, 2, T,Q> const& m) const
	{
		size_t seed = 0;
		hash<aster_linear::vec<2, T, Q>> hasher;
		aster_linear::detail::hash_combine(seed, hasher(m[0]));
		aster_linear::detail::hash_combine(seed, hasher(m[1]));
		aster_linear::detail::hash_combine(seed, hasher(m[2]));
		aster_linear::detail::hash_combine(seed, hasher(m[3]));
		return seed;
	}

	template<typename T, aster_linear::qualifier Q>
	ASTER_LINEAR_FUNC_QUALIFIER size_t hash<aster_linear::mat<4, 3, T,Q>>::operator()(aster_linear::mat<4, 3, T,Q> const& m) const
	{
		size_t seed = 0;
		hash<aster_linear::vec<3, T, Q>> hasher;
		aster_linear::detail::hash_combine(seed, hasher(m[0]));
		aster_linear::detail::hash_combine(seed, hasher(m[1]));
		aster_linear::detail::hash_combine(seed, hasher(m[2]));
		aster_linear::detail::hash_combine(seed, hasher(m[3]));
		return seed;
	}

	template<typename T, aster_linear::qualifier Q>
	ASTER_LINEAR_FUNC_QUALIFIER size_t hash<aster_linear::mat<4, 4, T,Q>>::operator()(aster_linear::mat<4, 4, T, Q> const& m) const
	{
		size_t seed = 0;
		hash<aster_linear::vec<4, T, Q>> hasher;
		aster_linear::detail::hash_combine(seed, hasher(m[0]));
		aster_linear::detail::hash_combine(seed, hasher(m[1]));
		aster_linear::detail::hash_combine(seed, hasher(m[2]));
		aster_linear::detail::hash_combine(seed, hasher(m[3]));
		return seed;
	}
}
