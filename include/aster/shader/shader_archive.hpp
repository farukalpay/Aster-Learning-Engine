// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#pragma once

#include "aster/shader/shader_compiler.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace aster {

struct ShaderArchiveDependencyHash {
  std::string logical_path;
  std::filesystem::path resolved_path;
  std::uint64_t source_hash = 0u;
};

struct ShaderArchiveEntry {
  ShaderBackend backend = ShaderBackend::SoftwareReference;
  StableShaderKey stable_key{};
  std::string permutation_tag;
  std::vector<ShaderArchiveDependencyHash> dependencies;
  ShaderReflection reflection;
  std::uint64_t preprocessed_source_hash = 0u;
  std::uint64_t payload_hash = 0u;
  std::vector<std::uint8_t> bytecode_payload;
  std::uint32_t request_count = 0u;
  std::uint32_t release_count = 0u;
};

struct ShaderArchiveManifest {
  std::uint32_t schema_version = 1u;
  std::vector<ShaderArchiveEntry> entries;
  std::uint64_t manifest_hash = 0u;
};

struct ShaderArchiveLoadResult {
  ShaderArchiveManifest manifest;
  std::vector<std::string> diagnostics;

  [[nodiscard]] bool ok() const;
};

class ShaderArchive {
public:
  bool add(ShaderArchiveEntry entry);
  [[nodiscard]] bool contains(ShaderBackend backend, std::uint64_t stable_key_hash) const;
  [[nodiscard]] const ShaderArchiveEntry *find(ShaderBackend backend,
                                               std::uint64_t stable_key_hash) const;
  ShaderArchiveEntry *request(ShaderBackend backend, std::uint64_t stable_key_hash);
  bool release(ShaderBackend backend, std::uint64_t stable_key_hash);
  [[nodiscard]] ShaderArchiveManifest manifest() const;
  bool saveManifest(const std::filesystem::path &path) const;

private:
  std::vector<ShaderArchiveEntry> entries_;
};

[[nodiscard]] ShaderArchiveEntry shaderArchiveEntryFromCompileResult(
    const ShaderCompileRequest &request, const ShaderCompileResult &result,
    std::vector<std::uint8_t> bytecode_payload = {});
[[nodiscard]] std::uint64_t shaderArchiveManifestHash(const ShaderArchiveManifest &manifest);
[[nodiscard]] std::string serializeShaderArchiveManifest(const ShaderArchiveManifest &manifest);
[[nodiscard]] ShaderArchiveLoadResult
loadShaderArchiveManifest(const std::filesystem::path &path);

} // namespace aster
