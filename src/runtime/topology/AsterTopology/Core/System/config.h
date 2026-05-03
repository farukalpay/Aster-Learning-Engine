/** \file config.h
 *  \todo Move content to config.hh and include it to be compatible with old
 *  source.
 */

//=============================================================================

#ifndef ASTER_TOPOLOGY_CONFIG_H
#define ASTER_TOPOLOGY_CONFIG_H

//=============================================================================

#include <assert.h>
#include <AsterTopology/Core/System/compiler.hh>
#include <AsterTopology/Core/System/AsterTopologyDLLMacros.hh>

// ----------------------------------------------------------------------------


#define OM_VERSION 0x80000
//#define OM_VERSION 0x70200

#define OM_GET_VER ((OM_VERSION & 0xf0000) >> 16)
#define OM_GET_MAJ ((OM_VERSION & 0x0ff00) >> 8)
#define OM_GET_MIN  (OM_VERSION & 0x000ff)

#ifdef WIN32
#  ifdef min
#    pragma message("Detected min macro! AsterTopology does not compile with min/max macros active! Please add a define NOMINMAX to your compiler flags or add #undef min before including AsterTopology headers !")
#    error min macro active 
#  endif
#  ifdef max
#    pragma message("Detected max macro! AsterTopology does not compile with min/max macros active! Please add a define NOMINMAX to your compiler flags or add #undef max before including AsterTopology headers !")
#    error max macro active 
#  endif
#endif

#if defined(_MSC_VER)
#  define OM_DEPRECATED(msg) __declspec(deprecated(msg))
#elif defined(__GNUC__)
#  if (__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__) >= 40500 /* Test for GCC >= 4.5.0 */
#    define OM_DEPRECATED(msg) __attribute__ ((deprecated(msg)))
#  else
#    define OM_DEPRECATED(msg) __attribute__ ((deprecated))
#  endif
#elif defined(__clang__)
#  define OM_DEPRECATED(msg) __attribute__ ((deprecated(msg)))
#else
#  define OM_DEPRECATED(msg)
#endif

typedef unsigned int uint;

#if ((defined(_MSC_VER) && (_MSC_VER >= 1800)) || __cplusplus > 199711L || defined(__GXX_EXPERIMENTAL_CXX0X__))
#define OM_HAS_HASH
#endif

//=============================================================================
#endif // ASTER_TOPOLOGY_CONFIG_H defined
//=============================================================================
