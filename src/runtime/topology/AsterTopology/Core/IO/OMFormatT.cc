
//=============================================================================
//
//  Helper Functions for binary reading / writing
//
//=============================================================================


#define ASTER_TOPOLOGY_IO_OMFORMAT_CC


//== INCLUDES =================================================================

#include <AsterTopology/Core/IO/OMFormat.hh>
#include <algorithm>
#include <iomanip>

//== NAMESPACES ===============================================================

namespace AsterTopology {
namespace IO {

  // helper to store a an integer
  template< typename T > 
  size_t 
  store( std::ostream& _os, 
	 const T& _val, 
	 OMFormat::Chunk::Integer_Size _b, 
	 bool _swap,
	 t_signed)
  {    
    assert( OMFormat::is_integer( _val ) );

    switch( _b ) 
    {
      case OMFormat::Chunk::Integer_8:
      { 	
	OMFormat::int8 v = static_cast<OMFormat::int8>(_val);
	return store( _os, v, _swap );
      }
      case OMFormat::Chunk::Integer_16:
      { 
	OMFormat::int16 v = static_cast<OMFormat::int16>(_val);
	return store( _os, v, _swap );
      }
      case OMFormat::Chunk::Integer_32:
      { 
	OMFormat::int32 v = static_cast<OMFormat::int32>(_val);
	return store( _os, v, _swap );
      }      
      case OMFormat::Chunk::Integer_64:
      { 
	OMFormat::int64 v = static_cast<OMFormat::int64>(_val);
	return store( _os, v, _swap );
      }
    }
    return 0;
  }


  // helper to store a an unsigned integer
  template< typename T > 
  size_t 
  store( std::ostream& _os, 
	 const T& _val, 
	 OMFormat::Chunk::Integer_Size _b, 
	 bool _swap,
	 t_unsigned)
  {    
    assert( OMFormat::is_integer( _val ) );

    switch( _b ) 
    {
      case OMFormat::Chunk::Integer_8:
      { 
	OMFormat::uint8 v = static_cast<OMFormat::uint8>(_val);
	return store( _os, v, _swap );
      }
      case OMFormat::Chunk::Integer_16:
      { 
	OMFormat::uint16 v = static_cast<OMFormat::uint16>(_val);
	return store( _os, v, _swap );
      }
      case OMFormat::Chunk::Integer_32:
      { 
	OMFormat::uint32 v = static_cast<OMFormat::uint32>(_val);
	return store( _os, v, _swap );
      }
      
      case OMFormat::Chunk::Integer_64:
      { 
	OMFormat::uint64 v = static_cast<OMFormat::uint64>(_val);
	return store( _os, v, _swap );
      }
    }
    return 0;
  }


  // helper to restore a an integer
  template< typename T > 
  size_t 
  restore( std::istream& _is, 
	   T& _val, 
	   OMFormat::Chunk::Integer_Size _b, 
	   bool _swap,
	   t_signed)
  {    
    assert( OMFormat::is_integer( _val ) );
    size_t bytes = 0;

    switch( _b ) 
    {
      case OMFormat::Chunk::Integer_8:
      { 	
	OMFormat::int8 v;
	bytes = restore( _is, v, _swap );
	_val = static_cast<T>(v);
	break;
      }
      case OMFormat::Chunk::Integer_16:
      { 
	OMFormat::int16 v;
	bytes = restore( _is, v, _swap );
	_val = static_cast<T>(v);
	break;
      }
      case OMFormat::Chunk::Integer_32:
      { 
	OMFormat::int32 v;
	bytes = restore( _is, v, _swap );
	_val = static_cast<T>(v);
	break;
      }      
      case OMFormat::Chunk::Integer_64:
      { 
	OMFormat::int64 v;
	bytes = restore( _is, v, _swap );
	_val = static_cast<T>(v);
	break;
      }
    }
    return bytes;
  }


  // helper to restore a an unsigned integer
  template< typename T > 
  size_t 
  restore( std::istream& _is, 
	   T& _val, 
	   OMFormat::Chunk::Integer_Size _b, 
	   bool _swap,
	   t_unsigned)
  {    
    assert( OMFormat::is_integer( _val ) );
    size_t bytes = 0;

    switch( _b ) 
    {
      case OMFormat::Chunk::Integer_8:
      { 
	OMFormat::uint8 v;
	bytes = restore( _is, v, _swap );
	_val = static_cast<T>(v);
	break;
      }
      case OMFormat::Chunk::Integer_16:
      { 
	OMFormat::uint16 v;
	bytes = restore( _is, v, _swap );
	_val = static_cast<T>(v);
	break;
      }
      case OMFormat::Chunk::Integer_32:
      { 
	OMFormat::uint32 v;
	bytes = restore( _is, v, _swap );
	_val = static_cast<T>(v);
	break;
      }
      
      case OMFormat::Chunk::Integer_64:
      { 
	OMFormat::uint64 v;
	bytes = restore( _is, v, _swap );
	_val = static_cast<T>(v);
	break;
      }
    }
    return bytes;
  }

//=============================================================================
} // namespace IO
} // namespace AsterTopology
//=============================================================================
