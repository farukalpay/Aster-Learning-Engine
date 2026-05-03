#ifndef NETIO_VERSION_HPP
#define NETIO_VERSION_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

// NETIO_VERSION % 100 is the sub-minor version
// NETIO_VERSION / 100 % 1000 is the minor version
// NETIO_VERSION / 100000 is the major version
#define NETIO_VERSION 103800 // 1.38.0

#endif // NETIO_VERSION_HPP
