
//=============================================================================
//
//  Helper Functions for binary reading / writing
//
//=============================================================================


#ifndef ASTER_TOPOLOGY_UTILS_ENDIAN_HH
#define ASTER_TOPOLOGY_UTILS_ENDIAN_HH


//== INCLUDES =================================================================


#include <AsterTopology/Core/System/config.h>


//== NAMESPACES ===============================================================


namespace AsterTopology {


//=============================================================================


/**  Determine byte order of host system.
 */
class ASTER_TOPOLOGYDLLEXPORT Endian
{
public:
   
  enum Type {
    LSB = 1, ///< Little endian (Intel family and clones)
    MSB      ///< big endian (Motorola's 68x family, DEC Alpha, MIPS)
  };
  
  /// Return endian type of host system.
  static Type local() { return local_; }

  /// Return type _t as string.
  static const char * as_string(Type _t);
   
private:
   static int one_;
   static const Type local_;
};

//=============================================================================
} // namespace AsterTopology
//=============================================================================
#endif // ASTER_TOPOLOGY_MESHREADER_HH defined
//=============================================================================

