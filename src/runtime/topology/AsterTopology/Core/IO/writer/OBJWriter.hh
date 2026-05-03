
//=============================================================================
//
//  Implements an IOManager writer module for OBJ files
//
//=============================================================================


#ifndef __OBJWRITER_HH__
#define __OBJWRITER_HH__


//=== INCLUDES ================================================================


#include <string>
#include <fstream>

#include <AsterTopology/Core/System/config.h>
#include <AsterTopology/Core/Utils/SingletonT.hh>
#include <AsterTopology/Core/IO/exporter/BaseExporter.hh>
#include <AsterTopology/Core/IO/writer/BaseWriter.hh>


//== NAMESPACES ===============================================================


namespace AsterTopology {
namespace IO {


//=== IMPLEMENTATION ==========================================================


/**
    This class defines the OBJ writer. This class is further singleton'ed
    by SingletonT to OBJWriter.
*/
class ASTER_TOPOLOGYDLLEXPORT _OBJWriter_ : public BaseWriter
{
public:

  _OBJWriter_();

  /// Destructor
  virtual ~_OBJWriter_() {};

  std::string get_description() const  { return "Alias/Wavefront"; }
  std::string get_extensions()  const  { return "obj"; }

  bool write(const std::string&, BaseExporter&, Options, std::streamsize _precision = 6) const;

  bool write(std::ostream&, BaseExporter&, Options, std::streamsize _precision = 6) const;

  size_t binary_size(BaseExporter&, Options) const { return 0; }

private:

  mutable std::string path_;
  mutable std::string objName_;

  mutable std::vector< AsterTopology::Vec3f > material_;
  mutable std::vector< AsterTopology::Vec4f > materialA_;

  size_t getMaterial(AsterTopology::Vec3f _color) const;

  size_t getMaterial(AsterTopology::Vec4f _color) const;

  bool writeMaterial(std::ostream& _out, BaseExporter&, Options) const;


};


//== TYPE DEFINITION ==========================================================


/// Declare the single entity of the OBJ writer
extern _OBJWriter_  __OBJWriterinstance;
ASTER_TOPOLOGYDLLEXPORT _OBJWriter_& OBJWriter();


//=============================================================================
} // namespace IO
} // namespace AsterTopology
//=============================================================================
#endif
//=============================================================================
