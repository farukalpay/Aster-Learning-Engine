
//=============================================================================
//
//  Helper Functions for generating a random number between 0.0 and 1.0 with
//  a garantueed resolution
//
//=============================================================================


//== INCLUDES =================================================================


#include <AsterTopology/Core/Utils/RandomNumberGenerator.hh>


//== NAMESPACES ===============================================================


namespace AsterTopology {


//== IMPLEMENTATION ===========================================================

RandomNumberGenerator::RandomNumberGenerator(const size_t _resolution) :
  resolution_(_resolution),
  iterations_(1),
  maxNum_(RAND_MAX + 1.0)
{
  double tmp = double(resolution_);
  while (tmp > (double(RAND_MAX) + 1.0) ) {
    iterations_++;
    tmp /= (double(RAND_MAX) + 1.0);
  }

  for ( unsigned int i = 0 ; i < iterations_ - 1; ++i ) {
    maxNum_ *= (RAND_MAX + 1.0);
  }
}

//-----------------------------------------------------------------------------

double RandomNumberGenerator::getRand() const {
  double randNum = 0.0;
  for ( unsigned int i = 0 ; i < iterations_; ++i ) {
    randNum *= (RAND_MAX + 1.0);
    randNum += rand();
  }

  return randNum / maxNum_;
}

double RandomNumberGenerator::resolution() const {
  return maxNum_;
}

//=============================================================================
} // namespace AsterTopology
//=============================================================================
