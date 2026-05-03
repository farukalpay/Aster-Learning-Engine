#ifndef NETIO_DETAIL_REGEX_FWD_HPP
#define NETIO_DETAIL_REGEX_FWD_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#if defined(NETIO_HAS_BOOST_REGEX)

namespace boost {

template <class BidiIterator>
struct sub_match;

template <class BidiIterator, class Allocator>
class match_results;

template <class CharT, class Traits>
class basic_regex;

} // namespace boost

#endif // defined(NETIO_HAS_BOOST_REGEX)

#endif // NETIO_DETAIL_REGEX_FWD_HPP
