#ifndef ASTER_TOPOLOGY_ITERATORS_HH
#define ASTER_TOPOLOGY_ITERATORS_HH

//=============================================================================
//
//  Iterators for PolyMesh/TriMesh
//
//=============================================================================



//== INCLUDES =================================================================

#include <AsterTopology/Core/System/config.h>
#include <AsterTopology/Core/Mesh/Status.hh>
#include <cassert>
#include <cstddef>
#include <iterator>


//== NAMESPACES ===============================================================

namespace AsterTopology {
namespace Iterators {


//== FORWARD DECLARATIONS =====================================================


template <class Mesh> class ConstVertexIterT;
template <class Mesh> class VertexIterT;
template <class Mesh> class ConstHalfedgeIterT;
template <class Mesh> class HalfedgeIterT;
template <class Mesh> class ConstEdgeIterT;
template <class Mesh> class EdgeIterT;
template <class Mesh> class ConstFaceIterT;
template <class Mesh> class FaceIterT;


template <class Mesh, class ValueHandle, class MemberOwner, bool (MemberOwner::*PrimitiveStatusMember)() const, size_t (MemberOwner::*PrimitiveCountMember)() const>
class GenericIteratorT {
    public:
        //--- Typedefs ---

        typedef ValueHandle                     value_handle;
        typedef value_handle                    value_type;
        typedef std::bidirectional_iterator_tag iterator_category;
        typedef std::ptrdiff_t                  difference_type;
        typedef const value_type&               reference;
        typedef const value_type*               pointer;
        typedef const Mesh*                     mesh_ptr;
        typedef const Mesh&                     mesh_ref;

        /// Default constructor.
        GenericIteratorT()
        : mesh_(0), skip_bits_(0)
        {}

        /// Construct with mesh and a target handle.
        GenericIteratorT(mesh_ref _mesh, value_handle _hnd, bool _skip=false)
        : mesh_(&_mesh), hnd_(_hnd), skip_bits_(0)
        {
            if (_skip) enable_skipping();
        }

        /// Standard dereferencing operator.
        reference operator*() const {
            return hnd_;
        }

        /// Standard pointer operator.
        pointer operator->() const {
            return &hnd_;
        }

        /**
         * \brief Get the handle of the item the iterator refers to.
         * \deprecated 
         * This function clutters your code. Use dereferencing operators -> and * instead.
         */
        OM_DEPRECATED("This function clutters your code. Use dereferencing operators -> and * instead.")
        value_handle handle() const {
            return hnd_;
        }

        /**
         * \brief Cast to the handle of the item the iterator refers to.
         * \deprecated
         * Implicit casts of iterators are unsafe. Use dereferencing operators
         * -> and * instead.
         */
        OM_DEPRECATED("Implicit casts of iterators are unsafe. Use dereferencing operators -> and * instead.")
        operator value_handle() const {
            return hnd_;
        }

        /// Are two iterators equal? Only valid if they refer to the same mesh!
        bool operator==(const GenericIteratorT& _rhs) const {
            return ((mesh_ == _rhs.mesh_) && (hnd_ == _rhs.hnd_));
        }

        /// Not equal?
        bool operator!=(const GenericIteratorT& _rhs) const {
            return !operator==(_rhs);
        }

        /// Standard pre-increment operator
        GenericIteratorT& operator++() {
            hnd_.__increment();
            if (skip_bits_)
                skip_fwd();
            return *this;
        }

        /// Standard post-increment operator
        GenericIteratorT operator++(int) {
            GenericIteratorT cpy(*this);
            ++(*this);
            return cpy;
        }

#if ((defined(_MSC_VER) && (_MSC_VER >= 1800)) || __cplusplus > 199711L || defined(__GXX_EXPERIMENTAL_CXX0X__)) && !defined(ASTER_TOPOLOGY_VECTOR_LEGACY)
        template<class T = value_handle>
        auto operator+=(int amount) ->
            typename std::enable_if<
                sizeof(decltype(std::declval<T>().__increment(amount))) >= 0,
                GenericIteratorT&>::type {
            static_assert(std::is_same<T, value_handle>::value,
                    "Template parameter must not deviate from default.");
            if (skip_bits_)
                throw std::logic_error("Skipping iterators do not support "
                        "random access.");
            hnd_.__increment(amount);
            return *this;
        }

        template<class T = value_handle>
        auto operator+(int rhs) ->
            typename std::enable_if<
                sizeof(decltype(std::declval<T>().__increment(rhs), void (), int {})) >= 0,
                GenericIteratorT>::type {
            static_assert(std::is_same<T, value_handle>::value,
                    "Template parameter must not deviate from default.");
            if (skip_bits_)
                throw std::logic_error("Skipping iterators do not support "
                        "random access.");
            GenericIteratorT result = *this;
            result.hnd_.__increment(rhs);
            return result;
        }
#endif

        /// Standard pre-decrement operator
        GenericIteratorT& operator--() {
            hnd_.__decrement();
            if (skip_bits_)
                skip_bwd();
            return *this;
        }

        /// Standard post-decrement operator
        GenericIteratorT operator--(int) {
            GenericIteratorT cpy(*this);
            --(*this);
            return cpy;
        }

        /// Turn on skipping: automatically skip deleted/hidden elements
        void enable_skipping() {
            if (mesh_ && (mesh_->*PrimitiveStatusMember)()) {
                Attributes::StatusInfo status;
                status.set_deleted(true);
                status.set_hidden(true);
                skip_bits_ = status.bits();
                skip_fwd();
            } else
                skip_bits_ = 0;
        }

        /// Turn on skipping: automatically skip deleted/hidden elements
        void disable_skipping() {
            skip_bits_ = 0;
        }

    private:

        void skip_fwd() {
            assert(mesh_ && skip_bits_);
            while ((hnd_.idx() < (signed) (mesh_->*PrimitiveCountMember)())
                    && (mesh_->status(hnd_).bits() & skip_bits_))
                hnd_.__increment();
        }

        void skip_bwd() {
            assert(mesh_ && skip_bits_);
            while ((hnd_.idx() >= 0) && (mesh_->status(hnd_).bits() & skip_bits_))
                hnd_.__decrement();
        }

    protected:
        mesh_ptr mesh_;
        value_handle hnd_;
        unsigned int skip_bits_;
};

//=============================================================================
} // namespace Iterators
} // namespace AsterTopology
//=============================================================================
#endif 
//=============================================================================
