// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#pragma once

#include <filesystem>

namespace aster {

void writeFramebufferPpm(const std::filesystem::path &path, int width, int height);
void writeFramebufferPng(const std::filesystem::path &path, int width, int height);

} // namespace aster
