
//=============================================================================
//
//  CLASS BaseMesh
//
//=============================================================================


#ifndef ASTER_TOPOLOGY_BASEMESH_HH
#define ASTER_TOPOLOGY_BASEMESH_HH


//== INCLUDES =================================================================


#include <AsterTopology/Core/System/config.h>
#include <AsterTopology/Core/Mesh/IteratorsT.hh>
#include <AsterTopology/Core/Mesh/CirculatorsT.hh>
#include <AsterTopology/Core/Mesh/Attributes.hh>
#include <vector>


//== NAMESPACES ===============================================================


namespace AsterTopology {


//== CLASS DEFINITION =========================================================


/** \class BaseMesh BaseMesh.hh <AsterTopology/Mesh/BaseMesh.hh>

    Base class for all meshes.
*/

class BaseMesh {
public:
  virtual ~BaseMesh(void) {;}
};


//=============================================================================
} // namespace AsterTopology
//=============================================================================

//=============================================================================
#endif // ASTER_TOPOLOGY_BASEMESH_HH defined
//=============================================================================
