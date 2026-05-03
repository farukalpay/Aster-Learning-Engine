
//=============================================================================
//
//  Helper Functions for binary reading / writing
//
//=============================================================================


#ifndef ASTER_TOPOLOGY_VECTORCAST_HH
#define ASTER_TOPOLOGY_VECTORCAST_HH


//== INCLUDES =================================================================


#include <AsterTopology/Core/System/config.h>
#include <AsterTopology/Core/Utils/vector_traits.hh>
#include <AsterTopology/Core/Utils/GenProg.hh>
#include <AsterTopology/Core/Geometry/VectorT.hh>


//== NAMESPACES ===============================================================


namespace AsterTopology {


//=============================================================================


/** \name Cast vector type to another vector type.
*/
//@{

//-----------------------------------------------------------------------------

template <typename src_t, typename dst_t, int n>
inline void vector_cast( const src_t &_src, dst_t &_dst, GenProg::Int2Type<n> )
{
  assert_compile(vector_traits<dst_t>::size_ <= vector_traits<src_t>::size_)
  vector_cast(_src,_dst, GenProg::Int2Type<n-1>());
  _dst[n-1] = static_cast<typename vector_traits<dst_t>::value_type >(_src[n-1]);
}

template <typename src_t, typename dst_t>
inline void vector_cast( const src_t & /*_src*/, dst_t & /*_dst*/, GenProg::Int2Type<0> )
{
}

template <typename src_t, typename dst_t, int n>
inline void vector_copy( const src_t &_src, dst_t &_dst, GenProg::Int2Type<n> )
{
  assert_compile(vector_traits<dst_t>::size_ <= vector_traits<src_t>::size_)
  vector_copy(_src,_dst, GenProg::Int2Type<n-1>());
  _dst[n-1] = _src[n-1];
}

template <typename src_t, typename dst_t>
inline void vector_copy( const src_t & /*_src*/, dst_t & /*_dst*/ , GenProg::Int2Type<0> )
{
}



//-----------------------------------------------------------------------------
#ifndef DOXY_IGNORE_THIS

template <typename dst_t, typename src_t>
struct vector_caster
{
  typedef dst_t  return_type;

  inline static return_type cast(const src_t& _src)
  {
    dst_t dst;
    vector_cast(_src, dst, GenProg::Int2Type<vector_traits<dst_t>::size_>());
    return dst;
  }
};

#if !defined(OM_CC_MSVC)
template <typename dst_t>
struct vector_caster<dst_t,dst_t>
{
  typedef const dst_t&  return_type;

  inline static return_type cast(const dst_t& _src)
  {
    return _src;
  }
};
#endif

#endif
//-----------------------------------------------------------------------------


/// Cast vector type to another vector type by copying the vector elements
template <typename dst_t, typename src_t>
inline
typename vector_caster<dst_t, src_t>::return_type
vector_cast(const src_t& _src )
{
  return vector_caster<dst_t, src_t>::cast(_src);
}


//@}


//=============================================================================
} // namespace AsterTopology
//=============================================================================
#endif // ASTER_TOPOLOGY_MESHREADER_HH defined
//=============================================================================
