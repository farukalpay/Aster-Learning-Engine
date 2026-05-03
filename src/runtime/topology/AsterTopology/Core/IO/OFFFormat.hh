
#ifndef ASTER_TOPOLOGY_IO_OFFFORMAT_HH
#define ASTER_TOPOLOGY_IO_OFFFORMAT_HH


//=== INCLUDES ================================================================


// AsterTopology
#include <AsterTopology/Core/System/config.h>


//== NAMESPACES ==============================================================


namespace AsterTopology {
namespace IO   {


//=== IMPLEMENTATION ==========================================================


/** \name Mesh Reading / Writing
    Option for writer modules.
*/
//@{


//-----------------------------------------------------------------------------

#ifndef DOXY_IGNORE_THIS

struct ASTER_TOPOLOGYDLLEXPORT OFFFormat
{
   typedef int   integer_type;
   typedef float float_type;
};
   
#endif



//@}


//=============================================================================
} // namespace IO
} // namespace AsterTopology
//=============================================================================
#endif
//=============================================================================
