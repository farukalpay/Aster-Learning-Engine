// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#include "aster/shader/shader_archive.hpp"

#include "aster/asset/json_document.hpp"

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <sstream>
#include <utility>

namespace aster {
namespace {

[[nodiscard]] std::string escapeJson(const std::string_view value) {
  std::string escaped;
  escaped.reserve(value.size() + 8u);
  for (const char c : value) {
    switch (c) {
    case '\\':
      escaped += "\\\\";
      break;
    case '"':
      escaped += "\\\"";
      break;
    case '\n':
      escaped += "\\n";
      break;
    case '\r':
      escaped += "\\r";
      break;
    case '\t':
      escaped += "\\t";
      break;
    default:
      escaped.push_back(c);
      break;
    }
  }
  return escaped;
}

[[nodiscard]] std::string shaderResourceKindName(const ShaderResourceKind kind) {
  switch (kind) {
  case ShaderResourceKind::UniformBuffer:
    return "uniform-buffer";
  case ShaderResourceKind::Texture:
    return "texture";
  case ShaderResourceKind::Sampler:
    return "sampler";
  }
  return "uniform-buffer";
}

[[nodiscard]] ShaderResourceKind parseShaderResourceKind(const std::string_view value) {
  if (value == "texture") {
    return ShaderResourceKind::Texture;
  }
  if (value == "sampler") {
    return ShaderResourceKind::Sampler;
  }
  return ShaderResourceKind::UniformBuffer;
}

[[nodiscard]] ShaderBackend parseBackend(const std::string_view value) {
  if (value == shaderBackendName(ShaderBackend::MetalMSL)) {
    return ShaderBackend::MetalMSL;
  }
  if (value == shaderBackendName(ShaderBackend::D3D12HLSL)) {
    return ShaderBackend::D3D12HLSL;
  }
  return ShaderBackend::SoftwareReference;
}

[[nodiscard]] std::uint64_t payloadHash(const std::vector<std::uint8_t> &payload) {
  std::uint64_t hash = 1469598103934665603ull;
  for (const std::uint8_t byte : payload) {
    hash = stableShaderHashAppend(hash, static_cast<std::uint64_t>(byte));
  }
  return hash;
}

[[nodiscard]] const asset_json::Value *arrayField(const asset_json::Value &json,
                                                  const std::string_view key) {
  const asset_json::Value *value = json.find(key);
  return value != nullptr && value->kind == asset_json::Value::Kind::Array ? value : nullptr;
}

[[nodiscard]] std::string readText(const std::filesystem::path &path) {
  std::ifstream file(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

} // namespace

bool ShaderArchiveLoadResult::ok() const {
  return diagnostics.empty();
}

bool ShaderArchive::add(ShaderArchiveEntry entry) {
  if (entry.stable_key.hash == 0u) {
    return false;
  }
  auto found = std::find_if(entries_.begin(), entries_.end(),
                            [&entry](const ShaderArchiveEntry &candidate) {
                              return candidate.backend == entry.backend &&
                                     candidate.stable_key.hash == entry.stable_key.hash;
                            });
  if (entry.payload_hash == 0u && !entry.bytecode_payload.empty()) {
    entry.payload_hash = payloadHash(entry.bytecode_payload);
  }
  if (found == entries_.end()) {
    entries_.push_back(std::move(entry));
  } else {
    *found = std::move(entry);
  }
  return true;
}

bool ShaderArchive::contains(const ShaderBackend backend,
                             const std::uint64_t stable_key_hash) const {
  return find(backend, stable_key_hash) != nullptr;
}

const ShaderArchiveEntry *ShaderArchive::find(const ShaderBackend backend,
                                              const std::uint64_t stable_key_hash) const {
  const auto found = std::find_if(entries_.begin(), entries_.end(),
                                  [backend, stable_key_hash](const ShaderArchiveEntry &entry) {
                                    return entry.backend == backend &&
                                           entry.stable_key.hash == stable_key_hash;
                                  });
  return found == entries_.end() ? nullptr : &*found;
}

ShaderArchiveEntry *ShaderArchive::request(const ShaderBackend backend,
                                           const std::uint64_t stable_key_hash) {
  auto found = std::find_if(entries_.begin(), entries_.end(),
                            [backend, stable_key_hash](const ShaderArchiveEntry &entry) {
                              return entry.backend == backend &&
                                     entry.stable_key.hash == stable_key_hash;
                            });
  if (found == entries_.end()) {
    return nullptr;
  }
  ++found->request_count;
  return &*found;
}

bool ShaderArchive::release(const ShaderBackend backend, const std::uint64_t stable_key_hash) {
  auto *entry = request(backend, stable_key_hash);
  if (entry == nullptr) {
    return false;
  }
  --entry->request_count;
  ++entry->release_count;
  return true;
}

ShaderArchiveManifest ShaderArchive::manifest() const {
  ShaderArchiveManifest manifest;
  manifest.entries = entries_;
  std::sort(manifest.entries.begin(), manifest.entries.end(),
            [](const ShaderArchiveEntry &lhs, const ShaderArchiveEntry &rhs) {
              if (lhs.backend != rhs.backend) {
                return static_cast<std::uint32_t>(lhs.backend) <
                       static_cast<std::uint32_t>(rhs.backend);
              }
              return lhs.stable_key.hash < rhs.stable_key.hash;
            });
  manifest.manifest_hash = shaderArchiveManifestHash(manifest);
  return manifest;
}

bool ShaderArchive::saveManifest(const std::filesystem::path &path) const {
  if (!path.parent_path().empty()) {
    std::filesystem::create_directories(path.parent_path());
  }
  std::ofstream file(path, std::ios::binary);
  if (!file) {
    return false;
  }
  file << serializeShaderArchiveManifest(manifest());
  return file.good();
}

ShaderArchiveEntry shaderArchiveEntryFromCompileResult(
    const ShaderCompileRequest &request, const ShaderCompileResult &result,
    std::vector<std::uint8_t> bytecode_payload) {
  ShaderArchiveEntry entry;
  entry.backend = request.backend;
  entry.stable_key = result.stable_key;
  entry.permutation_tag = request.variant.tag;
  entry.reflection = result.reflection;
  entry.preprocessed_source_hash = result.preprocessed_source_hash;
  entry.bytecode_payload = std::move(bytecode_payload);
  entry.payload_hash = payloadHash(entry.bytecode_payload);
  for (const ShaderPreprocessDependency &dependency : result.dependencies) {
    entry.dependencies.push_back({.logical_path = dependency.logical_path,
                                  .resolved_path = dependency.resolved_path,
                                  .source_hash = dependency.source_hash});
  }
  return entry;
}

std::uint64_t shaderArchiveManifestHash(const ShaderArchiveManifest &manifest) {
  std::uint64_t hash = 1469598103934665603ull;
  hash = stableShaderHashAppend(hash, manifest.schema_version);
  for (const ShaderArchiveEntry &entry : manifest.entries) {
    hash = stableShaderHashAppend(hash, static_cast<std::uint64_t>(entry.backend));
    hash = stableShaderHashAppend(hash, entry.stable_key.hash);
    hash = stableShaderHashAppend(hash, entry.stable_key.permutation_id);
    hash = stableShaderHashAppend(hash, entry.permutation_tag);
    hash = stableShaderHashAppend(hash, entry.preprocessed_source_hash);
    hash = stableShaderHashAppend(hash, entry.payload_hash);
    for (const ShaderArchiveDependencyHash &dependency : entry.dependencies) {
      hash = stableShaderHashAppend(hash, dependency.logical_path);
      hash = stableShaderHashAppend(hash, dependency.source_hash);
    }
    for (const ShaderResourceBinding &binding : entry.reflection.resources) {
      hash = stableShaderHashAppend(hash, binding.name);
      hash = stableShaderHashAppend(hash, static_cast<std::uint64_t>(binding.kind));
      hash = stableShaderHashAppend(hash, binding.binding);
      hash = stableShaderHashAppend(hash, binding.count);
    }
  }
  return hash;
}

std::string serializeShaderArchiveManifest(const ShaderArchiveManifest &manifest) {
  std::ostringstream out;
  out << "{\n";
  out << "  \"schema_version\": " << manifest.schema_version << ",\n";
  out << "  \"manifest_hash\": \"" << shaderStableHex(manifest.manifest_hash) << "\",\n";
  out << "  \"entries\": [\n";
  for (std::size_t entry_index = 0u; entry_index < manifest.entries.size(); ++entry_index) {
    const ShaderArchiveEntry &entry = manifest.entries[entry_index];
    out << "    {\n";
    out << "      \"backend\": \"" << shaderBackendName(entry.backend) << "\",\n";
    out << "      \"stable_key\": \"" << shaderStableHex(entry.stable_key.hash) << "\",\n";
    out << "      \"permutation_id\": " << entry.stable_key.permutation_id << ",\n";
    out << "      \"permutation_tag\": \"" << escapeJson(entry.permutation_tag) << "\",\n";
    out << "      \"preprocessed_source_hash\": \""
        << shaderStableHex(entry.preprocessed_source_hash) << "\",\n";
    out << "      \"payload_hash\": \"" << shaderStableHex(entry.payload_hash) << "\",\n";
    out << "      \"request_count\": " << entry.request_count << ",\n";
    out << "      \"release_count\": " << entry.release_count << ",\n";
    out << "      \"dependencies\": [";
    for (std::size_t i = 0u; i < entry.dependencies.size(); ++i) {
      const ShaderArchiveDependencyHash &dependency = entry.dependencies[i];
      out << (i == 0u ? "\n" : ",\n");
      out << "        {\"path\": \"" << escapeJson(dependency.logical_path)
          << "\", \"resolved\": \"" << escapeJson(dependency.resolved_path.generic_string())
          << "\", \"hash\": \"" << shaderStableHex(dependency.source_hash) << "\"}";
    }
    out << (entry.dependencies.empty() ? "" : "\n      ") << "],\n";
    out << "      \"reflection\": [";
    for (std::size_t i = 0u; i < entry.reflection.resources.size(); ++i) {
      const ShaderResourceBinding &binding = entry.reflection.resources[i];
      out << (i == 0u ? "\n" : ",\n");
      out << "        {\"name\": \"" << escapeJson(binding.name) << "\", \"kind\": \""
          << shaderResourceKindName(binding.kind) << "\", \"binding\": " << binding.binding
          << ", \"count\": " << binding.count << "}";
    }
    out << (entry.reflection.resources.empty() ? "" : "\n      ") << "]\n";
    out << "    }" << (entry_index + 1u == manifest.entries.size() ? "\n" : ",\n");
  }
  out << "  ]\n";
  out << "}\n";
  return out.str();
}

ShaderArchiveLoadResult loadShaderArchiveManifest(const std::filesystem::path &path) {
  ShaderArchiveLoadResult result;
  try {
    const asset_json::Value root = asset_json::parse(readText(path));
    result.manifest.schema_version = asset_json::u32Or(root, "schema_version", 1u);
    if (const asset_json::Value *entries = arrayField(root, "entries")) {
      for (const asset_json::Value &entry_json : entries->array) {
        ShaderArchiveEntry entry;
        entry.backend = parseBackend(asset_json::textOr(entry_json, "backend"));
        entry.stable_key.backend = std::string(shaderBackendName(entry.backend));
        entry.stable_key.hash = asset_json::u64Or(entry_json, "stable_key");
        if (entry.stable_key.hash == 0u) {
          const std::string stable_hex = asset_json::textOr(entry_json, "stable_key");
          entry.stable_key.hash =
              stable_hex.rfind("0x", 0u) == 0u
                  ? std::stoull(stable_hex.substr(2u), nullptr, 16)
                  : std::stoull(stable_hex.empty() ? "0" : stable_hex);
        }
        entry.stable_key.permutation_id = asset_json::u64Or(entry_json, "permutation_id");
        entry.permutation_tag = asset_json::textOr(entry_json, "permutation_tag");
        const std::string source_hash =
            asset_json::textOr(entry_json, "preprocessed_source_hash");
        entry.preprocessed_source_hash =
            source_hash.rfind("0x", 0u) == 0u
                ? std::stoull(source_hash.substr(2u), nullptr, 16)
                : asset_json::u64Or(entry_json, "preprocessed_source_hash");
        const std::string payload_hash = asset_json::textOr(entry_json, "payload_hash");
        entry.payload_hash =
            payload_hash.rfind("0x", 0u) == 0u
                ? std::stoull(payload_hash.substr(2u), nullptr, 16)
                : asset_json::u64Or(entry_json, "payload_hash");
        entry.request_count = asset_json::u32Or(entry_json, "request_count");
        entry.release_count = asset_json::u32Or(entry_json, "release_count");
        if (const asset_json::Value *dependencies = arrayField(entry_json, "dependencies")) {
          for (const asset_json::Value &dependency_json : dependencies->array) {
            const std::string hash_text = asset_json::textOr(dependency_json, "hash");
            const std::uint64_t hash =
                hash_text.rfind("0x", 0u) == 0u
                    ? std::stoull(hash_text.substr(2u), nullptr, 16)
                    : asset_json::u64Or(dependency_json, "hash");
            entry.dependencies.push_back(
                {.logical_path = asset_json::textOr(dependency_json, "path"),
                 .resolved_path = asset_json::textOr(dependency_json, "resolved"),
                 .source_hash = hash});
          }
        }
        if (const asset_json::Value *reflection = arrayField(entry_json, "reflection")) {
          for (const asset_json::Value &binding_json : reflection->array) {
            entry.reflection.resources.push_back(
                {.name = asset_json::textOr(binding_json, "name"),
                 .kind = parseShaderResourceKind(asset_json::textOr(binding_json, "kind")),
                 .binding = asset_json::u32Or(binding_json, "binding"),
                 .count = asset_json::u32Or(binding_json, "count", 1u)});
          }
        }
        result.manifest.entries.push_back(std::move(entry));
      }
    }
    result.manifest.manifest_hash = shaderArchiveManifestHash(result.manifest);
  } catch (const std::exception &error) {
    result.diagnostics.push_back(error.what());
  }
  return result;
}

} // namespace aster
