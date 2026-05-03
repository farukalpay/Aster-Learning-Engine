
//=============================================================================
//
//  Implements a writer module for PLY files
//
//=============================================================================


#ifndef __PLYWRITER_HH__
#define __PLYWRITER_HH__


//=== INCLUDES ================================================================

#include <string>
#include <ostream>
#include <vector>

#include <AsterTopology/Core/System/config.h>
#include <AsterTopology/Core/Utils/SingletonT.hh>
#include <AsterTopology/Core/IO/exporter/BaseExporter.hh>
#include <AsterTopology/Core/IO/writer/BaseWriter.hh>
#include <AsterTopology/Core/Utils/GenProg.hh>


//== NAMESPACES ===============================================================


namespace AsterTopology {
namespace IO {


//=== IMPLEMENTATION ==========================================================


/**
    Implementation of the PLY format writer. This class is singleton'ed by
    SingletonT to PLYWriter.

    currently supported options:
    - VertexColors
    - Binary
    - Binary -> MSB
*/
class ASTER_TOPOLOGYDLLEXPORT _PLYWriter_ : public BaseWriter
{
public:

  _PLYWriter_();

  /// Destructor
  virtual ~_PLYWriter_() {};

  std::string get_description() const { return "PLY polygon file format"; }
  std::string get_extensions() const  { return "ply"; }

  bool write(const std::string&, BaseExporter&, Options, std::streamsize _precision = 6) const;

  bool write(std::ostream&, BaseExporter&, Options, std::streamsize _precision = 6) const;

  size_t binary_size(BaseExporter& _be, Options _opt) const;

  enum ValueType {
    Unsupported = 0,
    ValueTypeFLOAT32, ValueTypeFLOAT,
    ValueTypeINT32, ValueTypeINT , ValueTypeUINT,
    ValueTypeUCHAR, ValueTypeCHAR, ValueTypeUINT8,
    ValueTypeUSHORT, ValueTypeSHORT,
    ValueTypeDOUBLE
  };

private:
  mutable Options options_;

  struct CustomProperty
  {
    ValueType type;
    const BaseProperty*  property;
    CustomProperty(const BaseProperty* const _p):type(Unsupported),property(_p){}
  };

  const char* nameOfType_[12];

  /// write custom persistant properties into the header for the current element, returns all properties, which were written sorted
  std::vector<CustomProperty> writeCustomTypeHeader(std::ostream& _out, BaseKernel::const_prop_iterator _begin, BaseKernel::const_prop_iterator _end) const;
  template<bool binary>
  void write_customProp(std::ostream& _our, const CustomProperty& _prop, size_t _index) const;
  template<typename T>
  void writeProxy(ValueType _type, std::ostream& _out, T _value, AsterTopology::GenProg::TrueType /*_binary*/) const
  {
    writeValue(_type, _out, _value);
  }
  template<typename T>
  void writeProxy(ValueType _type, std::ostream& _out, T _value, AsterTopology::GenProg::FalseType /*_binary*/) const
  {
    _out << " " << _value;
  }

protected:
  void writeValue(ValueType _type, std::ostream& _out, signed char value) const;
  void writeValue(ValueType _type, std::ostream& _out, unsigned char value) const;
  void writeValue(ValueType _type, std::ostream& _out, short value) const;
  void writeValue(ValueType _type, std::ostream& _out, unsigned short value) const;
  void writeValue(ValueType _type, std::ostream& _out, int value) const;
  void writeValue(ValueType _type, std::ostream& _out, unsigned int value) const;
  void writeValue(ValueType _type, std::ostream& _out, float value) const;
  void writeValue(ValueType _type, std::ostream& _out, double value) const;

  bool write_ascii(std::ostream& _out, BaseExporter&, Options) const;
  bool write_binary(std::ostream& _out, BaseExporter&, Options) const;
  /// write header into the stream _out. Returns custom properties (vertex and face) which are written into the header
  void write_header(std::ostream& _out, BaseExporter& _be, Options& _opt, std::vector<CustomProperty>& _ovProps, std::vector<CustomProperty>& _ofProps) const;
};


//== TYPE DEFINITION ==========================================================


/// Declare the single entity of the PLY writer.
extern _PLYWriter_  __PLYWriterInstance;
ASTER_TOPOLOGYDLLEXPORT _PLYWriter_& PLYWriter();


//=============================================================================
} // namespace IO
} // namespace AsterTopology
//=============================================================================
#endif
//=============================================================================
