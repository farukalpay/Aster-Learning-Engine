/// @ref ext_scalar_int_sized
/// @file aster_linear/ext/scalar_int_sized.hpp
///
/// @defgroup ext_scalar_int_sized ASTER_LINEAR_EXT_scalar_int_sized
/// @ingroup ext
///
/// Exposes sized signed integer scalar types.
///
/// Include <aster_linear/ext/scalar_int_sized.hpp> to use the features of this extension.
///
/// @see ext_scalar_uint_sized

#pragma once

#include "../detail/setup.hpp"

#if ASTER_LINEAR_MESSAGES == ASTER_LINEAR_ENABLE && !defined(ASTER_LINEAR_EXT_INCLUDED)
#	pragma message("ASTER_LINEAR: ASTER_LINEAR_EXT_scalar_int_sized extension included")
#endif

namespace aster_linear{
namespace detail
{
#	if ASTER_LINEAR_HAS_EXTENDED_INTEGER_TYPE
		typedef std::int8_t			int8;
		typedef std::int16_t		int16;
		typedef std::int32_t		int32;
#	else
		typedef signed char			int8;
		typedef signed short		int16;
		typedef signed int			int32;
#endif//

	template<>
	struct is_int<int8>
	{
		enum test {value = ~0};
	};

	template<>
	struct is_int<int16>
	{
		enum test {value = ~0};
	};

	template<>
	struct is_int<int64>
	{
		enum test {value = ~0};
	};
}//namespace detail


	/// @addtogroup ext_scalar_int_sized
	/// @{

	/// 8 bit signed integer type.
	typedef detail::int8		int8;

	/// 16 bit signed integer type.
	typedef detail::int16		int16;

	/// 32 bit signed integer type.
	typedef detail::int32		int32;

	/// 64 bit signed integer type.
	typedef detail::int64		int64;

	/// @}
}//namespace aster_linear
