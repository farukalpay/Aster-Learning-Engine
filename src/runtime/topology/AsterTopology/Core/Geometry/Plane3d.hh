

/*===========================================================================*\
 *                                                                           *             
 *   $Revision$                                                         *
 *   $Date$                   *
 *                                                                           *
\*===========================================================================*/

//=============================================================================
//
//  CLASS Plane3D
//
//=============================================================================


#ifndef ASTER_TOPOLOGY_PLANE3D_HH
#define ASTER_TOPOLOGY_PLANE3D_HH


//== INCLUDES =================================================================

#include <AsterTopology/Core/Geometry/VectorT.hh>


//== FORWARDDECLARATIONS ======================================================

//== NAMESPACES ===============================================================

namespace AsterTopology {
namespace VDPM {

//== CLASS DEFINITION =========================================================

	      
/** \class Plane3d Plane3d.hh <AsterTopology/Tools/VDPM/Plane3d.hh>

    ax + by + cz + d = 0
*/


class ASTER_TOPOLOGYDLLEXPORT Plane3d
{
public:

  typedef AsterTopology::Vec3f         vector_type;
  typedef vector_type::value_type value_type;

public:

  Plane3d()
    : d_(0)
  { }

  Plane3d(const vector_type &_dir, const vector_type &_pnt)
    : n_(_dir), d_(0)
  { 
    n_.normalize();
    d_ = -dot(n_,_pnt); 
  }

  value_type signed_distance(const AsterTopology::Vec3f &_p)
  {
    return  dot(n_ , _p) + d_;
  }

  // back compatibility
  value_type singed_distance(const AsterTopology::Vec3f &point)
  { return signed_distance( point ); }

public:

  vector_type n_;
  value_type  d_;

};

//=============================================================================
} // namespace VDPM
} // namespace AsterTopology
//=============================================================================
#endif // ASTER_TOPOLOGY_PLANE3D_HH defined
//=============================================================================
