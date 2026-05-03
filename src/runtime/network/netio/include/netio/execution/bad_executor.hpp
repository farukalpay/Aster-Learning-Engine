#ifndef NETIO_EXECUTION_BAD_EXECUTOR_HPP
#define NETIO_EXECUTION_BAD_EXECUTOR_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"
#include <exception>
#include "netio/detail/push_options.hpp"

namespace netio {
namespace execution {

/// Exception thrown when trying to access an empty polymorphic executor.
class bad_executor
  : public std::exception
{
public:
  /// Constructor.
  NETIO_DECL bad_executor() noexcept;

  /// Obtain message associated with exception.
  NETIO_DECL virtual const char* what() const noexcept;
};

} // namespace execution
} // namespace netio

#include "netio/detail/pop_options.hpp"

#if defined(NETIO_HEADER_ONLY)
# include "netio/execution/impl/bad_executor.ipp"
#endif // defined(NETIO_HEADER_ONLY)

#endif // NETIO_EXECUTION_BAD_EXECUTOR_HPP
