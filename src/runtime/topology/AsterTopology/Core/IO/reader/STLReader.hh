
//=============================================================================
//
//  Implements an reader module for STL files
//
//=============================================================================


#ifndef __STLREADER_HH__
#define __STLREADER_HH__


//=== INCLUDES ================================================================


#include <stdio.h>
#include <string>

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
    Implementation of the STL format reader. This class is singleton'ed by
    SingletonT to STLReader.
*/
class ASTER_TOPOLOGYDLLEXPORT _STLReader_ : public BaseReader
{
public:

  // constructor
  _STLReader_();

  /// Destructor
  virtual ~_STLReader_() {};


  std::string get_description() const
  { return "Stereolithography Interface Format"; }
  std::string get_extensions() const { return "stl stla stlb"; }

  bool read(const std::string& _filename,
	    BaseImporter& _bi,
            Options& _opt);

  bool read(std::istream& _in,
		    BaseImporter& _bi,
            Options& _opt);

  /** Set the threshold to be used for considering two point to be equal.
      Can be used to merge small gaps */
  void set_epsilon(float _eps) { eps_=_eps; }

  /// Returns the threshold to be used for considering two point to be equal.
  float epsilon() const { return eps_; }



private:

  enum STL_Type { STLA, STLB, NONE };
  STL_Type check_stl_type(const std::string& _filename) const;

  bool read_stla(const std::string& _filename, BaseImporter& _bi, Options& _opt) const;
  bool read_stla(std::istream& _in, BaseImporter& _bi, Options& _opt) const;
  bool read_stlb(const std::string& _filename, BaseImporter& _bi, Options& _opt) const;
  bool read_stlb(std::istream& _in, BaseImporter& _bi, Options& _opt) const;


private:

  float eps_;
};


//== TYPE DEFINITION ==========================================================


/// Declare the single entity of the STL reader
extern _STLReader_  __STLReaderInstance;
ASTER_TOPOLOGYDLLEXPORT _STLReader_&  STLReader();


//=============================================================================
} // namespace IO
} // namespace AsterTopology
//=============================================================================
#endif
//=============================================================================
