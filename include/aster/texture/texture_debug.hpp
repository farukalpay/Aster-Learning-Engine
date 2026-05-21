// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#pragma once

#include "aster/texture/texture_asset.hpp"

#include <string>

namespace aster {

[[nodiscard]] std::string textureDebugSummary(const TextureAssetMetadata &metadata);

} // namespace aster
