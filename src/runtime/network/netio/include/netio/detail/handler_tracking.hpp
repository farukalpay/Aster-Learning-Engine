#ifndef NETIO_DETAIL_HANDLER_TRACKING_HPP
#define NETIO_DETAIL_HANDLER_TRACKING_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"

namespace netio {

class execution_context;

} // namespace netio

#if defined(NETIO_CUSTOM_HANDLER_TRACKING)
# include NETIO_CUSTOM_HANDLER_TRACKING
#elif defined(NETIO_ENABLE_HANDLER_TRACKING)
# include "netio/error_code.hpp"
# include "netio/detail/cstdint.hpp"
# include "netio/detail/static_mutex.hpp"
# include "netio/detail/tss_ptr.hpp"
#endif // defined(NETIO_ENABLE_HANDLER_TRACKING)

#include "netio/detail/push_options.hpp"

namespace netio {
namespace detail {

#if defined(NETIO_CUSTOM_HANDLER_TRACKING)

// The user-specified header must define the following macros:
// - NETIO_INHERIT_TRACKED_HANDLER
// - NETIO_ALSO_INHERIT_TRACKED_HANDLER
// - NETIO_HANDLER_TRACKING_INIT
// - NETIO_HANDLER_CREATION(args)
// - NETIO_HANDLER_COMPLETION(args)
// - NETIO_HANDLER_INVOCATION_BEGIN(args)
// - NETIO_HANDLER_INVOCATION_END
// - NETIO_HANDLER_OPERATION(args)
// - NETIO_HANDLER_REACTOR_REGISTRATION(args)
// - NETIO_HANDLER_REACTOR_DEREGISTRATION(args)
// - NETIO_HANDLER_REACTOR_READ_EVENT
// - NETIO_HANDLER_REACTOR_WRITE_EVENT
// - NETIO_HANDLER_REACTOR_ERROR_EVENT
// - NETIO_HANDLER_REACTOR_EVENTS(args)
// - NETIO_HANDLER_REACTOR_OPERATION(args)

# if !defined(NETIO_ENABLE_HANDLER_TRACKING)
#  define NETIO_ENABLE_HANDLER_TRACKING 1
# endif /// !defined(NETIO_ENABLE_HANDLER_TRACKING)

#elif defined(NETIO_ENABLE_HANDLER_TRACKING)

class handler_tracking
{
public:
  class completion;

  // Base class for objects containing tracked handlers.
  class tracked_handler
  {
  private:
    // Only the handler_tracking class will have access to the id.
    friend class handler_tracking;
    friend class completion;
    uint64_t id_;

  protected:
    // Constructor initialises with no id.
    tracked_handler() : id_(0) {}

    // Prevent deletion through this type.
    ~tracked_handler() {}
  };

  // Initialise the tracking system.
  NETIO_DECL static void init();

  class location
  {
  public:
    // Constructor adds a location to the stack.
    NETIO_DECL explicit location(const char* file,
        int line, const char* func);

    // Destructor removes a location from the stack.
    NETIO_DECL ~location();

  private:
    // Disallow copying and assignment.
    location(const location&) = delete;
    location& operator=(const location&) = delete;

    friend class handler_tracking;
    const char* file_;
    int line_;
    const char* func_;
    location* next_;
  };

  // Record the creation of a tracked handler.
  NETIO_DECL static void creation(
      execution_context& context, tracked_handler& h,
      const char* object_type, void* object,
      uintmax_t native_handle, const char* op_name);

  class completion
  {
  public:
    // Constructor records that handler is to be invoked with no arguments.
    NETIO_DECL explicit completion(const tracked_handler& h);

    // Destructor records only when an exception is thrown from the handler, or
    // if the memory is being freed without the handler having been invoked.
    NETIO_DECL ~completion();

    // Records that handler is to be invoked with no arguments.
    NETIO_DECL void invocation_begin();

    // Records that handler is to be invoked with one arguments.
    NETIO_DECL void invocation_begin(const netio::error_code& ec);

    // Constructor records that handler is to be invoked with two arguments.
    NETIO_DECL void invocation_begin(
        const netio::error_code& ec, std::size_t bytes_transferred);

    // Constructor records that handler is to be invoked with two arguments.
    NETIO_DECL void invocation_begin(
        const netio::error_code& ec, int signal_number);

    // Constructor records that handler is to be invoked with two arguments.
    NETIO_DECL void invocation_begin(
        const netio::error_code& ec, const char* arg);

    // Record that handler invocation has ended.
    NETIO_DECL void invocation_end();

  private:
    friend class handler_tracking;
    uint64_t id_;
    bool invoked_;
    completion* next_;
  };

