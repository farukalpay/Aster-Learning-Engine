#ifndef ASTER_TOPOLOGY_CIRCULATORS_HH
#define ASTER_TOPOLOGY_CIRCULATORS_HH

//=============================================================================
//
//  Vertex and Face circulators for PolyMesh/TriMesh
//
//=============================================================================



//== INCLUDES =================================================================

#include <AsterTopology/Core/System/config.h>
#include <cassert>


//== NAMESPACES ===============================================================

namespace AsterTopology {
namespace Iterators {


//== FORWARD DECLARATIONS =====================================================


template <class Mesh> class VertexVertexIterT;
template <class Mesh> class VertexIHalfedgeIterT;
template <class Mesh> class VertexOHalfedgeIterT;
template <class Mesh> class VertexEdgeIterT;
template <class Mesh> class VertexFaceIterT;

template <class Mesh> class ConstVertexVertexIterT;
template <class Mesh> class ConstVertexIHalfedgeIterT;
template <class Mesh> class ConstVertexOHalfedgeIterT;
template <class Mesh> class ConstVertexEdgeIterT;
template <class Mesh> class ConstVertexFaceIterT;

template <class Mesh> class FaceVertexIterT;
template <class Mesh> class FaceHalfedgeIterT;
template <class Mesh> class FaceEdgeIterT;
template <class Mesh> class FaceFaceIterT;

template <class Mesh> class ConstFaceVertexIterT;
template <class Mesh> class ConstFaceHalfedgeIterT;
template <class Mesh> class ConstFaceEdgeIterT;
template <class Mesh> class ConstFaceFaceIterT;



