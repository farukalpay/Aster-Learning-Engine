
//=============================================================================
//
//  Implements a simple singleton template
//
//=============================================================================


#ifndef __SINGLETON_HH__
#define __SINGLETON_HH__


//=== INCLUDES ================================================================

// AsterTopology
#include <AsterTopology/Core/System/config.h>

// STL
#include <stdexcept>


//== NAMESPACES ===============================================================


namespace AsterTopology {


//=== IMPLEMENTATION ==========================================================


/** A simple singleton template.
    Encapsulates an arbitrary class and enforces its uniqueness.
*/

template <typename T>
class SingletonT
{
public:

  /** Singleton access function.
      Use this function to obtain a reference to the instance of the 
      encapsulated class. Note that this instance is unique and created
      on the first call to Instance().
  */

  static T& Instance()
  {
    if (!pInstance__)
    {
      // check if singleton alive
      if (destroyed__)
      {
	OnDeadReference();
      }
      // first time request -> initialize
      else
      {
	Create();
      }
    }
    return *pInstance__;
  }


private:

  // Disable constructors/assignment to enforce uniqueness
  SingletonT();
  SingletonT(const SingletonT&);
  SingletonT& operator=(const SingletonT&);

  // Create a new singleton and store its pointer
  static void Create()
  {
    static T theInstance;
    pInstance__ = &theInstance;
  }
  
  // Will be called if instance is accessed after its lifetime has expired
  static void OnDeadReference()
  {
    throw std::runtime_error("[Singelton error] - Dead reference detected!\n");
  }

  virtual ~SingletonT()
  {
    pInstance__ = 0;
    destroyed__ = true;
  }
  
  static T*     pInstance__;
  static bool   destroyed__;
};



//=============================================================================
} // namespace AsterTopology
//=============================================================================
#if defined(OM_INCLUDE_TEMPLATES) && !defined(ASTER_TOPOLOGY_SINGLETON_C)
#  define ASTER_TOPOLOGY_SINGLETON_TEMPLATES
#  include "SingletonT.cc"
#endif
//=============================================================================
#endif // __SINGLETON_HH__
//=============================================================================
