// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/render/material_compiler.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>

namespace aster {
namespace {

void appendByte(std::uint64_t &hash, const std::uint8_t byte) {
  hash ^= byte;
  hash *= 1099511628211ull;
}

void appendU32(std::uint64_t &hash, const std::uint32_t value) {
  for (int byte_index = 0; byte_index < 4; ++byte_index) {
    appendByte(hash, static_cast<std::uint8_t>(value >> (byte_index * 8)));
  }
}

void appendFloat(std::uint64_t &hash, const float value) {
  std::uint32_t bits = 0u;
  std::memcpy(&bits, &value, sizeof(bits));
  appendU32(hash, bits);
}

template <typename Enum> void appendEnum(std::uint64_t &hash, const Enum value) {
  static_assert(std::is_enum_v<Enum>);
  appendU32(hash, static_cast<std::uint32_t>(value));
}

void appendKey(std::uint64_t &hash, const std::uint32_t value) {
  appendU32(hash, value);
}

void appendString(std::uint64_t &hash, const std::string_view value) {
  for (const char byte : value) {
    appendByte(hash, static_cast<std::uint8_t>(static_cast<unsigned char>(byte)));
  }
}

void appendU64(std::uint64_t &hash, const std::uint64_t value) {
  for (int byte_index = 0; byte_index < 8; ++byte_index) {
    appendByte(hash, static_cast<std::uint8_t>(value >> (byte_index * 8)));
  }
}

} // namespace

bool materialHasProceduralPermutation(const Material &material) {
  return material.surface_pattern != SurfacePattern::None ||
         material.procedural.macro_variation > 0.0f ||
         material.procedural.micro_normal_strength > 0.0f ||
         material.procedural.roughness_variation > 0.0f ||
         material.procedural.wetness > 0.0f || material.procedural.height_shading > 0.0f;
}

std::uint64_t materialPermutationKey(const Material &material,
                                     const bool has_texture_dependencies) {
  return compileMaterialForRendering(material, has_texture_dependencies).permutation_key;
}

CompiledMaterial compileMaterialForRendering(const Material &material,
                                             const bool has_texture_dependencies,
                                             const std::string_view material_id) {
  CompiledMaterial compiled;
  compiled.material = material;

  if (has_texture_dependencies) {
    compiled.permutation_flags |= materialPermutationFlagBit(MaterialPermutationFlag::Textured);
  }
  if (materialHasProceduralPermutation(material)) {
    compiled.permutation_flags |= materialPermutationFlagBit(MaterialPermutationFlag::Procedural);
  }
  if (isMaterialTranslucent(material)) {
    compiled.permutation_flags |= materialPermutationFlagBit(MaterialPermutationFlag::Transparent);
  }
  if (isDoubleSidedMaterial(material)) {
    compiled.permutation_flags |= materialPermutationFlagBit(MaterialPermutationFlag::DoubleSided);
  }
  if (materialWritesDepth(material)) {
    compiled.permutation_flags |= materialPermutationFlagBit(MaterialPermutationFlag::DepthWrite);
  }
  if (material.surface_pattern == SurfacePattern::ContactShadow) {
    compiled.permutation_flags |= materialPermutationFlagBit(MaterialPermutationFlag::ContactShadow);
  }
  if (material.shader_variant_key != 0u || !material.asset_id.empty()) {
    compiled.permutation_flags |= materialPermutationFlagBit(MaterialPermutationFlag::ShaderVariant);
  }

  std::uint64_t hash = 1469598103934665603ull;
  appendKey(hash, compiled.permutation_flags);
  appendEnum(hash, material.render_role);
  appendEnum(hash, material.alpha_mode);
  appendEnum(hash, material.depth_write);
  appendEnum(hash, material.cull_mode);
  appendEnum(hash, material.surface_pattern);
  appendFloat(hash, material.pattern_scale.x);
  appendFloat(hash, material.pattern_scale.y);
  appendFloat(hash, material.pattern_depth);
  appendFloat(hash, material.pattern_contrast);
  appendFloat(hash, material.pattern_mortar);
  appendFloat(hash, material.procedural.macro_variation);
  appendFloat(hash, material.procedural.micro_normal_strength);
  appendFloat(hash, material.procedural.roughness_variation);
  appendFloat(hash, material.procedural.wetness);
  appendFloat(hash, material.procedural.height_shading);
  appendU64(hash, material.shader_variant_key);
  appendString(hash, material.asset_id);
  appendString(hash, material_id);
  compiled.permutation_key = hash;
  compiled.material.compiled_permutation_key = compiled.permutation_key;
  compiled.material.compiled_permutation_flags = compiled.permutation_flags;
  compiled.pipeline_tag = materialPipelineTag(compiled);
  return compiled;
}

std::string materialPipelineTag(const CompiledMaterial &material) {
  std::string tag;
  tag.reserve(96u);
  tag += isMaterialTranslucent(material.material) ? "transparent" : "opaque";
  tag += materialWritesDepth(material.material) ? ".depth-write" : ".depth-read";
  tag += isDoubleSidedMaterial(material.material) ? ".double-sided" : ".culled";
  if ((material.permutation_flags & materialPermutationFlagBit(MaterialPermutationFlag::Textured)) !=
      0u) {
    tag += ".textured";
  }
  if ((material.permutation_flags &
       materialPermutationFlagBit(MaterialPermutationFlag::Procedural)) != 0u) {
    tag += ".procedural";
  }
  if ((material.permutation_flags &
       materialPermutationFlagBit(MaterialPermutationFlag::ContactShadow)) != 0u) {
    tag += ".contact-shadow";
  }
  if ((material.permutation_flags &
       materialPermutationFlagBit(MaterialPermutationFlag::ShaderVariant)) != 0u) {
    tag += ".shader-variant";
  }
  return tag;
}

MaterialPermutationArtifact materialPermutationArtifactFor(
    const CompiledMaterial &material, const std::string_view backend_name,
    const std::string_view shader_variant_tag, const std::string_view fallback_reason) {
  MaterialPermutationArtifact artifact;
  artifact.permutation_key = material.permutation_key;
  artifact.permutation_flags = material.permutation_flags;
  artifact.pipeline_tag = material.pipeline_tag;
  artifact.shader_variant_tag = shader_variant_tag;
  artifact.backend_name = backend_name;
  artifact.fallback_reason = fallback_reason;

  std::uint64_t source_hash = 1469598103934665603ull;
  appendU64(source_hash, material.permutation_key);
  appendKey(source_hash, material.permutation_flags);
  appendString(source_hash, material.pipeline_tag);
  appendString(source_hash, shader_variant_tag);
  artifact.source_hash = source_hash;

  std::uint64_t artifact_hash = 1469598103934665603ull;
  appendU64(artifact_hash, artifact.source_hash);
  appendString(artifact_hash, backend_name);
  appendString(artifact_hash, fallback_reason);
  artifact.artifact_hash = artifact_hash;
  return artifact;
}

} // namespace aster
