// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#include "aster/material/material_instance.hpp"

namespace aster {

MaterialParameterBlock resolveMaterialParameters(const MaterialAsset &asset,
                                                 const MaterialInstance *instance) {
  MaterialParameterBlock resolved = materialParameterBlockForAsset(asset);
  if (instance == nullptr) {
    return resolved;
  }
  for (const auto &[name, value] : instance->overrides.scalars) {
    resolved.scalars[name] = value;
  }
  for (const auto &[name, value] : instance->overrides.colors) {
    resolved.colors[name] = value;
  }
  return resolved;
}

} // namespace aster
