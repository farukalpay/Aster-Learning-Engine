
//=============================================================================
//
//  Helper Functions for binary reading / writing
//
//=============================================================================

#ifndef ASTER_TOPOLOGY_STORERESTORE_HH
#define ASTER_TOPOLOGY_STORERESTORE_HH


//== INCLUDES =================================================================

#include <stdexcept>
#include <AsterTopology/Core/System/config.h>
#include <AsterTopology/Core/IO/SR_binary.hh>
#include <AsterTopology/Core/IO/SR_binary_spec.hh>

//== NAMESPACES ===============================================================

namespace AsterTopology {
namespace IO {


//=============================================================================


/** \name Handling binary input/output.
    These functions take care of swapping bytes to get the right Endian.
*/
//@{


//-----------------------------------------------------------------------------
// StoreRestore definitions

template <typename T> inline
bool is_streamable(void)
{ return binary< T >::is_streamable; }

template <typename T> inline
bool is_streamable( const T& ) 
{ return binary< T >::is_streamable; }

template <typename T> inline
size_t size_of( const T& _v ) 
{ return binary< T >::size_of(_v); }

template <typename T> inline
size_t size_of(void) 
{ return binary< T >::size_of(); }

template <typename T> inline
size_t store( std::ostream& _os, const T& _v, bool _swap=false)
{ return binary< T >::store( _os, _v, _swap ); }

template <typename T> inline
size_t restore( std::istream& _is, T& _v, bool _swap=false)
{ return binary< T >::restore( _is, _v, _swap ); }

//@}


//=============================================================================
} // namespace IO
} // namespace AsterTopology
//=============================================================================
#endif // ASTER_TOPOLOGY_MESHREADER_HH defined
//=============================================================================
