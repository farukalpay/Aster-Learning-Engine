
//=============================================================================
//
//  Helper Functions for generating a random number between 0.0 and 1.0 with
//  a garantueed resolution
//
//=============================================================================


#ifndef ASTER_TOPOLOGY_UTILS_RANDOMNUMBERGENERATOR_HH
#define ASTER_TOPOLOGY_UTILS_RANDOMNUMBERGENERATOR_HH


//== INCLUDES =================================================================


#include <AsterTopology/Core/System/config.h>
#include <cstdlib>



//== NAMESPACES ===============================================================


namespace AsterTopology {


//=============================================================================


/**  Generate a random number between 0.0 and 1.0 with a guaranteed resolution
 *   ( Number of possible values )
 *
 * Especially useful on windows, as there MAX_RAND is often only 32k which is
 * not enough resolution for a lot of applications
 */
class ASTER_TOPOLOGYDLLEXPORT RandomNumberGenerator
{
public:

  /** \brief Constructor
  *
  * @param _resolution specifies the desired resolution for the random number generated
  */
  explicit RandomNumberGenerator(const size_t _resolution);

  /// returns a random double between 0.0 and 1.0 with a guaranteed resolution
  double getRand() const;

  double resolution() const;

private:

  /// desired resolution
  const size_t resolution_;

  /// number of "blocks" of RAND_MAX that make up the desired _resolution
  size_t iterations_;

  /// maximum random number generated, which is used for normalization
  double maxNum_;
};

//=============================================================================
} // namespace AsterTopology
//=============================================================================
#endif // ASTER_TOPOLOGY_UTILS_RANDOMNUMBERGENERATOR_HH defined
//=============================================================================

