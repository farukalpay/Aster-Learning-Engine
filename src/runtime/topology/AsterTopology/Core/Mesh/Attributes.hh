
/**
    \file Attributes.hh
    This file provides some macros containing attribute usage.
*/


#ifndef ASTER_TOPOLOGY_ATTRIBUTES_HH
#define ASTER_TOPOLOGY_ATTRIBUTES_HH


//== INCLUDES =================================================================


#include <AsterTopology/Core/System/config.h>
#include <AsterTopology/Core/Mesh/Status.hh>


//== NAMESPACES ===============================================================


namespace AsterTopology {
namespace Attributes {


//== CLASS DEFINITION  ========================================================

/** Attribute bits
 *
 *  Use the bits to define a standard property at compile time using traits.
 *
 *  \include traits5.cc
 *
 *  \see \ref mesh_type
 */
enum AttributeBits
{
  None          = 0,  ///< Clear all attribute bits
  Normal        = 1,  ///< Add normals to mesh item (vertices/faces)
  Color         = 2,  ///< Add colors to mesh item (vertices/faces/edges)
  PrevHalfedge  = 4,  ///< Add storage for previous halfedge (halfedges). The bit is set by default in the DefaultTraits.
  Status        = 8,  ///< Add status to mesh item (all items)
  TexCoord1D    = 16, ///< Add 1D texture coordinates (vertices, halfedges)
  TexCoord2D    = 32, ///< Add 2D texture coordinates (vertices, halfedges)
  TexCoord3D    = 64, ///< Add 3D texture coordinates (vertices, halfedges)
  TextureIndex  = 128 ///< Add texture index (faces)
};


//=============================================================================
} // namespace Attributes
} // namespace AsterTopology
//=============================================================================
#endif // ASTER_TOPOLOGY_ATTRIBUTES_HH defined
//=============================================================================
