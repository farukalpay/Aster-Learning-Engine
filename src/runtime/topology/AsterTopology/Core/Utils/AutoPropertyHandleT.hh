#ifndef ASTER_TOPOLOGY_AutoPropertyHandleT_HH
#define ASTER_TOPOLOGY_AutoPropertyHandleT_HH

//== INCLUDES =================================================================
#include <assert.h>
#include <string>

//== NAMESPACES ===============================================================

namespace AsterTopology {

//== CLASS DEFINITION =========================================================

template <class Mesh_, class PropertyHandle_>
class AutoPropertyHandleT : public PropertyHandle_
{
public:
  typedef Mesh_                             Mesh;
  typedef PropertyHandle_                   PropertyHandle;
  typedef PropertyHandle                    Base;
  typedef typename PropertyHandle::Value    Value;
  typedef AutoPropertyHandleT<Mesh, PropertyHandle>
                                            Self;
protected:
  Mesh*                                     m_;
  bool                                      own_property_;//ref counting?

public:
  AutoPropertyHandleT()
  : m_(NULL), own_property_(false)
  {}
  
  AutoPropertyHandleT(const Self& _other)
  : Base(_other.idx()), m_(_other.m_), own_property_(false)
  {}
  
  explicit AutoPropertyHandleT(Mesh& _m, const std::string& _pp_name = std::string())
  { add_property(_m, _pp_name); }

  AutoPropertyHandleT(Mesh& _m, PropertyHandle _pph)
  : Base(_pph.idx()), m_(&_m), own_property_(false)
  {}

  ~AutoPropertyHandleT()
  {
    if (own_property_)
    {
      m_->remove_property(*this);
    }
  }

  inline void                               add_property(Mesh& _m, const std::string& _pp_name = std::string())
  {
    assert(!is_valid());
    m_ = &_m;
    own_property_ = _pp_name.empty() || !m_->get_property_handle(*this, _pp_name);
    if (own_property_)
    {
      m_->add_property(*this, _pp_name);
    }
  }
  
  inline void                               remove_property()
  {
    assert(own_property_);//only the owner can delete the property
    m_->remove_property(*this);
    own_property_ = false;
    invalidate();
  }
  
  template <class _Handle>
  inline Value&                             operator [] (_Handle _hnd)
  { return m_->property(*this, _hnd); }

  template <class _Handle>
  inline const Value&                       operator [] (_Handle _hnd) const
  { return m_->property(*this, _hnd); }

  inline bool                               own_property() const
  { return own_property_; }

  inline void                               free_property()
  { own_property_ = false; }
};

//=============================================================================
} // namespace AsterTopology
//=============================================================================
#endif // ASTER_TOPOLOGY_AutoPropertyHandleT_HH defined
//=============================================================================
