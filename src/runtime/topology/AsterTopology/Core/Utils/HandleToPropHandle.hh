#ifndef HANDLETOPROPHANDLE_HH_
#define HANDLETOPROPHANDLE_HH_

#include <AsterTopology/Core/Mesh/Handles.hh>
#include <AsterTopology/Core/Utils/Property.hh>

namespace AsterTopology {

    template<typename ElementT, typename T>
    struct HandleToPropHandle {
    };

    template<typename T>
    struct HandleToPropHandle<AsterTopology::VertexHandle, T> {
        using type = AsterTopology::VPropHandleT<T>;
    };

    template<typename T>
    struct HandleToPropHandle<AsterTopology::HalfedgeHandle, T> {
        using type = AsterTopology::HPropHandleT<T>;
    };

    template<typename T>
    struct HandleToPropHandle<AsterTopology::EdgeHandle, T> {
        using type = AsterTopology::EPropHandleT<T>;
    };

    template<typename T>
    struct HandleToPropHandle<AsterTopology::FaceHandle, T> {
        using type = AsterTopology::FPropHandleT<T>;
    };

    template<typename T>
    struct HandleToPropHandle<void, T> {
        using type = AsterTopology::MPropHandleT<T>;
    };

} // namespace AsterTopology

#endif // HANDLETOPROPHANDLE_HH_