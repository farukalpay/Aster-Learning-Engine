
//=============================================================================
//
//  CLASS TriMeshT - IMPLEMENTATION
//
//=============================================================================


#define ASTER_TOPOLOGY_TRIMESH_C


//== INCLUDES =================================================================


#include <AsterTopology/Core/Mesh/TriMeshT.hh>
#include <AsterTopology/Core/System/omstream.hh>
#include <vector>


//== NAMESPACES ==============================================================


namespace AsterTopology {


//== IMPLEMENTATION ==========================================================

template <class Kernel>
typename TriMeshT<Kernel>::Normal
TriMeshT<Kernel>::
calc_face_normal(FaceHandle _fh) const
{
  assert(this->halfedge_handle(_fh).is_valid());
  ConstFaceVertexIter fv_it(this->cfv_iter(_fh));

  const Point& p0(this->point(*fv_it));  ++fv_it;
  const Point& p1(this->point(*fv_it));  ++fv_it;
  const Point& p2(this->point(*fv_it));

  return PolyMesh::calc_face_normal(p0, p1, p2);
}

//=============================================================================
} // namespace AsterTopology
//=============================================================================
