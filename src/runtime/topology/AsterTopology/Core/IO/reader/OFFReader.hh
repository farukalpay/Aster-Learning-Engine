
//=============================================================================
//
//  Implements a reader module for OFF files
//
//=============================================================================


#ifndef __OFFREADER_HH__
#define __OFFREADER_HH__


//=== INCLUDES ================================================================


#include <iosfwd>
#include <string>
#include <cstdio>

#include <AsterTopology/Core/System/config.h>
#include <AsterTopology/Core/Utils/SingletonT.hh>
#include <AsterTopology/Core/IO/reader/BaseReader.hh>


//== NAMESPACES ===============================================================


namespace AsterTopology {
namespace IO {


//== FORWARDS =================================================================


class BaseImporter;


//== IMPLEMENTATION ===========================================================


/**
    Implementation of the OFF format reader. This class is singleton'ed by
    SingletonT to OFFReader.

    By passing Options to the read function you can manipulate the reading behavoir.
    The following options can be set:

    VertexNormal
    VertexColor
    VertexTexCoord
    FaceColor
    ColorAlpha [only when reading binary]

    These options define if the corresponding data should be read (if available)
    or if it should be omitted.

    After execution of the read function. The options object contains information about
    what was actually read.

    e.g. if VertexNormal was true when the read function was called, but the file
    did not contain vertex normals then it is false afterwards.

    When reading a binary off with Color Flag in the header it is assumed that all vertices
    and faces have colors in the format "int int int".
    If ColorAlpha is set the format "int int int int" is assumed.

*/

class ASTER_TOPOLOGYDLLEXPORT _OFFReader_ : public BaseReader
{
public:

  _OFFReader_();

  /// Destructor
  virtual ~_OFFReader_() {};

  std::string get_description() const { return "Object File Format"; }
  std::string get_extensions()  const { return "off"; }
  std::string get_magic()       const { return "OFF"; }

  bool read(const std::string& _filename,
	    BaseImporter& _bi,
	    Options& _opt);

  bool can_u_read(const std::string& _filename) const;

  bool read(std::istream& _in, BaseImporter& _bi, Options& _opt );

private:

  bool can_u_read(std::istream& _is) const;

  bool read_ascii(std::istream& _in, BaseImporter& _bi, Options& _opt) const;
  bool read_binary(std::istream& _in, BaseImporter& _bi, Options& _opt, bool swap) const;

  void readValue(std::istream& _in, float& _value) const;
  void readValue(std::istream& _in, int& _value) const;
  void readValue(std::istream& _in, unsigned int& _value) const;

  int getColorType(std::string & _line, bool _texCoordsAvailable) const;

  //available options for reading
  mutable Options options_;
  //options that the user wants to read
  mutable Options userOptions_;
};


//== TYPE DEFINITION ==========================================================


/// Declare the single entity of the OFF reader
extern _OFFReader_  __OFFReaderInstance;
ASTER_TOPOLOGYDLLEXPORT _OFFReader_& OFFReader();


//=============================================================================
} // namespace IO
} // namespace AsterTopology
//=============================================================================
#endif
//=============================================================================
