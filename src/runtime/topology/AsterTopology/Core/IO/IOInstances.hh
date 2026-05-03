
//=============================================================================
//
//  Helper file for static builds
//
//  In opposite to dynamic builds where the instance of every reader module
//  is generated within the AsterTopology library, static builds only instanciate
//  objects that are at least referenced once. As all reader modules are
//  never used directly, they will not be part of a static build, hence
//  this file.
//
//=============================================================================


#ifndef __IOINSTANCES_HH__
#define __IOINSTANCES_HH__

#if defined(OM_STATIC_BUILD) || defined(ARCH_DARWIN)

//=============================================================================

#include <AsterTopology/Core/System/config.h>

#include <AsterTopology/Core/IO/reader/BaseReader.hh>
#include <AsterTopology/Core/IO/reader/OBJReader.hh>
#include <AsterTopology/Core/IO/reader/OFFReader.hh>
#include <AsterTopology/Core/IO/reader/PLYReader.hh>
#include <AsterTopology/Core/IO/reader/STLReader.hh>
#include <AsterTopology/Core/IO/reader/OMReader.hh>

#include <AsterTopology/Core/IO/writer/BaseWriter.hh>
#include <AsterTopology/Core/IO/writer/OBJWriter.hh>
#include <AsterTopology/Core/IO/writer/OFFWriter.hh>
#include <AsterTopology/Core/IO/writer/STLWriter.hh>
#include <AsterTopology/Core/IO/writer/OMWriter.hh>
#include <AsterTopology/Core/IO/writer/PLYWriter.hh>
#include <AsterTopology/Core/IO/writer/VTKWriter.hh>

//=== NAMESPACES ==============================================================

namespace AsterTopology {
namespace IO {

//=============================================================================


// Instanciate every Reader module
static BaseReader* OFFReaderInstance = &OFFReader();
static BaseReader* OBJReaderInstance = &OBJReader();
static BaseReader* PLYReaderInstance = &PLYReader();
static BaseReader* STLReaderInstance = &STLReader();
static BaseReader* OMReaderInstance  = &OMReader();

// Instanciate every writer module
static BaseWriter* OBJWriterInstance = &OBJWriter();
static BaseWriter* OFFWriterInstance = &OFFWriter();
static BaseWriter* STLWriterInstance = &STLWriter();
static BaseWriter* OMWriterInstance  = &OMWriter();
static BaseWriter* PLYWriterInstance = &PLYWriter();
static BaseWriter* VTKWriterInstance = &VTKWriter();


//=============================================================================
} // namespace IO
} // namespace AsterTopology
//=============================================================================
#endif // static ?
#endif //__IOINSTANCES_HH__
//=============================================================================
