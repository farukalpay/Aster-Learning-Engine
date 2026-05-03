#ifndef ASTER_TOPOLOGY_ARRAY_ITEMS_HH
#define ASTER_TOPOLOGY_ARRAY_ITEMS_HH


//== INCLUDES =================================================================


#include <AsterTopology/Core/System/config.h>
#include <AsterTopology/Core/Utils/GenProg.hh>
#include <AsterTopology/Core/Mesh/Handles.hh>


//== NAMESPACES ===============================================================

namespace AsterTopology {


//== CLASS DEFINITION =========================================================


/// Definition of mesh items for use in the ArrayKernel
struct ArrayItems
{

  //------------------------------------------------------ internal vertex type

  /// The vertex item
  class Vertex
  {
    friend class ArrayKernel;
    HalfedgeHandle  halfedge_handle_;
  };


  //---------------------------------------------------- internal halfedge type

#ifndef DOXY_IGNORE_THIS
  class Halfedge_without_prev
  {
    friend class ArrayKernel;
    FaceHandle      face_handle_;
    VertexHandle    vertex_handle_;
    HalfedgeHandle  next_halfedge_handle_;
  };
#endif

#ifndef DOXY_IGNORE_THIS
  class Halfedge_with_prev : public Halfedge_without_prev
  {
    friend class ArrayKernel;
    HalfedgeHandle  prev_halfedge_handle_;
  };
#endif

  //TODO: should be selected with config.h define
  typedef Halfedge_with_prev                Halfedge;
  typedef GenProg::Bool2Type<true>          HasPrevHalfedge;

  //-------------------------------------------------------- internal edge type
#ifndef DOXY_IGNORE_THIS
  class Edge
  {
    friend class ArrayKernel;
    Halfedge  halfedges_[2];
  };
#endif

  //-------------------------------------------------------- internal face type
#ifndef DOXY_IGNORE_THIS
  class Face
  {
    friend class ArrayKernel;
    HalfedgeHandle  halfedge_handle_;
  };
};
#endif

//=============================================================================
} // namespace AsterTopology
//=============================================================================
#endif // ASTER_TOPOLOGY_ITEMS_HH defined
//=============================================================================
