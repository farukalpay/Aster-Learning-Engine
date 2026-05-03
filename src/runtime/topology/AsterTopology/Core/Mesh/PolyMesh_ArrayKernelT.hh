
//=============================================================================
//
//  CLASS PolyMesh_ArrayKernelT
//
//=============================================================================


#ifndef ASTER_TOPOLOGY_POLY_MESH_ARRAY_KERNEL_HH
#define ASTER_TOPOLOGY_POLY_MESH_ARRAY_KERNEL_HH


//== INCLUDES =================================================================


#include <AsterTopology/Core/System/config.h>
#include <AsterTopology/Core/Mesh/PolyConnectivity.hh>
#include <AsterTopology/Core/Mesh/Traits.hh>
#include <AsterTopology/Core/Mesh/FinalMeshItemsT.hh>
#include <AsterTopology/Core/Mesh/AttribKernelT.hh>
#include <AsterTopology/Core/Mesh/PolyMeshT.hh>


//== NAMESPACES ===============================================================


namespace AsterTopology {

template<class Traits>
class TriMesh_ArrayKernelT;
//== CLASS DEFINITION =========================================================

/// Helper class to build a PolyMesh-type
template <class Traits>
struct PolyMesh_ArrayKernel_GeneratorT
{
  typedef FinalMeshItemsT<Traits, false>              MeshItems;
  typedef AttribKernelT<MeshItems, PolyConnectivity>  AttribKernel;
  typedef PolyMeshT<AttribKernel>                     Mesh;
};


/** \class PolyMesh_ArrayKernelT PolyMesh_ArrayKernelT.hh <AsterTopology/Mesh/Types/PolyMesh_ArrayKernelT.hh>

    \ingroup mesh_types_group
    Polygonal mesh based on the ArrayKernel.
    \see AsterTopology::PolyMeshT
    \see AsterTopology::ArrayKernel
*/
template <class Traits = DefaultTraits>
class PolyMesh_ArrayKernelT
  : public PolyMesh_ArrayKernel_GeneratorT<Traits>::Mesh
{
public:
  PolyMesh_ArrayKernelT() {}
  template<class OtherTraits>
   PolyMesh_ArrayKernelT( const TriMesh_ArrayKernelT<OtherTraits> & t)
  {
     //assign the connectivity and standard properties
     this->assign(t, true);

  }
};


//=============================================================================
} // namespace AsterTopology
//=============================================================================
#endif // ASTER_TOPOLOGY_POLY_MESH_ARRAY_KERNEL_HH
//=============================================================================
