
//=============================================================================
//
//  Implements a simple singleton template
//
//=============================================================================


#define ASTER_TOPOLOGY_SINGLETON_C


//== INCLUDES =================================================================


// header
#include <AsterTopology/Core/Utils/SingletonT.hh>


//== NAMESPACES ===============================================================


namespace AsterTopology {


//== SINGLETON'S DATA =========================================================


template <class T> 
T* SingletonT<T>::pInstance__ = 0;

template <class T>
bool SingletonT<T>::destroyed__ = false;


//=============================================================================
} // namespace AsterTopology
//=============================================================================
