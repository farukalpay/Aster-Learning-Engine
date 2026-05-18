// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace aster {

enum class TextureColorSpace {
  Linear,
  SRGB,
};

enum class TextureKind {
  Unknown,
  Albedo,
  Normal,
  Roughness,
  Metallic,
  Occlusion,
  MetallicRoughness,
  ORM,
  Height,
  Emissive,
  Wetness,
  Opacity,
  Mask,
};

enum class TextureCompression {
  None,
  Ktx2Basis,
};

struct TextureMipInfo {
  std::uint32_t width = 1u;
  std::uint32_t height = 1u;
  std::uint64_t byte_offset = 0u;
  std::uint64_t byte_length = 0u;
};

struct TextureAssetMetadata {
  std::filesystem::path source_path;
  std::filesystem::path baked_path;
  TextureKind kind = TextureKind::Unknown;
  TextureColorSpace color_space = TextureColorSpace::Linear;
  TextureCompression compression = TextureCompression::None;
  std::uint32_t width = 0u;
  std::uint32_t height = 0u;
  std::vector<TextureMipInfo> mips;
  bool valid = false;
  std::vector<std::string> diagnostics;
};

[[nodiscard]] std::string_view textureKindName(TextureKind kind);
[[nodiscard]] std::string_view textureColorSpaceName(TextureColorSpace color_space);
[[nodiscard]] TextureKind textureKindForRole(std::string_view role);
[[nodiscard]] TextureColorSpace defaultTextureColorSpace(TextureKind kind);
[[nodiscard]] std::uint32_t textureMipCount(std::uint32_t width, std::uint32_t height);

} // namespace aster
