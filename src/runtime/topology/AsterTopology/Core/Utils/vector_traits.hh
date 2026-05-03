
//=============================================================================
//
//  Helper Functions for binary reading / writing
//
//=============================================================================


#ifndef ASTER_TOPOLOGY_VECTOR_TRAITS_HH
#define ASTER_TOPOLOGY_VECTOR_TRAITS_HH


//== INCLUDES =================================================================

#include <AsterTopology/Core/System/config.h>
#include <AsterTopology/Core/Utils/GenProg.hh>
#if defined(OM_CC_MIPS)
#  include <stdlib.h>
#else
#  include <cstdlib>
#endif

//== NAMESPACES ===============================================================


namespace AsterTopology {


//=============================================================================


/** \name Provide a standardized access to relevant information about a
     vector type.
*/
//@{

//-----------------------------------------------------------------------------

/** Helper class providing information about a vector type.
 *
 * If want to use a different vector type than the one provided %AsterTopology
 * you need to supply a specialization of this class for the new vector type.
 */
template <typename T>
struct vector_traits
{
  /// Type of the vector class
  typedef typename T::vector_type vector_type;

  /// Type of the scalar value
  typedef typename T::value_type  value_type;

  /// size/dimension of the vector
  static const size_t size_ = T::size_;

  /// size/dimension of the vector
  static size_t size() { return size_; }
};

//@}


//=============================================================================
} // namespace AsterTopology
//=============================================================================
#endif // ASTER_TOPOLOGY_MESHREADER_HH defined
//=============================================================================
