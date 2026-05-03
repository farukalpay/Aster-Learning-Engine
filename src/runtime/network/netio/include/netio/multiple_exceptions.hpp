#ifndef NETIO_MULTIPLE_EXCEPTIONS_HPP
#define NETIO_MULTIPLE_EXCEPTIONS_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"
#include <exception>
#include "netio/detail/push_options.hpp"

namespace netio {

/// Exception thrown when there are multiple pending exceptions to rethrow.
class multiple_exceptions
  : public std::exception
{
public:
  /// Constructor.
  NETIO_DECL multiple_exceptions(
      std::exception_ptr first) noexcept;

  /// Obtain message associated with exception.
  NETIO_DECL virtual const char* what() const
    noexcept;

  /// Obtain a pointer to the first exception.
  NETIO_DECL std::exception_ptr first_exception() const;

private:
  std::exception_ptr first_;
};

} // namespace netio

#include "netio/detail/pop_options.hpp"

#if defined(NETIO_HEADER_ONLY)
# include "netio/impl/multiple_exceptions.ipp"
#endif // defined(NETIO_HEADER_ONLY)

#endif // NETIO_MULTIPLE_EXCEPTIONS_HPP
