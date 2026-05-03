
//=============================================================================
//
//  Helper Functions for binary reading / writing
//
//=============================================================================

#ifndef ASTER_TOPOLOGY_SR_TYPES_HH
#define ASTER_TOPOLOGY_SR_TYPES_HH


//== INCLUDES =================================================================

#include <AsterTopology/Core/System/config.h>


//== NAMESPACES ===============================================================

namespace AsterTopology {
namespace IO {


//=============================================================================


/** \name Handling binary input/output.
    These functions take care of swapping bytes to get the right Endian.
*/
//@{

//-----------------------------------------------------------------------------

typedef unsigned char    uchar;
typedef unsigned short   ushort;
typedef unsigned long    ulong;

typedef signed char    int8_t;  typedef unsigned char      uint8_t;
typedef short          int16_t; typedef unsigned short     uint16_t;

// Int should be 32 bit on all archs.
// long is 32 under windows but 64 under unix 64 bit
typedef int            int32_t; typedef unsigned int       uint32_t;
#if defined(OM_CC_MSVC)
typedef __int64        int64_t; typedef unsigned __int64   uint64_t;
#else
typedef long long      int64_t; typedef unsigned long long uint64_t;
#endif

typedef float          float32_t;
typedef double         float64_t;

typedef uint8_t        rgb_t[3];
typedef uint8_t        rgba_t[4];

//@}


//=============================================================================
} // namespace IO
} // namespace AsterTopology
//=============================================================================
#endif // ASTER_TOPOLOGY_MESHREADER_HH defined
//=============================================================================

