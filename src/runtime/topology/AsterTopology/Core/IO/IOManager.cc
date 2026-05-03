
//=============================================================================
//
//  Implements the AsterTopology IOManager singleton
//
//=============================================================================


//== INCLUDES =================================================================


#include <AsterTopology/Core/IO/IOManager.hh>

#include <iostream>


//== NAMESPACES ===============================================================


namespace AsterTopology {
namespace IO {


//=============================================================================

// Destructor never called. Moved into singleton  getter function
// _IOManager_  *__IOManager_instance = 0;

_IOManager_& IOManager()
{

  static _IOManager_  __IOManager_instance;

  //if (!__IOManager_instance)
  //  __IOManager_instance = new _IOManager_();

  return __IOManager_instance;
}

//-----------------------------------------------------------------------------

bool
_IOManager_::
read(const std::string& _filename, BaseImporter& _bi, Options& _opt)
{
  std::set<BaseReader*>::const_iterator it     =  reader_modules_.begin();
  std::set<BaseReader*>::const_iterator it_end =  reader_modules_.end();

  if( it == it_end ) 
  { 
    omerr() << "[AsterTopology::IO::_IOManager_] No reading modules available!\n"; 
    return false; 
  } 

  // Try all registered modules
  for(; it != it_end; ++it)
    if ((*it)->can_u_read(_filename))
    {
      _bi.prepare();
      bool ok = (*it)->read(_filename, _bi, _opt);
      _bi.finish();
      return ok;
    }

  // All modules failed to read
  return false;
}


//-----------------------------------------------------------------------------


bool
_IOManager_::
read(std::istream& _is, const std::string& _ext, BaseImporter& _bi, Options& _opt)
{
  std::set<BaseReader*>::const_iterator it     =  reader_modules_.begin();
  std::set<BaseReader*>::const_iterator it_end =  reader_modules_.end();

  // Try all registered modules
  for(; it != it_end; ++it)
    if ((*it)->BaseReader::can_u_read(_ext))  //Use the extension check only (no file existence)
    {
      _bi.prepare();
      bool ok = (*it)->read(_is, _bi, _opt);
      _bi.finish();
      return ok;
    }

  // All modules failed to read
  return false;
}


//-----------------------------------------------------------------------------


bool
_IOManager_::
write(const std::string& _filename, BaseExporter& _be, Options _opt, std::streamsize _precision)
{
  std::set<BaseWriter*>::const_iterator it     = writer_modules_.begin();
  std::set<BaseWriter*>::const_iterator it_end = writer_modules_.end();

  if ( it == it_end )
  {
    omerr() << "[AsterTopology::IO::_IOManager_] No writing modules available!\n";
    return false;
  }

  // Try all registered modules
  for(; it != it_end; ++it)
  {
    if ((*it)->can_u_write(_filename))
    {
      return (*it)->write(_filename, _be, _opt, _precision);
    }
  }

  // All modules failed to save
  return false;
}

//-----------------------------------------------------------------------------


bool
_IOManager_::
write(std::ostream& _os,const std::string &_ext, BaseExporter& _be, Options _opt, std::streamsize _precision)
{
  std::set<BaseWriter*>::const_iterator it     = writer_modules_.begin();
  std::set<BaseWriter*>::const_iterator it_end = writer_modules_.end();

  if ( it == it_end )
  {
    omerr() << "[AsterTopology::IO::_IOManager_] No writing modules available!\n";
    return false;
  }

  // Try all registered modules
  for(; it != it_end; ++it)
  {
    if ((*it)->BaseWriter::can_u_write(_ext)) //Restrict test to the extension check
    {
      return (*it)->write(_os, _be, _opt, _precision);
    }
  }

  // All modules failed to save
  return false;
}

//-----------------------------------------------------------------------------


bool
_IOManager_::
can_read( const std::string& _format ) const
{
  std::set<BaseReader*>::const_iterator it     = reader_modules_.begin();
  std::set<BaseReader*>::const_iterator it_end = reader_modules_.end();
  std::string filename = "dummy." + _format;

  for(; it != it_end; ++it)
    if ((*it)->can_u_read(filename))
      return true;

  return false;
}


//-----------------------------------------------------------------------------


bool
_IOManager_::
can_write( const std::string& _format ) const
{
  std::set<BaseWriter*>::const_iterator it     = writer_modules_.begin();
  std::set<BaseWriter*>::const_iterator it_end = writer_modules_.end();
  std::string filename = "dummy." + _format;

  // Try all registered modules
  for(; it != it_end; ++it)
    if ((*it)->can_u_write(filename))
      return true;

  return false;
}


//-----------------------------------------------------------------------------


const BaseWriter*
_IOManager_::
find_writer(const std::string& _format)
{
  using std::string;

  string::size_type dot = _format.rfind('.');

  string ext;
  if (dot == string::npos)
    ext = _format;
  else
    ext = _format.substr(dot+1,_format.length()-(dot+1));

  std::set<BaseWriter*>::const_iterator it     = writer_modules_.begin();
  std::set<BaseWriter*>::const_iterator it_end = writer_modules_.end();
  std::string filename = "dummy." + ext;

  // Try all registered modules
  for(; it != it_end; ++it)
    if ((*it)->can_u_write(filename))
      return *it;

  return NULL;
}


//-----------------------------------------------------------------------------


void
_IOManager_::
update_read_filters()
{
  std::set<BaseReader*>::const_iterator it     = reader_modules_.begin(),
                                        it_end = reader_modules_.end();
  std::string all = "";
  std::string filters = "";

  for(; it != it_end; ++it)
  {
    // Initialized with space, as a workaround for debug build with clang on mac
    // which crashes if tmp is initialized with an empty string ?!
    std::string tmp = " ";

    filters += (*it)->get_description() + " (";

    std::istringstream iss((*it)->get_extensions());

    while (iss && !iss.eof() && (iss >> tmp) )
    {
     tmp = " *." + tmp; filters += tmp; all += tmp;
    }

    filters += " );;";
  }

  all = "All files ( " + all + " );;";

  read_filters_ = all + filters;
}


//-----------------------------------------------------------------------------


void
_IOManager_::
update_write_filters()
{
  std::set<BaseWriter*>::const_iterator it     = writer_modules_.begin(),
                                        it_end = writer_modules_.end();
  std::string all;
  std::string filters;

  for(; it != it_end; ++it)
  {
    // Initialized with space, as a workaround for debug build with clang on mac
    // which crashes if tmp is initialized with an empty string ?!
    std::string s = " ";

    filters += (*it)->get_description() + " (";

    std::istringstream iss((*it)->get_extensions());
    while (iss && !iss.eof() && (iss >> s))
    { s = " *." + s; filters += s; all += s; }

    filters += " );;";
  }
  all = "All files ( " + all + " );;";

  write_filters_ = all + filters;
}


//=============================================================================
} // namespace IO
} // namespace AsterTopology
//=============================================================================
