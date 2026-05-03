//=============================================================================
//
//  CLASS mostream - IMPLEMENTATION
//
//=============================================================================


//== INCLUDES =================================================================

#include <AsterTopology/Core/System/omstream.hh>
#include <iostream>


//== IMPLEMENTATION ========================================================== 


AsterTopology::mostream& omlog() 
{
  static bool initialized = false;
  static AsterTopology::mostream mystream;
  if (!initialized)
  {
    mystream.connect(std::clog);
#ifdef NDEBUG
    mystream.disable();
#endif
    initialized = true;
  }
  return mystream;
}


AsterTopology::mostream& omout() 
{
  static bool initialized = false;
  static AsterTopology::mostream mystream;
  if (!initialized)
  {
    mystream.connect(std::cout);
    initialized = true;
  }
  return mystream;
}


AsterTopology::mostream& omerr() 
{
  static bool initialized = false;
  static AsterTopology::mostream mystream;
  if (!initialized)
  {
    mystream.connect(std::cerr);
    initialized = true;
  }
  return mystream;
}


//=============================================================================
