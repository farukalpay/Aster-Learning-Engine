
//=============================================================================
//
//  Utils for generic/generative programming
//
//=============================================================================

#ifndef ASTER_TOPOLOGY_GENPROG_HH
#define ASTER_TOPOLOGY_GENPROG_HH


//== INCLUDES =================================================================

#include <AsterTopology/Core/System/config.h>


//== NAMESPACES ===============================================================

namespace AsterTopology {

namespace GenProg  {
#ifndef DOXY_IGNORE_THIS

//== IMPLEMENTATION ===========================================================


/// This type maps \c true or \c false to different types.
template <bool b> struct Bool2Type { enum { my_bool = b }; };

/// This class generates different types from different \c int 's.
template <int i>  struct Int2Type  { enum { my_int = i }; };

/// Handy typedef for Bool2Type<true> classes
typedef Bool2Type<true> TrueType;

/// Handy typedef for Bool2Type<false> classes
typedef Bool2Type<false> FalseType;

//-----------------------------------------------------------------------------
/// compile time assertions 
template <bool Expr> struct AssertCompile;
template <> struct AssertCompile<true> {};



//--- Template "if" w/ partial specialization ---------------------------------
#if OM_PARTIAL_SPECIALIZATION


template <bool condition, class Then, class Else>
struct IF { typedef Then Result; };

/** Template \c IF w/ partial specialization
\code
typedef IF<bool, Then, Else>::Result  ResultType;
\endcode    
*/
template <class Then, class Else>
struct IF<false, Then, Else> { typedef Else Result; };





//--- Template "if" w/o partial specialization --------------------------------
#else


struct SelectThen 
{
  template <class Then, class Else> struct Select {
    typedef Then Result;
  };
};

struct SelectElse
{
  template <class Then, class Else> struct Select {
    typedef Else Result;
  };
};

template <bool condition> struct ChooseSelector {
  typedef SelectThen Result;
};

template <> struct ChooseSelector<false> {
  typedef SelectElse Result;
};


/** Template \c IF w/o partial specialization. Use it like
\code
typedef IF<bool, Then, Else>::Result  ResultType;
\endcode    
*/

template <bool condition, class Then, class Else>
class IF 
{ 
  typedef typename ChooseSelector<condition>::Result  Selector;
public:
  typedef typename Selector::template Select<Then, Else>::Result  Result;
};

#endif

//=============================================================================
#endif
} // namespace GenProg
} // namespace AsterTopology

#define assert_compile(EXPR)                GenProg::AssertCompile<(EXPR)>();

//=============================================================================
#endif // ASTER_TOPOLOGY_GENPROG_HH defined
//=============================================================================