  // Record an operation that is not directly associated with a handler.
  NETIO_DECL static void operation(execution_context& context,
      const char* object_type, void* object,
      uintmax_t native_handle, const char* op_name);

  // Record that a descriptor has been registered with the reactor.
  NETIO_DECL static void reactor_registration(execution_context& context,
      uintmax_t native_handle, uintmax_t registration);

  // Record that a descriptor has been deregistered from the reactor.
  NETIO_DECL static void reactor_deregistration(execution_context& context,
      uintmax_t native_handle, uintmax_t registration);

  // Record a reactor-based operation that is associated with a handler.
  NETIO_DECL static void reactor_events(execution_context& context,
      uintmax_t registration, unsigned events);

  // Record a reactor-based operation that is associated with a handler.
  NETIO_DECL static void reactor_operation(
      const tracked_handler& h, const char* op_name,
      const netio::error_code& ec);

  // Record a reactor-based operation that is associated with a handler.
  NETIO_DECL static void reactor_operation(
      const tracked_handler& h, const char* op_name,
      const netio::error_code& ec, std::size_t bytes_transferred);

  // Write a line of output.
  NETIO_DECL static void write_line(const char* format, ...);

private:
  struct tracking_state;
  NETIO_DECL static tracking_state* get_state();
};

# define NETIO_INHERIT_TRACKED_HANDLER \
  : public netio::detail::handler_tracking::tracked_handler

# define NETIO_ALSO_INHERIT_TRACKED_HANDLER \
  , public netio::detail::handler_tracking::tracked_handler

# define NETIO_HANDLER_TRACKING_INIT \
  netio::detail::handler_tracking::init()

# define NETIO_HANDLER_LOCATION(args) \
  netio::detail::handler_tracking::location tracked_location args

# define NETIO_HANDLER_CREATION(args) \
  netio::detail::handler_tracking::creation args

# define NETIO_HANDLER_COMPLETION(args) \
  netio::detail::handler_tracking::completion tracked_completion args

# define NETIO_HANDLER_INVOCATION_BEGIN(args) \
  tracked_completion.invocation_begin args

# define NETIO_HANDLER_INVOCATION_END \
  tracked_completion.invocation_end()

# define NETIO_HANDLER_OPERATION(args) \
  netio::detail::handler_tracking::operation args

# define NETIO_HANDLER_REACTOR_REGISTRATION(args) \
  netio::detail::handler_tracking::reactor_registration args

# define NETIO_HANDLER_REACTOR_DEREGISTRATION(args) \
  netio::detail::handler_tracking::reactor_deregistration args

# define NETIO_HANDLER_REACTOR_READ_EVENT 1
# define NETIO_HANDLER_REACTOR_WRITE_EVENT 2
# define NETIO_HANDLER_REACTOR_ERROR_EVENT 4

# define NETIO_HANDLER_REACTOR_EVENTS(args) \
  netio::detail::handler_tracking::reactor_events args

# define NETIO_HANDLER_REACTOR_OPERATION(args) \
  netio::detail::handler_tracking::reactor_operation args

#else // defined(NETIO_ENABLE_HANDLER_TRACKING)

# define NETIO_INHERIT_TRACKED_HANDLER
# define NETIO_ALSO_INHERIT_TRACKED_HANDLER
# define NETIO_HANDLER_TRACKING_INIT (void)0
# define NETIO_HANDLER_LOCATION(loc) (void)0
# define NETIO_HANDLER_CREATION(args) (void)0
# define NETIO_HANDLER_COMPLETION(args) (void)0
# define NETIO_HANDLER_INVOCATION_BEGIN(args) (void)0
# define NETIO_HANDLER_INVOCATION_END (void)0
# define NETIO_HANDLER_OPERATION(args) (void)0
# define NETIO_HANDLER_REACTOR_REGISTRATION(args) (void)0
# define NETIO_HANDLER_REACTOR_DEREGISTRATION(args) (void)0
# define NETIO_HANDLER_REACTOR_READ_EVENT 0
# define NETIO_HANDLER_REACTOR_WRITE_EVENT 0
# define NETIO_HANDLER_REACTOR_ERROR_EVENT 0
# define NETIO_HANDLER_REACTOR_EVENTS(args) (void)0
# define NETIO_HANDLER_REACTOR_OPERATION(args) (void)0

#endif // defined(NETIO_ENABLE_HANDLER_TRACKING)

} // namespace detail
} // namespace netio

#include "netio/detail/pop_options.hpp"

#if defined(NETIO_HEADER_ONLY)
# include "netio/detail/impl/handler_tracking.ipp"
#endif // defined(NETIO_HEADER_ONLY)

#endif // NETIO_DETAIL_HANDLER_TRACKING_HPP
