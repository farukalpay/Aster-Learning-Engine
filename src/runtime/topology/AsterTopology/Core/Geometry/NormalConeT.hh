
/*===========================================================================*\
 *                                                                           *
 *   $Revision$                                                         *
 *   $Date$                   *
 *                                                                           *
\*===========================================================================*/




//=============================================================================
//
//  CLASS NormalCone
//
//=============================================================================


#ifndef ASTER_TOPOLOGY_NORMALCONE_HH
#define ASTER_TOPOLOGY_NORMALCONE_HH


//== INCLUDES =================================================================


#include <AsterTopology/Core/Geometry/VectorT.hh>


//== NAMESPACES ===============================================================


namespace AsterTopology {


//== CLASS DEFINITION =========================================================


/** /class NormalCone NormalCone.hh <ACG/Geometry/Types/NormalCone.hh>

    NormalCone that can be merged with other normal cones. Provides
    the center normal and the opening angle.
**/

template <typename Scalar>
class NormalConeT
{
public:

  // typedefs
  typedef VectorT<Scalar, 3>  Vec3;


  //! default constructor (not initialized)
  NormalConeT() {}

  //! Initialize cone with center (unit vector) and angle (radius in radians)
  NormalConeT(const Vec3& _center_normal, Scalar _angle=0.0);

  //! return max. distance (radians) unit vector to cone (distant side)
  Scalar max_angle(const Vec3&) const;

  //! return max. distance (radians) cone to cone (distant sides)
  Scalar max_angle(const NormalConeT&) const;

  //! merge _cone; this instance will then enclose both former cones
  void merge(const NormalConeT&);

  //! returns center normal
  const Vec3& center_normal() const { return center_normal_; }

  //! returns size of cone (radius in radians)
  inline Scalar angle() const { return angle_; }

private:

  Vec3    center_normal_;
  Scalar  angle_;
};


//=============================================================================
} // namespace AsterTopology
//=============================================================================
#if defined(OM_INCLUDE_TEMPLATES) && !defined(ASTER_TOPOLOGY_NORMALCONE_C)
#define ASTER_TOPOLOGY_NORMALCONE_TEMPLATES
#include "NormalConeT.cc"
#endif
//=============================================================================
#endif // ASTER_TOPOLOGY_NORMALCONE_HH defined
//=============================================================================

