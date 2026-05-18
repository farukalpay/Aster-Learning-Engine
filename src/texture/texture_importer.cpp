// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/texture/texture_importer.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <fstream>

namespace aster {
namespace {

std::uint32_t readBe32(const std::array<unsigned char, 32> &bytes, const std::size_t offset) {
  return (static_cast<std::uint32_t>(bytes[offset]) << 24u) |
         (static_cast<std::uint32_t>(bytes[offset + 1u]) << 16u) |
         (static_cast<std::uint32_t>(bytes[offset + 2u]) << 8u) |
         static_cast<std::uint32_t>(bytes[offset + 3u]);
}

std::uint32_t readLe32(const std::array<unsigned char, 80> &bytes, const std::size_t offset) {
  return static_cast<std::uint32_t>(bytes[offset]) |
         (static_cast<std::uint32_t>(bytes[offset + 1u]) << 8u) |
         (static_cast<std::uint32_t>(bytes[offset + 2u]) << 16u) |
         (static_cast<std::uint32_t>(bytes[offset + 3u]) << 24u);
}

bool inspectPng(const std::filesystem::path &path, std::uint32_t &width,
                std::uint32_t &height) {
  std::ifstream file(path, std::ios::binary);
  std::array<unsigned char, 32> bytes{};
  file.read(reinterpret_cast<char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
  if (file.gcount() < 24) {
    return false;
  }
  constexpr std::array<unsigned char, 8> signature{0x89u, 'P', 'N', 'G', '\r', '\n', 0x1au, '\n'};
  if (!std::equal(signature.begin(), signature.end(), bytes.begin())) {
    return false;
  }
  width = readBe32(bytes, 16u);
  height = readBe32(bytes, 20u);
  return width > 0u && height > 0u;
}

bool inspectKtx2(const std::filesystem::path &path, std::uint32_t &width,
                 std::uint32_t &height, std::uint32_t &mips) {
  std::ifstream file(path, std::ios::binary);
  std::array<unsigned char, 80> bytes{};
  file.read(reinterpret_cast<char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
  if (file.gcount() < 68) {
    return false;
  }
  constexpr std::array<unsigned char, 12> signature{0xabu, 'K', 'T', 'X', ' ', '2',
                                                    '0',   0xbbu, '\r', '\n', 0x1au, '\n'};
  if (!std::equal(signature.begin(), signature.end(), bytes.begin())) {
    return false;
  }
  width = readLe32(bytes, 20u);
  height = readLe32(bytes, 24u);
  mips = std::max(readLe32(bytes, 40u), 1u);
  return width > 0u && height > 0u;
}

} // namespace

TextureAssetMetadata inspectTextureAsset(const std::filesystem::path &path, const TextureKind kind,
                                         const TextureImportOptions options) {
  TextureAssetMetadata metadata;
  metadata.source_path = path;
  metadata.kind = kind;
  metadata.color_space = defaultTextureColorSpace(kind);
  metadata.compression = options.compression;
  if (!std::filesystem::exists(path)) {
    metadata.diagnostics.push_back("texture is missing: " + path.string());
    metadata.valid = !options.require_existing_files;
    return metadata;
  }

  std::uint32_t mips = 0u;
  if (inspectKtx2(path, metadata.width, metadata.height, mips)) {
    metadata.compression = TextureCompression::Ktx2Basis;
  } else if (inspectPng(path, metadata.width, metadata.height)) {
    mips = options.generate_mips ? textureMipCount(metadata.width, metadata.height) : 1u;
  } else {
    metadata.diagnostics.push_back(
        "texture header is not PNG or KTX2; runtime decode may require the Rust importer");
    metadata.width = 1u;
    metadata.height = 1u;
    mips = 1u;
  }

  std::uint32_t width = std::max(metadata.width, 1u);
  std::uint32_t height = std::max(metadata.height, 1u);
  for (std::uint32_t level = 0u; level < std::max(mips, 1u); ++level) {
    metadata.mips.push_back({.width = width, .height = height});
    width = std::max(width / 2u, 1u);
    height = std::max(height / 2u, 1u);
  }
  metadata.valid = true;
  return metadata;
}

TextureSetValidation validateMaterialTextureSet(const MaterialAsset &asset,
                                                const std::filesystem::path &asset_root,
                                                const TextureImportOptions options) {
  TextureSetValidation validation;
  for (const auto &[role, slot] : asset.textures) {
    std::filesystem::path path = slot.uri;
    if (!path.is_absolute()) {
      path = asset_root.empty() ? asset.source_path.parent_path() / path : asset_root / path;
    }
    TextureAssetMetadata metadata =
        inspectTextureAsset(path, textureKindForRole(role), options);
    for (const std::string &diagnostic : metadata.diagnostics) {
      validation.diagnostics.push_back(role + ": " + diagnostic);
    }
    validation.ok = validation.ok && metadata.valid;
    validation.textures.push_back(std::move(metadata));
  }
  return validation;
}

} // namespace aster
