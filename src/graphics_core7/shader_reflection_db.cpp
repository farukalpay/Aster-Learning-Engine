// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#include "aster/graphics_core7/shader_reflection_db.hpp"

#include <utility>

namespace aster::graphics_core7 {
namespace {

[[nodiscard]] std::string keyFor(const std::uint64_t shader_key, const std::string_view backend) {
  return std::to_string(shader_key) + ":" + std::string(backend);
}

} // namespace

void ShaderReflectionDb::upsert(ShaderReflectionRecord record) {
  records_[keyFor(record.shader_key, record.backend)] = std::move(record);
}

const ShaderReflectionRecord *ShaderReflectionDb::find(const std::uint64_t shader_key,
                                                       const std::string_view backend) const {
  const auto found = records_.find(keyFor(shader_key, backend));
  return found == records_.end() ? nullptr : &found->second;
}

std::size_t ShaderReflectionDb::size() const noexcept {
  return records_.size();
}

} // namespace aster::graphics_core7
