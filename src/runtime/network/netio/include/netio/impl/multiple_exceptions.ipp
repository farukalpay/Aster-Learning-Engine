#ifndef NETIO_IMPL_MULTIPLE_EXCEPTIONS_IPP
#define NETIO_IMPL_MULTIPLE_EXCEPTIONS_IPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"
#include "netio/multiple_exceptions.hpp"

#include "netio/detail/push_options.hpp"

namespace netio {

multiple_exceptions::multiple_exceptions(
    std::exception_ptr first) noexcept
  : first_(static_cast<std::exception_ptr&&>(first))
{
}

const char* multiple_exceptions::what() const noexcept
{
  return "multiple exceptions";
}

std::exception_ptr multiple_exceptions::first_exception() const
{
  return first_;
}

} // namespace netio

#include "netio/detail/pop_options.hpp"

#endif // NETIO_IMPL_MULTIPLE_EXCEPTIONS_IPP
