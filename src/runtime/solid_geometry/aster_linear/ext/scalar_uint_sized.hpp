/// @ref ext_scalar_uint_sized
/// @file aster_linear/ext/scalar_uint_sized.hpp
///
/// @defgroup ext_scalar_uint_sized ASTER_LINEAR_EXT_scalar_uint_sized
/// @ingroup ext
///
/// Exposes sized unsigned integer scalar types.
///
/// Include <aster_linear/ext/scalar_uint_sized.hpp> to use the features of this extension.
///
/// @see ext_scalar_int_sized

#pragma once

#include "../detail/setup.hpp"

#if ASTER_LINEAR_MESSAGES == ASTER_LINEAR_ENABLE && !defined(ASTER_LINEAR_EXT_INCLUDED)
#	pragma message("ASTER_LINEAR: ASTER_LINEAR_EXT_scalar_uint_sized extension included")
#endif

namespace aster_linear{
namespace detail
{
#	if ASTER_LINEAR_HAS_EXTENDED_INTEGER_TYPE
		typedef std::uint8_t		uint8;
		typedef std::uint16_t		uint16;
		typedef std::uint32_t		uint32;
#	else
		typedef unsigned char		uint8;
		typedef unsigned short		uint16;
		typedef unsigned int		uint32;
#endif

	template<>
	struct is_int<uint8>
	{
		enum test {value = ~0};
	};

	template<>
	struct is_int<uint16>
	{
		enum test {value = ~0};
	};

	template<>
	struct is_int<uint64>
	{
		enum test {value = ~0};
	};
}//namespace detail


	/// @addtogroup ext_scalar_uint_sized
	/// @{

	/// 8 bit unsigned integer type.
	typedef detail::uint8		uint8;

	/// 16 bit unsigned integer type.
	typedef detail::uint16		uint16;

	/// 32 bit unsigned integer type.
	typedef detail::uint32		uint32;

	/// 64 bit unsigned integer type.
	typedef detail::uint64		uint64;

	/// @}
}//namespace aster_linear
