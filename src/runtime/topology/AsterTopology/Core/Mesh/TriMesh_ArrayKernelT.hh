
//=============================================================================
//
//  CLASS TriMesh_ArrayKernelT
//
//=============================================================================


#ifndef ASTER_TOPOLOGY_TRIMESH_ARRAY_KERNEL_HH
#define ASTER_TOPOLOGY_TRIMESH_ARRAY_KERNEL_HH


//== INCLUDES =================================================================


#include <AsterTopology/Core/System/config.h>
#include <AsterTopology/Core/Mesh/TriConnectivity.hh>
#include <AsterTopology/Core/Mesh/Traits.hh>
#include <AsterTopology/Core/Mesh/FinalMeshItemsT.hh>
#include <AsterTopology/Core/Mesh/AttribKernelT.hh>
#include <AsterTopology/Core/Mesh/TriMeshT.hh>


//== NAMESPACES ===============================================================


namespace AsterTopology {

template<class Traits>
class PolyMesh_ArrayKernelT;
//== CLASS DEFINITION =========================================================


/// Helper class to create a TriMesh-type based on ArrayKernelT
template <class Traits>
struct TriMesh_ArrayKernel_GeneratorT
{
  typedef FinalMeshItemsT<Traits, true>               MeshItems;
  typedef AttribKernelT<MeshItems, TriConnectivity>   AttribKernel;
  typedef TriMeshT<AttribKernel>                      Mesh;
};



/** \ingroup mesh_types_group
    Triangle mesh based on the ArrayKernel.
    \see AsterTopology::TriMeshT
    \see AsterTopology::ArrayKernelT
*/
template <class Traits = DefaultTraits>
class TriMesh_ArrayKernelT
  : public TriMesh_ArrayKernel_GeneratorT<Traits>::Mesh
{
public:
  TriMesh_ArrayKernelT() {}
  template<class OtherTraits>
   TriMesh_ArrayKernelT( const PolyMesh_ArrayKernelT<OtherTraits> & t)
  {
     //assign the connectivity and standard properties
     this->assign(t,true);
  }
};


//=============================================================================
} // namespace AsterTopology
//=============================================================================
#endif // ASTER_TOPOLOGY_TRIMESH_ARRAY_KERNEL_HH
//=============================================================================
