#ifndef ASTER_TOPOLOGY_CASTS_HH
#define ASTER_TOPOLOGY_CASTS_HH
//== INCLUDES =================================================================

#include <AsterTopology/Core/Mesh/PolyMesh_ArrayKernelT.hh>
#include <AsterTopology/Core/Mesh/TriMesh_ArrayKernelT.hh>

//== NAMESPACES ===============================================================
namespace AsterTopology 
{

template <class Traits>
inline TriMesh_ArrayKernelT<Traits>& TRIMESH_CAST(PolyMesh_ArrayKernelT<Traits>& _poly_mesh)
{ return reinterpret_cast< TriMesh_ArrayKernelT<Traits>& >(_poly_mesh); }

template <class Traits>
inline const TriMesh_ArrayKernelT<Traits>& TRIMESH_CAST(const PolyMesh_ArrayKernelT<Traits>& _poly_mesh)
{ return reinterpret_cast< const TriMesh_ArrayKernelT<Traits>& >(_poly_mesh); }

template <class Traits>
inline PolyMesh_ArrayKernelT<Traits>& POLYMESH_CAST(TriMesh_ArrayKernelT<Traits>& _tri_mesh)
{ return reinterpret_cast< PolyMesh_ArrayKernelT<Traits>& >(_tri_mesh); }

template <class Traits>
inline const PolyMesh_ArrayKernelT<Traits>& POLYMESH_CAST(const TriMesh_ArrayKernelT<Traits>& _tri_mesh)
{ return reinterpret_cast< const PolyMesh_ArrayKernelT<Traits>& >(_tri_mesh); }

};
#endif//ASTER_TOPOLOGY_CASTS_HH
