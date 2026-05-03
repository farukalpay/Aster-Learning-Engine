//=============================================================================
//
//  AsterTopology streams: omlog, omout, omerr
//
//=============================================================================

#ifndef ASTER_TOPOLOGY_OMSTREAMS_HH
#define ASTER_TOPOLOGY_OMSTREAMS_HH


//== INCLUDES =================================================================

#include <AsterTopology/Core/System/mostream.hh>


//== CLASS DEFINITION =========================================================

/** \file omstream.hh
    This file provides the streams omlog, omout, and omerr.
*/

/** \name stream replacements
    These stream provide replacements for clog, cout, and cerr. They have
    the advantage that they can easily be multiplexed.
    \see AsterTopology::mostream
*/
//@{
ASTER_TOPOLOGYDLLEXPORT AsterTopology::mostream& omlog();
ASTER_TOPOLOGYDLLEXPORT AsterTopology::mostream& omout();
ASTER_TOPOLOGYDLLEXPORT AsterTopology::mostream& omerr();
//@}

//=============================================================================
#endif // ASTER_TOPOLOGY_OMSTREAMS_HH defined
//=============================================================================
