// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/texture/texture_asset.hpp"

#include <algorithm>

namespace aster {

std::string_view textureKindName(const TextureKind kind) {
  switch (kind) {
  case TextureKind::Unknown:
    return "unknown";
  case TextureKind::Albedo:
    return "albedo";
  case TextureKind::Normal:
    return "normal";
  case TextureKind::Roughness:
    return "roughness";
  case TextureKind::Metallic:
    return "metallic";
  case TextureKind::Occlusion:
    return "occlusion";
  case TextureKind::MetallicRoughness:
    return "metallic_roughness";
  case TextureKind::ORM:
    return "orm";
  case TextureKind::Height:
    return "height";
  case TextureKind::Emissive:
    return "emissive";
  case TextureKind::Wetness:
    return "wetness";
  case TextureKind::Opacity:
    return "opacity";
  case TextureKind::Mask:
    return "mask";
  }
  return "unknown";
}

std::string_view textureColorSpaceName(const TextureColorSpace color_space) {
  switch (color_space) {
  case TextureColorSpace::Linear:
    return "linear";
  case TextureColorSpace::SRGB:
    return "srgb";
  }
  return "linear";
}

TextureKind textureKindForRole(const std::string_view role) {
  if (role == "albedo" || role == "base_color" || role == "baseColor") {
    return TextureKind::Albedo;
  }
  if (role == "normal") {
    return TextureKind::Normal;
  }
  if (role == "roughness") {
    return TextureKind::Roughness;
  }
  if (role == "metallic") {
    return TextureKind::Metallic;
  }
  if (role == "ao" || role == "occlusion" || role == "ambient_occlusion") {
    return TextureKind::Occlusion;
  }
  if (role == "metallic_roughness") {
    return TextureKind::MetallicRoughness;
  }
  if (role == "orm") {
    return TextureKind::ORM;
  }
  if (role == "height" || role == "displacement") {
    return TextureKind::Height;
  }
  if (role == "emissive") {
    return TextureKind::Emissive;
  }
  if (role == "wetness") {
    return TextureKind::Wetness;
  }
  if (role == "opacity" || role == "alpha") {
    return TextureKind::Opacity;
  }
  if (role.find("mask") != std::string_view::npos || role == "moss" || role == "crack") {
    return TextureKind::Mask;
  }
  return TextureKind::Unknown;
}

TextureColorSpace defaultTextureColorSpace(const TextureKind kind) {
  return kind == TextureKind::Albedo || kind == TextureKind::Emissive ? TextureColorSpace::SRGB
                                                                     : TextureColorSpace::Linear;
}

std::uint32_t textureMipCount(std::uint32_t width, std::uint32_t height) {
  std::uint32_t levels = 1u;
  width = std::max(width, 1u);
  height = std::max(height, 1u);
  while (width > 1u || height > 1u) {
    width = std::max(width / 2u, 1u);
    height = std::max(height / 2u, 1u);
    ++levels;
  }
  return levels;
}

} // namespace aster
