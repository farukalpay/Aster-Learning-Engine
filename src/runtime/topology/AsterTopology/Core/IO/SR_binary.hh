
//=============================================================================
//
//  Helper Functions for binary reading / writing
//
//=============================================================================

#ifndef ASTER_TOPOLOGY_SR_BINARY_HH
#define ASTER_TOPOLOGY_SR_BINARY_HH


//== INCLUDES =================================================================

#include <AsterTopology/Core/System/config.h>
// -------------------- STL
#include <typeinfo>
#include <stdexcept>
#include <sstream>
#include <numeric>   // accumulate
// -------------------- AsterTopology


//== NAMESPACES ===============================================================

namespace AsterTopology {
namespace IO {


//=============================================================================


//-----------------------------------------------------------------------------

  const static size_t UnknownSize(size_t(-1));


//-----------------------------------------------------------------------------
// struct binary, helper for storing/restoring

#define X \
   std::ostringstream msg; \
   msg << "Type not supported: " << typeid(value_type).name(); \
   throw std::logic_error(msg.str())

/// \struct binary SR_binary.hh <AsterTopology/Core/IO/SR_binary.hh>
///
/// The struct defines how to store and restore the type T.
/// It's used by the OM reader/writer modules.
///
/// The following specialization are provided:
/// - Fundamental types except \c long \c double
/// - %AsterTopology vector types
/// - %AsterTopology::StatusInfo
/// - std::string (max. length 65535)
///
/// \todo Complete documentation of members
template < typename T > struct binary
{
  typedef T     value_type;

  static const bool is_streamable = false;

  static size_t size_of(void) { return UnknownSize; }
  static size_t size_of(const value_type&) { return UnknownSize; }

  static 
  size_t store( std::ostream& /* _os */,
		const value_type& /* _v */,
		bool /* _swap=false */)
  { X; return 0; }

  static 
  size_t restore( std::istream& /* _is */,
		  value_type& /* _v */,
		  bool /* _swap=false */)
  { X; return 0; }
};

#undef X


//=============================================================================
} // namespace IO
} // namespace AsterTopology
//=============================================================================
#endif // ASTER_TOPOLOGY_SR_RBO_HH defined
//=============================================================================

