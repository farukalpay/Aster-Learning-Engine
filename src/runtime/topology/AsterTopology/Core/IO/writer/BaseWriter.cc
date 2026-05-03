
//=== INCLUDES ================================================================


#include <AsterTopology/Core/IO/writer/BaseWriter.hh>

#if defined(OM_CC_MIPS)
#  include <ctype.h>
#else
#endif


//== NAMESPACES ===============================================================


namespace AsterTopology {
namespace IO {


//=== IMPLEMENTATION ==========================================================

#ifndef DOXY_IGNORE_THIS

static inline char tolower(char c)
{
  using namespace std;
  return ::tolower(c); 
}

#endif

//-----------------------------------------------------------------------------


bool 
BaseWriter::
can_u_write(const std::string& _filename) const 
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


//=============================================================================
} // namespace IO
} // namespace AsterTopology
//=============================================================================
