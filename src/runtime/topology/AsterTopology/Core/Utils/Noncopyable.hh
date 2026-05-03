
//=============================================================================
//
//  Implements the Non-Copyable metapher
//
//=============================================================================

#ifndef ASTER_TOPOLOGY_NONCOPYABLE_HH
#define ASTER_TOPOLOGY_NONCOPYABLE_HH


//-----------------------------------------------------------------------------

#include <AsterTopology/Core/System/config.h>

//-----------------------------------------------------------------------------

namespace AsterTopology {
namespace Utils {

//-----------------------------------------------------------------------------
   
/** This class demonstrates the non copyable idiom. In some cases it is
    important an object can't be copied. Deriving from Noncopyable makes sure
    all relevant constructor and operators are made inaccessable, for public
    AND derived classes.
**/
class Noncopyable
{
public:
  Noncopyable() { }

private:
  /// Prevent access to copy constructor
  Noncopyable( const Noncopyable& );
  
  /// Prevent access to assignment operator
  const Noncopyable& operator=( const Noncopyable& );
};

//=============================================================================
} // namespace Utils
} // namespace AsterTopology
//=============================================================================
#endif // ASTER_TOPOLOGY_NONCOPYABLE_HH
//=============================================================================
