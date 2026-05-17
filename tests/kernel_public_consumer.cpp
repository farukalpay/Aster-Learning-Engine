// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/kernel/api.hpp"

#include <cassert>

int main() {
  const AsterAbiVersion version = aster::kernel::abiVersion();
  assert(version.major == ASTER_KERNEL_ABI_MAJOR);

  auto engine = aster::kernel::Engine::create();
  assert(engine);
  assert(engine.value().valid());
  assert(engine.value().lastStatus());
  return 0;
}
