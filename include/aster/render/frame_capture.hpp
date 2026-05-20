// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include <filesystem>

namespace aster {

void writeFramebufferPpm(const std::filesystem::path &path, int width, int height);
void writeFramebufferPng(const std::filesystem::path &path, int width, int height);

} // namespace aster
