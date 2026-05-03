#ifndef ASTER_TOPOLOGY_ITERATORS_HH
#define ASTER_TOPOLOGY_ITERATORS_HH

//=============================================================================
//
//  Iterators for PolyMesh/TriMesh
//
//=============================================================================



//== INCLUDES =================================================================

#include <AsterTopology/Core/System/config.h>
#include <AsterTopology/Core/Attributes/Status.hh>
#include <cassert>


//== NAMESPACES ===============================================================

namespace AsterTopology {
namespace Iterators {


//== FORWARD DECLARATIONS =====================================================


template <class Mesh> class VertexIterT;
template <class Mesh> class ConstVertexIterT;
template <class Mesh> class HalfedgeIterT;
template <class Mesh> class ConstHalfedgeIterT;
template <class Mesh> class EdgeIterT;
template <class Mesh> class ConstEdgeIterT;
template <class Mesh> class FaceIterT;
template <class Mesh> class ConstFaceIterT;




