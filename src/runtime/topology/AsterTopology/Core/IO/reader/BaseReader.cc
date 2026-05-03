
//=== INCLUDES ================================================================

#include <AsterTopology/Core/IO/reader/BaseReader.hh>

#if defined(OM_CC_MIPS)
#  include <ctype.h>
#else
#endif


//== NAMESPACES ===============================================================


namespace AsterTopology {
namespace IO {


//=== IMPLEMENTATION ==========================================================


static inline char tolower(char c) 
{
  using namespace std;
  return ::tolower(c); 
}


//-----------------------------------------------------------------------------


bool 
BaseReader::
can_u_read(const std::string& _filename) const 
{
  // get file extension
  std::string extension;
  std::string::size_type pos(_filename.rfind("."));

  if (pos != std::string::npos)
    extension = _filename.substr(pos+1, _filename.length()-pos-1);
  else
    extension = _filename; //check, if the whole filename defines the extension

  std::transform( extension.begin(), extension.end(),
	  extension.begin(), tolower );

  // locate extension in extension string
  return (get_extensions().find(extension) != std::string::npos);
}


//-----------------------------------------------------------------------------


bool 
BaseReader::
check_extension(const std::string& _fname, const std::string& _ext) const
{
  std::string cmpExt(_ext);

  std::transform( _ext.begin(), _ext.end(),  cmpExt.begin(), tolower );

  std::string::size_type pos(_fname.rfind("."));

  if (pos != std::string::npos && !_ext.empty() )
  { 
    std::string ext;

    // extension without dot!
    ext = _fname.substr(pos+1, _fname.length()-pos-1);

    std::transform( ext.begin(), ext.end(), ext.begin(), tolower );
    
    return ext == cmpExt;
  }
  return false;  
}


//=============================================================================
} // namespace IO
} // namespace AsterTopology
//=============================================================================
