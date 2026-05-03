//=============================================================================
//
//  Implements the AsterTopology IOManager singleton
//
//=============================================================================

#ifndef __IOMANAGER_HH__
#define __IOMANAGER_HH__


//=== INCLUDES ================================================================


// STL
#include <iosfwd>
#include <sstream>
#include <string>
#include <set>

// AsterTopology
#include <AsterTopology/Core/System/config.h>
#include <AsterTopology/Core/IO/Options.hh>
#include <AsterTopology/Core/IO/reader/BaseReader.hh>
#include <AsterTopology/Core/IO/writer/BaseWriter.hh>
#include <AsterTopology/Core/IO/importer/BaseImporter.hh>
#include <AsterTopology/Core/IO/exporter/BaseExporter.hh>
#include <AsterTopology/Core/Utils/SingletonT.hh>


//== NAMESPACES ===============================================================


namespace AsterTopology {
namespace IO {


//=== IMPLEMENTATION ==========================================================


/** This is the real IOManager class that is later encapsulated by
    SingletonT to enforce its uniqueness. _IOManager_ is not meant to be used
    directly by the programmer - the IOManager alias exists for this task.

    All reader/writer modules register themselves at this class. For
    reading or writing data all modules are asked to do the job. If no
    suitable module is found, an error is returned.

    For the sake of reading, the target data structure is hidden
    behind the BaseImporter interface that takes care of adding
    vertices or faces.

    Writing from a source structure is encapsulate similarly behind a
    BaseExporter interface, providing iterators over vertices/faces to
    the writer modules.

    \see \ref mesh_io
*/

class ASTER_TOPOLOGYDLLEXPORT _IOManager_
{
private:

  /// Constructor has nothing todo for the Manager
  _IOManager_() {}

  /// Destructor has nothing todo for the Manager
  ~_IOManager_() {};

  /** Declare the singleton getter function as friend to access the private constructor
      and destructor
    */
  friend ASTER_TOPOLOGYDLLEXPORT _IOManager_& IOManager();

public:

  /**
     Read a mesh from file _filename. The target data structure is specified
     by the given BaseImporter. The \c read method consecutively queries all
     of its reader modules. True is returned upon success, false if all
     reader modules failed to interprete _filename.
  */
  bool read(const std::string& _filename,
	    BaseImporter& _bi,
	    Options& _opt);

/**
     Read a mesh from open std::istream _is. The target data structure is specified
     by the given BaseImporter. The \c sread method consecutively queries all
     of its reader modules. True is returned upon success, false if all
     reader modules failed to use _is.
  */
  bool read(std::istream& _filename,
	    const std::string& _ext,
	    BaseImporter& _bi,
	    Options& _opt);


  /** Write a mesh to file _filename. The source data structure is specified
      by the given BaseExporter. The \c save method consecutively queries all
      of its writer modules. True is returned upon success, false if all
      writer modules failed to write the requested format.
      Options is determined by _filename's extension.
  */
  bool write(const std::string& _filename,
	     BaseExporter& _be,
	     Options _opt=Options::Default,
             std::streamsize _precision = 6);

/** Write a mesh to open std::ostream _os. The source data structure is specified
      by the given BaseExporter. The \c save method consecutively queries all
      of its writer modules. True is returned upon success, false if all
      writer modules failed to write the requested format.
      Options is determined by _filename's extension.
  */
  bool write(std::ostream& _filename,
	     const std::string& _ext,
	     BaseExporter& _be,
	     Options _opt=Options::Default,
             std::streamsize _precision = 6);


  /// Returns true if the format is supported by one of the reader modules.
  bool can_read( const std::string& _format ) const;

  /// Returns true if the format is supported by one of the writer modules.
  bool can_write( const std::string& _format ) const;


  size_t binary_size(const std::string& _format,
		     BaseExporter& _be,
		     Options _opt = Options::Default)
  {
    const BaseWriter *bw = find_writer(_format);
    return bw ? bw->binary_size(_be,_opt) : 0;
  }



public: //-- QT convenience function ------------------------------------------


  /** Returns all readable file extension + descriptions in one string.
      File formats are separated by <c>;;</c>.
      Convenience function for Qt file dialogs.
  */
  const std::string& qt_read_filters()  const { return read_filters_;  }


  /** Returns all writeable file extension + descriptions in one string.
      File formats are separated by <c>;;</c>.
      Convenience function for Qt file dialogs.
  */
  const std::string& qt_write_filters() const { return write_filters_; }



private:

  // collect all readable file extensions
  void update_read_filters();


  // collect all writeable file extensions
  void update_write_filters();



public:  //-- SYSTEM PART------------------------------------------------------


  /** Registers a new reader module. A call to this function should be
      implemented in the constructor of all classes derived from BaseReader.
  */
  bool register_module(BaseReader* _bl)
  {
    reader_modules_.insert(_bl);
    update_read_filters();
    return true;
  }



  /** Registers a new writer module. A call to this function should be
      implemented in the constructor of all classed derived from BaseWriter.
  */
  bool register_module(BaseWriter* _bw)
  {
    writer_modules_.insert(_bw);
    update_write_filters();
    return true;
  }


private:

  const BaseWriter *find_writer(const std::string& _format);

  // stores registered reader modules
  std::set<BaseReader*> reader_modules_;

  // stores registered writer modules
  std::set<BaseWriter*> writer_modules_;

  // input filters (e.g. for Qt file dialog)
  std::string read_filters_;

  // output filters (e.g. for Qt file dialog)
  std::string write_filters_;
};


//=============================================================================


//_IOManager_*  __IOManager_instance; Causes memory leak, as destructor is never called

ASTER_TOPOLOGYDLLEXPORT _IOManager_& IOManager();

//=============================================================================
} // namespace IO
} // namespace AsterTopology
//=============================================================================
#endif
//=============================================================================
