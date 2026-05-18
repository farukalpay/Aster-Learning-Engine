// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include <cstdint>
#include <string>

namespace aster::rhi {

enum class BackendKind : std::uint32_t {
  SoftwareReference,
  Metal,
  D3D12,
  Null,
  Unknown,
};

struct AdapterDesc {
  BackendKind backend = BackendKind::Unknown;
  std::string name;
  bool gpu = false;
  bool timestamp_queries = false;
};

} // namespace aster::rhi
