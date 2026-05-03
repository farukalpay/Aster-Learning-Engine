#include "coroutine.hpp"

#ifndef reenter
# define reenter(c) NETIO_CORO_REENTER(c)
#endif

#ifndef yield
# define yield NETIO_CORO_YIELD
#endif

#ifndef fork
# define fork NETIO_CORO_FORK
#endif
