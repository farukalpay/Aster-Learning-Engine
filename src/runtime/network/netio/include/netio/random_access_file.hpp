#ifndef NETIO_RANDOM_ACCESS_FILE_HPP
#define NETIO_RANDOM_ACCESS_FILE_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"

#if defined(NETIO_HAS_FILE) \
  || defined(GENERATING_DOCUMENTATION)

#include "netio/basic_random_access_file.hpp"

namespace netio {

/// Typedef for the typical usage of a random-access file.
typedef basic_random_access_file<> random_access_file;

} // namespace netio

#endif // defined(NETIO_HAS_FILE)
       //   || defined(GENERATING_DOCUMENTATION)

#endif // NETIO_RANDOM_ACCESS_FILE_HPP
