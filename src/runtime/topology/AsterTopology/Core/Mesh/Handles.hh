#ifndef ASTER_TOPOLOGY_HANDLES_HH
#define ASTER_TOPOLOGY_HANDLES_HH


//== INCLUDES =================================================================

#include <AsterTopology/Core/System/config.h>
#include <ostream>


//== NAMESPACES ===============================================================

namespace AsterTopology {

//== CLASS DEFINITION =========================================================


/// Base class for all handle types
class BaseHandle
{ 
public:
  
  explicit BaseHandle(int _idx=-1) : idx_(_idx) {}

  /// Get the underlying index of this handle
  int idx() const { return idx_; }

  /// The handle is valid iff the index is not negative.
  bool is_valid() const { return idx_ >= 0; }

  /// reset handle to be invalid
  void reset() { idx_=-1; }
  /// reset handle to be invalid
  void invalidate() { idx_ = -1; }

  bool operator==(const BaseHandle& _rhs) const { 
    return (this->idx_ == _rhs.idx_); 
  }

  bool operator!=(const BaseHandle& _rhs) const { 
    return (this->idx_ != _rhs.idx_); 
  }

  bool operator<(const BaseHandle& _rhs) const { 
    return (this->idx_ < _rhs.idx_); 
  }


  // this is to be used only by the iterators
  void __increment() { ++idx_; }
  void __decrement() { --idx_; }

  void __increment(int amount) { idx_ += amount; }
  void __decrement(int amount) { idx_ -= amount; }

private:

  int idx_; 
};

// this is used by boost::unordered_set/map
inline size_t hash_value(const BaseHandle&  h)   { return h.idx(); }

//-----------------------------------------------------------------------------

/// Write handle \c _hnd to stream \c _os
inline std::ostream& operator<<(std::ostream& _os, const BaseHandle& _hnd) 
{
  return (_os << _hnd.idx());
}


//-----------------------------------------------------------------------------


/// Handle for a vertex entity
struct VertexHandle : public BaseHandle
{
  explicit VertexHandle(int _idx=-1) : BaseHandle(_idx) {}
};


/// Handle for a halfedge entity
struct HalfedgeHandle : public BaseHandle
{
  explicit HalfedgeHandle(int _idx=-1) : BaseHandle(_idx) {}
};


/// Handle for a edge entity
struct EdgeHandle : public BaseHandle
{
  explicit EdgeHandle(int _idx=-1) : BaseHandle(_idx) {}
};


/// Handle for a face entity
struct FaceHandle : public BaseHandle
{
  explicit FaceHandle(int _idx=-1) : BaseHandle(_idx) {}
};


//=============================================================================
} // namespace AsterTopology
//=============================================================================

#ifdef OM_HAS_HASH
#include <functional>
namespace std {

#if defined(_MSVC_VER)
#  pragma warning(push)
#  pragma warning(disable:4099) // For VC++ it is class hash
#endif


template <>
struct hash<AsterTopology::BaseHandle >
{
  typedef AsterTopology::BaseHandle argument_type;
  typedef std::size_t result_type;

  std::size_t operator()(const AsterTopology::BaseHandle& h) const
  {
    return h.idx();
  }
};

template <>
struct hash<AsterTopology::VertexHandle >
{
  typedef AsterTopology::VertexHandle argument_type;
  typedef std::size_t result_type;

  std::size_t operator()(const AsterTopology::VertexHandle& h) const
  {
    return h.idx();
  }
};

template <>
struct hash<AsterTopology::HalfedgeHandle >
{

  typedef AsterTopology::HalfedgeHandle argument_type;
  typedef std::size_t result_type;
  
  std::size_t operator()(const AsterTopology::HalfedgeHandle& h) const
  {
    return h.idx();
  }
};

template <>
struct hash<AsterTopology::EdgeHandle >
{

  typedef AsterTopology::EdgeHandle argument_type;
  typedef std::size_t result_type;
  
  std::size_t operator()(const AsterTopology::EdgeHandle& h) const
  {
    return h.idx();
  }
};

template <>
struct hash<AsterTopology::FaceHandle >
{

  typedef AsterTopology::FaceHandle argument_type;
  typedef std::size_t result_type;
  
  std::size_t operator()(const AsterTopology::FaceHandle& h) const
  {
    return h.idx();
  }
};

#if defined(_MSVC_VER)
#  pragma warning(pop)
#endif

}
#endif  // OM_HAS_HASH


#endif // ASTER_TOPOLOGY_HANDLES_HH
//=============================================================================
