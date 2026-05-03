
//=============================================================================
//
//  Implements a writer module for OM files
//
//=============================================================================


#ifndef __OMWRITER_HH__
#define __OMWRITER_HH__


//=== INCLUDES ================================================================


// STD C++
#include <iosfwd>
#include <string>

// AsterTopology
#include <AsterTopology/Core/IO/BinaryHelper.hh>
#include <AsterTopology/Core/System/config.h>
#include <AsterTopology/Core/Utils/SingletonT.hh>
#include <AsterTopology/Core/IO/OMFormat.hh>
#include <AsterTopology/Core/IO/IOManager.hh>
#include <AsterTopology/Core/IO/writer/BaseWriter.hh>


//== NAMESPACES ===============================================================


namespace AsterTopology {
namespace IO {

//=== FORWARDS ================================================================


class BaseExporter;


//=== IMPLEMENTATION ==========================================================


/**
 *  Implementation of the OM format writer. This class is singleton'ed by
 *  SingletonT to OMWriter.
 */
class ASTER_TOPOLOGYDLLEXPORT _OMWriter_ : public BaseWriter
{
public:

  /// Constructor
  _OMWriter_();

  /// Destructor
  virtual ~_OMWriter_() {};

  std::string get_description() const
  { return "AsterTopology Format"; }

  std::string get_extensions() const
  { return "om"; }

  bool write(std::ostream&, BaseExporter&, Options, std::streamsize _precision = 6) const;

  size_t binary_size(BaseExporter& _be, Options _opt) const;

  static OMFormat::uint8 get_version() { return version_; }


protected:

  static const OMFormat::uchar magic_[3];
  static const OMFormat::uint8 version_;

  bool write(const std::string&, BaseExporter&, Options, std::streamsize _precision = 6) const;

  bool write_binary(std::ostream&, BaseExporter&, Options) const;


  size_t store_binary_custom_chunk( std::ostream&, const BaseProperty&,
				    OMFormat::Chunk::Entity, bool) const;
};


//== TYPE DEFINITION ==========================================================


/// Declare the single entity of the OM writer.
extern _OMWriter_  __OMWriterInstance;
ASTER_TOPOLOGYDLLEXPORT _OMWriter_& OMWriter();


//=============================================================================
} // namespace IO
} // namespace AsterTopology
//=============================================================================
#endif
//=============================================================================
