//=============================================================================
//
//  Implements an IOManager writer module for VTK files
//
//=============================================================================

#ifndef __VTKWRITER_HH__
#define __VTKWRITER_HH__

//=== INCLUDES ================================================================

#include <string>
#include <iosfwd>

#include <AsterTopology/Core/System/config.h>
#include <AsterTopology/Core/Utils/SingletonT.hh>
#include <AsterTopology/Core/IO/exporter/BaseExporter.hh>
#include <AsterTopology/Core/IO/writer/BaseWriter.hh>

//== NAMESPACES ===============================================================

namespace AsterTopology {
namespace IO {

//=== IMPLEMENTATION ==========================================================

class ASTER_TOPOLOGYDLLEXPORT _VTKWriter_ : public BaseWriter
{
public:
    _VTKWriter_();

    std::string get_description() const { return "VTK"; }
    std::string get_extensions()  const { return "vtk"; }

    bool write(const std::string&, BaseExporter&, Options, std::streamsize _precision = 6) const;
    bool write(std::ostream&, BaseExporter&, Options, std::streamsize _precision = 6) const;

    size_t binary_size(BaseExporter&, Options) const { return 0; }
};

//== TYPE DEFINITION ==========================================================

/// Declare the single entity of the OBJ writer
extern _VTKWriter_  __VTKWriterinstance;
ASTER_TOPOLOGYDLLEXPORT _VTKWriter_& VTKWriter();

//=============================================================================
} // namespace IO
} // namespace AsterTopology
//=============================================================================
#endif
//=============================================================================
