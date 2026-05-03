
//=============================================================================
//
//  Implements a writer module for STL ascii files
//
//=============================================================================
// $Id: STLWriter.hh,v 1.2 2007-05-18 15:17:43 habbecke Exp $

#ifndef __STLWRITER_HH__
#define __STLWRITER_HH__


//=== INCLUDES ================================================================

// -------------------- STL
#include <iosfwd>
#include <string>
// -------------------- AsterTopology
#include <AsterTopology/Core/System/config.h>
#include <AsterTopology/Core/Utils/SingletonT.hh>
#include <AsterTopology/Core/IO/exporter/BaseExporter.hh>
#include <AsterTopology/Core/IO/writer/BaseWriter.hh>


//== NAMESPACES ===============================================================


namespace AsterTopology {
namespace IO {


//=== IMPLEMENTATION ==========================================================


/**
    Implementation of the STL format writer. This class is singleton'ed by
    SingletonT to STLWriter.
*/
class ASTER_TOPOLOGYDLLEXPORT _STLWriter_ : public BaseWriter
{
public:

  _STLWriter_();

  /// Destructor
  virtual ~_STLWriter_() {};

  std::string get_description() const { return "Stereolithography Format"; }
  std::string get_extensions()  const { return "stl stla stlb"; }

  bool write(const std::string&, BaseExporter&, Options, std::streamsize _precision = 6) const;

  bool write(std::ostream&, BaseExporter&, Options, std::streamsize _precision = 6) const;

  size_t binary_size(BaseExporter&, Options) const;

private:
  bool write_stla(const std::string&, BaseExporter&, Options) const;
  bool write_stla(std::ostream&, BaseExporter&, Options, std::streamsize _precision = 6) const;
  bool write_stlb(const std::string&, BaseExporter&, Options) const;
  bool write_stlb(std::ostream&, BaseExporter&, Options, std::streamsize _precision = 6) const;
};


//== TYPE DEFINITION ==========================================================


// Declare the single entity of STL writer.
extern _STLWriter_  __STLWriterInstance;
ASTER_TOPOLOGYDLLEXPORT _STLWriter_& STLWriter();


//=============================================================================
} // namespace IO
} // namespace AsterTopology
//=============================================================================
#endif
//=============================================================================
