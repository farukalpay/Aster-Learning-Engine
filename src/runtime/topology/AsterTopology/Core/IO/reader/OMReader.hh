
//=============================================================================
//
//  Implements a reader module for OFF files
//
//=============================================================================


#ifndef __OMREADER_HH__
#define __OMREADER_HH__


//=== INCLUDES ================================================================

// AsterTopology
#include <AsterTopology/Core/System/config.h>
#include <AsterTopology/Core/Utils/SingletonT.hh>
#include <AsterTopology/Core/IO/OMFormat.hh>
#include <AsterTopology/Core/IO/IOManager.hh>
#include <AsterTopology/Core/IO/importer/BaseImporter.hh>
#include <AsterTopology/Core/IO/reader/BaseReader.hh>

// STD C++
#include <iosfwd>
#include <string>


//== NAMESPACES ===============================================================


namespace AsterTopology {
namespace IO {


//== IMPLEMENTATION ===========================================================


/**
    Implementation of the OM format reader. This class is singleton'ed by
    SingletonT to OMReader.
*/
class ASTER_TOPOLOGYDLLEXPORT _OMReader_ : public BaseReader
{
public:

  _OMReader_();
  virtual ~_OMReader_() { }

  std::string get_description() const { return "AsterTopology File Format"; }
  std::string get_extensions()  const { return "om"; }
  std::string get_magic()       const { return "OM"; }

  bool read(const std::string& _filename,
	    BaseImporter& _bi,
	    Options& _opt );

//!  Stream Reader for std::istream input in binary format
  bool read(std::istream& _is,
	    BaseImporter& _bi,
	    Options& _opt );

  virtual bool can_u_read(const std::string& _filename) const;
  virtual bool can_u_read(std::istream& _is) const;


private:

  bool supports( const OMFormat::uint8 version ) const;

  bool read_ascii(std::istream& _is, BaseImporter& _bi, Options& _opt) const;
  bool read_binary(std::istream& _is, BaseImporter& _bi, Options& _opt) const;

  typedef OMFormat::Header              Header;
  typedef OMFormat::Chunk::Header       ChunkHeader;
  typedef OMFormat::Chunk::PropertyName PropertyName;

  // initialized/updated by read_binary*/read_ascii*
  mutable size_t       bytes_;
  mutable Options      fileOptions_;
  mutable Header       header_;
  mutable ChunkHeader  chunk_header_;
  mutable PropertyName property_name_;

  bool read_binary_vertex_chunk(   std::istream      &_is,
				   BaseImporter      &_bi,
				   Options           &_opt,
				   bool              _swap) const;

  bool read_binary_face_chunk(     std::istream      &_is,
			           BaseImporter      &_bi,
			           Options           &_opt,
				   bool              _swap) const;

  bool read_binary_edge_chunk(     std::istream      &_is,
			           BaseImporter      &_bi,
			           Options           &_opt,
				   bool              _swap) const;

  bool read_binary_halfedge_chunk( std::istream      &_is,
				   BaseImporter      &_bi,
				   Options           &_opt,
				   bool              _swap) const;

  bool read_binary_mesh_chunk(     std::istream      &_is,
				   BaseImporter      &_bi,
				   Options           &_opt,
				   bool              _swap) const;

  size_t restore_binary_custom_data( std::istream& _is,
				     BaseProperty* _bp,
				     size_t _n_elem,
				     bool _swap) const;

};


//== TYPE DEFINITION ==========================================================


/// Declare the single entity of the OM reader.
extern _OMReader_  __OMReaderInstance;
ASTER_TOPOLOGYDLLEXPORT _OMReader_&  OMReader();


//=============================================================================
} // namespace IO
} // namespace AsterTopology
//=============================================================================
#endif
//=============================================================================
