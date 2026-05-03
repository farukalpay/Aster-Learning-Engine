
//=============================================================================
//
//  Implements a writer module for OFF files
//
//=============================================================================


#ifndef __OFFWRITER_HH__
#define __OFFWRITER_HH__


//=== INCLUDES ================================================================

#include <string>
#include <ostream>

#include <AsterTopology/Core/System/config.h>
#include <AsterTopology/Core/Utils/SingletonT.hh>
#include <AsterTopology/Core/IO/exporter/BaseExporter.hh>
#include <AsterTopology/Core/IO/writer/BaseWriter.hh>


//== NAMESPACES ===============================================================


namespace AsterTopology {
namespace IO {


//=== IMPLEMENTATION ==========================================================


/**
    Implementation of the OFF format writer. This class is singleton'ed by
    SingletonT to OFFWriter.

    By passing Options to the write function you can manipulate the writing behavoir.
    The following options can be set:

    Binary
    VertexNormal
    VertexColor
    VertexTexCoord
    FaceColor
    ColorAlpha

*/
class ASTER_TOPOLOGYDLLEXPORT _OFFWriter_ : public BaseWriter
{
public:

  _OFFWriter_();

  virtual ~_OFFWriter_() {};

  std::string get_description() const { return "no description"; }
  std::string get_extensions() const  { return "off"; }

  bool write(const std::string&, BaseExporter&, Options, std::streamsize _precision = 6) const;

  bool write(std::ostream&, BaseExporter&, Options, std::streamsize _precision = 6) const;

  size_t binary_size(BaseExporter& _be, Options _opt) const;


protected:
  void writeValue(std::ostream& _out, int value) const;
  void writeValue(std::ostream& _out, unsigned int value) const;
  void writeValue(std::ostream& _out, float value) const;

  bool write_ascii(std::ostream& _in, BaseExporter&, Options) const;
  bool write_binary(std::ostream& _in, BaseExporter&, Options) const;
};


//== TYPE DEFINITION ==========================================================


/// Declare the single entity of the OFF writer.
extern _OFFWriter_  __OFFWriterInstance;
ASTER_TOPOLOGYDLLEXPORT _OFFWriter_& OFFWriter();


//=============================================================================
} // namespace IO
} // namespace AsterTopology
//=============================================================================
#endif
//=============================================================================
