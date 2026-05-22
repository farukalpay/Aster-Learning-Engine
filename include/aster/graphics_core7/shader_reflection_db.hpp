// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#pragma once

#include "aster/shader/shader_reflection.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>

namespace aster::graphics_core7 {

struct ShaderReflectionRecord {
  std::uint64_t shader_key = 0u;
  std::string backend;
  ShaderReflection reflection{};
  std::uint64_t layout_hash = 0u;
};

class ShaderReflectionDb {
public:
  void upsert(ShaderReflectionRecord record);
  [[nodiscard]] const ShaderReflectionRecord *find(std::uint64_t shader_key,
                                                   std::string_view backend) const;
  [[nodiscard]] std::size_t size() const noexcept;

private:
  std::unordered_map<std::string, ShaderReflectionRecord> records_;
};

} // namespace aster::graphics_core7
