//=============================================================================
//
//  CLASS newClass
//
//=============================================================================
#ifndef DOXY_IGNORE_THIS
#ifndef ASTER_TOPOLOGY_NEWCLASST_HH
#define ASTER_TOPOLOGY_NEWCLASST_HH


//== INCLUDES =================================================================


//== FORWARDDECLARATIONS ======================================================


//== NAMESPACES ===============================================================

namespace AsterTopology {


//== CLASS DEFINITION =========================================================



	      
/** \class newClassT newClassT.hh <AsterTopology/.../newClassT.hh>

    Brief Description.
  
    A more elaborate description follows.
*/

template <>
class newClassT
{
public:
   
  /// Default constructor
  newClassT() {}
 
  /// Destructor
  ~newClassT() {}

  
private:

  /// Copy constructor (not used)
  newClassT(const newClassT& _rhs);

  /// Assignment operator (not used)
  newClassT& operator=(const newClassT& _rhs);
  
};


//=============================================================================
} // namespace AsterTopology
//=============================================================================
#if defined(OM_INCLUDE_TEMPLATES) && !defined(ASTER_TOPOLOGY_NEWCLASS_C)
#define ASTER_TOPOLOGY_NEWCLASS_TEMPLATES
#include "newClass.cc"
#endif
//=============================================================================
#endif // ASTER_TOPOLOGY_NEWCLASST_HH defined
#endif // DOXY_IGNORE_THIS
//=============================================================================
