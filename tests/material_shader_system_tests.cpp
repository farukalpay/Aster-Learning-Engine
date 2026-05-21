// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/material/material_compiler.hpp"
#include "aster/material/material_graph.hpp"
#include "aster/material/material_lab.hpp"
#include "aster/asset/procedural_asset_graph.hpp"
#include "aster/math/color.hpp"
#include "aster/shader/shader_compiler.hpp"
#include "aster/shader/shader_hot_reload.hpp"
#include "aster/render/render_quality.hpp"
#include "aster/scene/scene.hpp"
#include "aster/texture/texture_atlas.hpp"
#include "aster/texture/texture_debug.hpp"
#include "aster/texture/texture_importer.hpp"
#include "aster/texture/runtime_texture.hpp"

#include <cassert>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <thread>
#include <utility>

#ifndef ASTER_SOURCE_DIR
#define ASTER_SOURCE_DIR "."
#endif

namespace {

std::filesystem::path tempDir() {
  const std::filesystem::path path =
      std::filesystem::temp_directory_path() / "aster_material_shader_system_tests";
  std::filesystem::remove_all(path);
  std::filesystem::create_directories(path);
  return path;
}

void writeText(const std::filesystem::path &path, const std::string &text) {
  std::ofstream file(path, std::ios::binary);
  file << text;
  assert(file.good());
}

void writeKtx2Header(const std::filesystem::path &path, const std::uint32_t width,
                     const std::uint32_t height, const std::uint32_t mip_count) {
  std::ofstream file(path, std::ios::binary);
  file.put(static_cast<char>(0xab));
  file.write("KTX 20", 6);
  file.put(static_cast<char>(0xbb));
  file.put('\r');
  file.put('\n');
  file.put(static_cast<char>(0x1a));
  file.put('\n');
  const auto write_le32 = [&file](const std::uint32_t value) {
    const char bytes[4] = {static_cast<char>(value & 0xffu),
                           static_cast<char>((value >> 8u) & 0xffu),
                           static_cast<char>((value >> 16u) & 0xffu),
                           static_cast<char>((value >> 24u) & 0xffu)};
    file.write(bytes, 4);
  };
  write_le32(37u);
  write_le32(1u);
  write_le32(width);
  write_le32(height);
  write_le32(0u);
  write_le32(0u);
  write_le32(1u);
  write_le32(mip_count);
  write_le32(1u);
  file.write("ASTER_TEST_PAYLOAD_PADDING_0000", 30);
  assert(file.good());
}

std::string readText(const std::filesystem::path &path) {
  std::ifstream file(path, std::ios::binary);
  assert(file.good());
  return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}

std::string sampleMaterialSource() {
  return R"mat(
material TestWetRock {
  schema_version: 1
	  name: "Test Wet Rock"
	  shading_model: LitPBR
	  surface_profile: stratified-rock
	  blend_mode: Masked
  cull_mode: None
  receives_decals: true
  receives_shadows: true

  textures {
    albedo: "rock_albedo.ktx2"
    normal: "rock_normal.ktx2"
    orm: "rock_orm.ktx2"
    height: "rock_height.ktx2"
    wetness: "wetness_mask.ktx2"
  }

  params {
    base_color_r: 0.24
    base_color_g: 0.21
    base_color_b: 0.18
    roughness: 0.81
    metallic: 0.02
    triplanar_scale: 3.25
    wetness_strength: 0.72
    micro_normal_strength: 0.40
    height_shading: 0.22
    physical_texel_density: 768.0
    height_normal_coupling: 0.82
    roughness_height_coupling: 0.58
    macro_frequency_breakup: 0.34
    micro_frequency_breakup: 0.48
  }

  features {
    triplanar: true
    normal_map: true
    parallax: true
  }

  layers {
    base: triplanar(albedo, normal, height)
    wet: height_blend(wetness, height, 0.7)
  }
}
)mat";
}

void testMaterialAssetParserAndCompiler() {
  const std::filesystem::path dir = tempDir();
  const std::filesystem::path material_path = dir / "wet_rock.astermat";
  writeText(material_path, sampleMaterialSource());

  const aster::MaterialAssetLoadResult loaded = aster::loadMaterialAsset(material_path);
  assert(loaded.ok());
  assert(loaded.value.id == "TestWetRock");
  assert(loaded.value.textures.size() == 5u);
  assert(loaded.value.layers.size() == 2u);
  assert(loaded.value.params.at("roughness") > 0.80f);

  const aster::MaterialFeatureSet features = aster::materialFeatureSet(loaded.value);
  assert(features.textured);
  assert(features.normal_map);
  assert(features.orm_texture);
  assert(features.height);
  assert(features.parallax);
  assert(features.triplanar);
  assert(features.decal_receiver);
  assert(features.alpha_clip);
  assert(features.double_sided);

  const aster::CompiledMaterialAsset compiled =
      aster::compileMaterialAssetForRendering(loaded.value);
  assert(compiled.variant.stable_hash != 0u);
  assert(compiled.variant.tag.find("LitPBR.Masked") != std::string::npos);
  assert(compiled.variant.tag.find("triplanar") != std::string::npos);
  assert(compiled.graph.nodes.size() == loaded.value.layers.size());
  assert(compiled.graph.nodes.front().op == aster::MaterialGraphOperation::TriplanarSample);
  assert(compiled.binding_layout.bindings.size() == loaded.value.textures.size() + 2u);
  assert(compiled.fallback_material.material.asset_id == loaded.value.id);
  assert(aster::exactEqual(compiled.fallback_material.material.base_color,
                           aster::LinearRgb{0.24f, 0.21f, 0.18f}));
  assert(compiled.fallback_material.material.double_sided);
  assert(compiled.fallback_material.material.receives_shadows);
	  assert(compiled.fallback_material.material.surface_profile ==
	         aster::MaterialSurfaceProfile::StratifiedRock);
	  assert(aster::resolveMaterialSurfaceProfile(compiled.fallback_material.material) ==
	         aster::MaterialSurfaceProfile::StratifiedRock);
  assert((compiled.fallback_material.permutation_flags &
          aster::materialPermutationFlagBit(aster::MaterialPermutationFlag::ShaderVariant)) != 0u);

  const aster::LinearRgb albedo_from_srgb = aster::srgbToLinear(aster::Srgb{0.5f, 0.5f, 0.5f});
  assert(albedo_from_srgb.x > 0.21f && albedo_from_srgb.x < 0.22f);

  const aster::MaterialGraph graph = aster::materialGraphForAsset(loaded.value);
  assert(graph.nodes.size() == 2u);
  assert(graph.nodes.front().operation == "triplanar");
  assert(graph.nodes.front().op == aster::MaterialGraphOperation::TriplanarSample);
  assert(graph.nodes.front().value_type == aster::MaterialGraphValueType::MaterialLayer);
  assert(aster::materialGraphOperationName(graph.nodes.back().op) == "height-blend");
}

void testBiologicalIntegumentMaterialAliases() {
  const aster::MaterialAssetLoadResult loaded = aster::parseMaterialAsset(R"mat(
material SkinFurAlias {
  schema_version: 1
  name: "Skin Fur Alias"
  shading_model: LitPBR
  surface_profile: skin-fur
  blend_mode: Opaque
  cull_mode: Back

  params {
    base_color_r: 0.36
    base_color_g: 0.28
    base_color_b: 0.19
    roughness: 0.86
    macro_variation: 0.42
    micro_normal_strength: 0.34
  }
}
)mat",
                                                          "skin_fur_alias.astermat");
  assert(loaded.ok());
  assert(loaded.value.surface_profile == aster::MaterialSurfaceProfile::BiologicalIntegument);
  const std::string serialized = aster::serializeMaterialAsset(loaded.value);
  assert(serialized.find("surface_profile: biological-integument") != std::string::npos);

  const aster::CompiledMaterialAsset compiled =
      aster::compileMaterialAssetForRendering(loaded.value);
  assert(compiled.fallback_material.material.surface_profile ==
         aster::MaterialSurfaceProfile::BiologicalIntegument);
  assert(aster::resolveMaterialSurfaceProfile(compiled.fallback_material.material) ==
         aster::MaterialSurfaceProfile::BiologicalIntegument);

  const aster::Material patterned =
      aster::makeMaterial({.surface_pattern = aster::SurfacePattern::BiologicalIntegument});
  assert(aster::resolveMaterialSurfaceProfile(patterned) ==
         aster::MaterialSurfaceProfile::BiologicalIntegument);

  const std::filesystem::path dir = tempDir();
  const std::filesystem::path path = dir / "integument.assetgraphbin";
  writeText(path, R"json({
  "schema_version": 1,
  "asset_guid": "graph-guid-integument",
  "id": "asset_graph.integument",
  "name": "Graph Integument",
  "kind": "asset_graph",
  "source_path": "integument.astergraph",
  "runtime_model": "runtime-procedural",
  "material": {
    "id": "material.graph_integument",
    "surface_profile": "biological-integument",
    "feature_mask": 1,
    "shader_variant_key": 19,
    "shader_variant_tag": "AssetGraph.material.graph_integument.runtime-procedural",
    "pipeline_tag": "material:material.graph_integument:biological-integument:runtime-procedural",
    "fallback": {
      "base_color": [0.36, 0.28, 0.19],
      "emission_color": [0.0, 0.0, 0.0],
      "roughness": 0.86,
      "metallic": 0.0,
      "emission_strength": 0.0,
      "opacity": 1.0,
      "double_sided": false,
      "alpha_mode": "Opaque",
      "receives_shadows": true,
      "surface_profile": "dermal-fur"
    },
    "params": { "macro_variation": 0.42, "micro_normal_strength": 0.34 },
    "features": { "triplanar": true }
  },
  "mesh": {
    "primitive": "cercopithecidae",
    "uv_policy": "anatomical-triplanar",
    "tangent_policy": "validate-or-generate",
    "collision_proxy": "anatomical-bounds",
    "lod_policy": "inspection-single-lod"
  },
  "nodes": [],
  "edges": [],
  "quality": { "score": 92, "production_ready": true, "issues": [] },
  "derived_hashes": { "pipeline_cache_key": "0x0000000000000013" },
  "diagnostics": []
})json");
  const aster::ProceduralAssetGraphPackage package =
      aster::loadProceduralAssetGraphPackage(path);
  assert(package.material.surface_profile == aster::MaterialSurfaceProfile::BiologicalIntegument);
  assert(aster::resolveMaterialSurfaceProfile(aster::proceduralAssetGraphMaterial(package)) ==
         aster::MaterialSurfaceProfile::BiologicalIntegument);
}

void testMaterialAssetMetadataRoundTrip() {
  const aster::MaterialAssetLoadResult loaded = aster::parseMaterialAsset(R"mat(
material RoundTripRock {
  schema_version: 1
  name: "Round Trip Rock"
  shading_model: LitPBR
  surface_profile: stratified-rock
  blend_mode: Opaque
  cull_mode: Back
  receives_decals: true
  receives_shadows: true

  provenance {
    generator: "unit-test"
    source_surface: "wet-cave-wall"
  }

  authoring {
    texel_density: 2.7
    mapping_policy: triplanar
  }

  preview {
    environment: "cave-dark"
    rig: "material-lab"
  }

  quality_profile {
    mobile_drop_parallax: true
    mobile_max_texture_size: 1024
  }

  textures {
    albedo: "albedo.ktx2"
    normal: "normal.ktx2"
    orm: "orm.ktx2"
  }

  params {
    roughness: 0.72
    metallic: 0.0
  }

  features {
    triplanar: true
    normal_map: true
  }

  layers {
    base: triplanar(albedo, normal, orm)
  }
}
)mat",
                                                              "roundtrip.astermat");
  assert(loaded.ok());
  assert(loaded.value.provenance.at("generator") == "unit-test");
  assert(loaded.value.authoring.at("texel_density") == "2.7");
  assert(loaded.value.preview.at("environment") == "cave-dark");
  assert(loaded.value.quality_profile.at("mobile_max_texture_size") == "1024");

  const std::string serialized = aster::serializeMaterialAsset(loaded.value);
  assert(serialized.find("quality_profile") != std::string::npos);
  const aster::MaterialAssetLoadResult reparsed =
      aster::parseMaterialAsset(serialized, "roundtrip_saved.astermat");
  assert(reparsed.ok());
  assert(reparsed.value.provenance == loaded.value.provenance);
  assert(reparsed.value.authoring == loaded.value.authoring);
  assert(reparsed.value.preview == loaded.value.preview);
  assert(reparsed.value.quality_profile == loaded.value.quality_profile);
  assert(reparsed.value.layers.size() == loaded.value.layers.size());
}

void testMaterialAuthoringGraphAndLabAudit() {
  aster::MaterialAssetLoadResult loaded =
      aster::parseMaterialAsset(sampleMaterialSource(), "lab_audit.astermat");
  assert(loaded.ok());
  loaded.value.quality_profile["mobile_drop_parallax"] = "true";
  const aster::MaterialAuthoringGraph graph =
      aster::materialAuthoringGraphForAsset(loaded.value);
  assert(graph.source_kind == "astermat");
  assert(graph.nodes.size() >= loaded.value.layers.size());
  assert(!graph.edges.empty());

  const aster::TextureSetValidation validation =
      aster::validateMaterialTextureSet(loaded.value, {}, {.require_existing_files = false});
  const aster::MaterialLabAudit audit = aster::buildMaterialLabAudit(loaded.value, validation);
  assert(audit.shader_variant_key != 0u);
  assert(audit.feature_mask != 0u);
  assert(!audit.surface_fidelity.empty());
  assert(std::any_of(audit.surface_fidelity.begin(), audit.surface_fidelity.end(),
                     [](const std::string &note) {
                       return note.find("temporal stability") != std::string::npos;
                     }));
  assert(!audit.mobile_degradations.empty());
  assert(std::any_of(audit.provenance_notes.begin(), audit.provenance_notes.end(),
                     [](const std::string &note) {
                       return note.find("histogram unavailable") != std::string::npos;
                     }));

  aster::MaterialAsset changed = loaded.value;
  changed.explicit_features["parallax"] = false;
  const aster::MaterialLabAudit changed_audit =
      aster::buildMaterialLabAudit(changed, validation);
  assert(changed_audit.shader_variant_key != audit.shader_variant_key);

  const aster::MaterialAssetLoadResult unsupported = aster::parseMaterialAsset(R"mat(
material UnsupportedLayer {
  shading_model: LitPBR
  layers {
    mystery: not_a_node(albedo)
  }
}
)mat",
                                                                              "unsupported.astermat");
  const aster::MaterialAuthoringGraph unsupported_graph =
      aster::materialAuthoringGraphForAsset(unsupported.value);
  assert(std::any_of(unsupported_graph.nodes.begin(), unsupported_graph.nodes.end(),
                     [](const aster::MaterialAuthoringNode &node) {
                       return node.capability_status == "unsupported";
                     }));
}

void testMaterialLabPreviewRendersDebugViews() {
  const aster::MaterialAssetLoadResult loaded =
      aster::parseMaterialAsset(sampleMaterialSource(), "preview_lab.astermat");
  assert(loaded.ok());
  const aster::MaterialLabPreviewImage beauty =
      aster::renderMaterialLabPreview(loaded.value,
                                      {.mode = aster::MaterialLabPreviewMode::Beauty,
                                       .mesh = aster::MaterialLabMeshTarget::Rock,
                                       .environment = aster::MaterialLabEnvironmentRig::CaveDark,
                                       .width = 64,
                                       .height = 48});
  const aster::MaterialLabPreviewImage normal =
      aster::renderMaterialLabPreview(loaded.value,
                                      {.mode = aster::MaterialLabPreviewMode::Normal,
                                       .mesh = aster::MaterialLabMeshTarget::Rock,
                                       .environment = aster::MaterialLabEnvironmentRig::CaveDark,
                                       .width = 64,
                                       .height = 48});
  assert(beauty.available);
  assert(normal.available);
  assert(beauty.rgba8.size() == 64u * 48u * 4u);
  assert(normal.rgba8.size() == beauty.rgba8.size());
  assert(beauty.rgba8 != normal.rgba8);
}

void testShaderLibraryAndReflection() {
  aster::ShaderLibrary library;
  assert(library.addModule("math", "float aster_test(float v) { return v; }\n"));
  assert(library.addModule("material", "float4 fs_main() { return float4(1.0); }\n"));
  const aster::MaterialAssetLoadResult loaded =
      aster::parseMaterialAsset(sampleMaterialSource(), "memory.astermat");
  assert(loaded.ok());
  const aster::ShaderVariantKey variant = aster::shaderVariantKeyForMaterial(loaded.value);
  const aster::ShaderCompileResult compiled =
      aster::compileShaderVariant(library, {.backend = aster::ShaderBackend::MetalMSL,
                                            .variant = variant,
                                            .modules = {"math", "material"},
                                            .entry_point = "fs_main"});
  assert(compiled.success);
  assert(compiled.source.find("#include <metal_stdlib>") != std::string::npos);
  assert(compiled.source.find("backend: metal-msl") != std::string::npos);
  assert(compiled.reflection.resources.size() >= 5u);

  const aster::ShaderLibraryLoadResult real_library =
      aster::loadShaderLibrary(std::filesystem::path(ASTER_SOURCE_DIR) / "shaders" / "lib");
  assert(real_library.ok());
  const aster::ShaderCompileResult pbr =
      aster::compileShaderVariant(real_library.library,
                                  {.backend = aster::ShaderBackend::D3D12HLSL,
                                   .variant = variant,
                                   .modules = {"brdf", "pbr", "tonemap", "material_lit_pbr"},
                                   .entry_point = "fs_main"});
  assert(pbr.success);
  assert(pbr.source.find("generated HLSL") != std::string::npos);
  assert(pbr.source.find("return float4(1.0, 0.0, 1.0, 1.0)") == std::string::npos);
  assert(pbr.source.find("aster_material_lit_pbr") != std::string::npos);

  const aster::ShaderCompileResult material_utilities =
      aster::compileShaderVariant(real_library.library,
                                  {.backend = aster::ShaderBackend::SoftwareReference,
                                   .variant = variant,
                                   .modules = {"brdf",
                                               "debug_views",
                                               "clearcoat",
                                               "wetness",
                                               "parallax",
                                               "detail_normal",
                                               "alpha",
                                               "material_debug",
                                               "material_lit_pbr"},
                                   .entry_point = "fs_main"});
  assert(material_utilities.success);
  assert(material_utilities.source.find("aster_clearcoat_specular") != std::string::npos);
  assert(material_utilities.source.find("aster_apply_wetness_roughness") != std::string::npos);
  assert(material_utilities.source.find("aster_parallax_offset") != std::string::npos);
  assert(material_utilities.source.find("aster_debug_roughness") != std::string::npos);

  const aster::ShaderCompileResult fog =
      aster::compileShaderVariant(real_library.library,
                                  {.backend = aster::ShaderBackend::D3D12HLSL,
                                   .variant = variant,
                                   .modules = {"fog"},
                                   .entry_point = "aster_fog_factor"});
  assert(fog.success);
  assert(fog.source.find("aster_fog_factor") != std::string::npos);
  assert(fog.source.find("falloff") != std::string::npos);
}

void testTextureValidationAndDebugContracts() {
  const aster::MaterialAssetLoadResult loaded =
      aster::parseMaterialAsset(sampleMaterialSource(), "memory.astermat");
  assert(loaded.ok());
  const aster::TextureSetValidation validation =
      aster::validateMaterialTextureSet(loaded.value, {}, {.require_existing_files = false});
  assert(validation.ok);
  assert(validation.textures.size() == loaded.value.textures.size());
  bool saw_srgb_albedo = false;
  bool saw_linear_normal = false;
  for (const aster::TextureAssetMetadata &texture : validation.textures) {
    saw_srgb_albedo = saw_srgb_albedo ||
                      (texture.kind == aster::TextureKind::Albedo &&
                       texture.color_space == aster::TextureColorSpace::SRGB);
    saw_linear_normal = saw_linear_normal ||
                        (texture.kind == aster::TextureKind::Normal &&
                         texture.color_space == aster::TextureColorSpace::Linear);
    assert(aster::textureDebugSummary(texture).find(aster::textureKindName(texture.kind)) !=
           std::string::npos);
  }
  assert(saw_srgb_albedo);
  assert(saw_linear_normal);
  assert(aster::textureKindForRole("wetness") == aster::TextureKind::Wetness);
  assert(aster::textureKindForRole("opacity") == aster::TextureKind::Opacity);

  std::vector<aster::TextureAssetMetadata> atlas_inputs = validation.textures;
  for (aster::TextureAssetMetadata &texture : atlas_inputs) {
    texture.width = 16u;
    texture.height = 8u;
  }
  const aster::TextureAtlasPlan atlas = aster::packTextureAtlasRows(atlas_inputs, 32u);
  assert(atlas.width == 32u);
  assert(atlas.height >= 8u);
  assert(atlas.entries.size() == atlas_inputs.size());

  const std::filesystem::path dir = tempDir();
  writeKtx2Header(dir / "rock_albedo.ktx2", 16u, 8u, 4u);
  writeKtx2Header(dir / "rock_normal.ktx2", 16u, 8u, 4u);
  writeKtx2Header(dir / "rock_orm.ktx2", 16u, 8u, 4u);
  writeKtx2Header(dir / "rock_height.ktx2", 16u, 8u, 4u);
  writeKtx2Header(dir / "wetness_mask.ktx2", 16u, 8u, 4u);
  aster::MaterialAsset authored_asset = loaded.value;
  authored_asset.source_path = dir / "wet_rock.astermat";
  const aster::RuntimeTextureSet direct_set =
      aster::runtimeTextureSetForMaterial(authored_asset, {}, {.require_existing_files = false});
  aster::MaterialResourceLibrary library;
  const bool added = library.addMaterialAsset(authored_asset, {}, {.require_existing_files = false});
  assert(added);
  (void)added;
  const aster::MaterialRuntimeResource *resource = library.find("TestWetRock");
  assert(resource != nullptr);
  assert(direct_set.textures.size() == aster::materialRuntimeTextureRoles().size());
  assert(resource->asset_id == "TestWetRock");
  assert(resource->fallback_material.asset_id == "TestWetRock");
  assert(resource->texture_set.textures.size() == aster::materialRuntimeTextureRoles().size());
  const aster::RuntimeTexture *albedo = resource->texture_set.find("albedo");
  const aster::RuntimeTexture *normal = resource->texture_set.find("normal");
  const aster::RuntimeTexture *orm = resource->texture_set.find("orm");
  const aster::RuntimeTexture *emissive = resource->texture_set.find("emissive");
  const aster::RuntimeTexture *wetness = resource->texture_set.find("wetness");
  const aster::RuntimeTexture *opacity = resource->texture_set.find("opacity");
  assert(albedo != nullptr && albedo->valid && !albedo->fallback);
  assert(normal != nullptr && normal->normal_convention == aster::TextureNormalConvention::OpenGlYUp);
  assert(orm != nullptr && orm->kind == aster::TextureKind::ORM);
  assert(emissive != nullptr && emissive->fallback);
  assert(wetness != nullptr && wetness->kind == aster::TextureKind::Wetness);
  assert(opacity != nullptr && opacity->kind == aster::TextureKind::Opacity && opacity->fallback);
  assert(albedo->sampler.max_anisotropy >= 1.0f);
  const aster::Vec3 albedo_sample = aster::sampleRuntimeTextureRgb(*albedo, {0.3f, 0.7f});
  const aster::Vec4 normal_sample = aster::sampleRuntimeTexture(*normal, {0.3f, 0.7f});
  const aster::Vec4 orm_sample = aster::sampleRuntimeTexture(*orm, {0.3f, 0.7f});
  assert(albedo_sample.x > 0.0f || albedo_sample.y > 0.0f || albedo_sample.z > 0.0f);
  assert(normal_sample.z > 0.9f);
  assert(orm_sample.x >= 0.0f && orm_sample.y >= 0.0f && orm_sample.z >= 0.0f);
}

void testRenderQualityProfileContracts() {
  const aster::RenderQualityProfile production =
      aster::makeRenderQualityProfile(aster::RenderQualityTier::Production);
  assert(aster::renderQualityTierName(production.tier) == "production");
  assert(production.shadows.technique == aster::ShadowTechnique::CascadedDirectional);
  assert(production.reflections.mode == aster::ReflectionProbeMode::StaticLocal);
  assert(production.textures.require_mip_chain);
  assert(production.surface_fidelity.require_area_light_response);
  assert(production.surface_fidelity.minimum_area_light_radius > 0.0f);

  aster::RendererSettings settings;
  aster::applyRenderQualityProfile(settings, production);
  assert(settings.sun_light.enabled);
  assert(settings.grounding.contact_shadows);
  assert(settings.atmosphere.enabled);
  assert(settings.pipeline.tone_mapper == production.post.tone_mapper);
  assert(settings.post.hdr_scene_color);
  assert(settings.post.fxaa);
  assert(settings.shadows.cascaded_directional);
  assert(settings.shadows.directional_cascades == production.shadows.directional_cascades);
  assert(settings.reflections.enabled);
  assert(settings.reflections.static_local_probes);
  assert(std::all_of(settings.light_rig.begin(), settings.light_rig.end(),
                     [&production](const aster::Light &light) {
                       return light.source_radius >=
                              production.surface_fidelity.minimum_area_light_radius;
                     }));

  const aster::TextureImportOptions options =
      aster::textureImportOptionsForQuality(production, false);
  assert(!options.require_existing_files);
  assert(options.generate_mips);
  assert(options.compression == aster::TextureCompression::Ktx2Basis);

  const aster::MaterialAssetLoadResult loaded =
      aster::parseMaterialAsset(sampleMaterialSource(), "quality.astermat");
  assert(loaded.ok());
  aster::TextureSetValidation validation;
  for (const auto &[role, slot] : loaded.value.textures) {
    (void)slot;
    aster::TextureAssetMetadata metadata;
    metadata.source_path = role + ".ktx2";
    metadata.kind = aster::textureKindForRole(role);
    metadata.color_space = aster::defaultTextureColorSpace(metadata.kind);
    metadata.compression = aster::TextureCompression::Ktx2Basis;
    metadata.width = 1024u;
    metadata.height = 1024u;
    metadata.mips = {{1024u, 1024u, 0u, 0u},
                     {512u, 512u, 0u, 0u},
                     {256u, 256u, 0u, 0u}};
    metadata.valid = true;
    validation.textures.push_back(std::move(metadata));
  }
  const aster::MaterialQualityReport report =
      aster::evaluateMaterialQuality(loaded.value, validation, production);
  assert(report.production_ready);
  assert(report.score >= 70u);
  assert(std::any_of(report.issues.begin(), report.issues.end(),
                     [](const aster::RenderQualityIssue &issue) {
                       return issue.category == "surface-fidelity";
                     }));
  aster::MaterialAsset signal_asset = loaded.value;
  signal_asset.authoring["energy_conservation"] = "LitPBR";
  signal_asset.authoring["tangent_basis"] = "mikktspace";
  signal_asset.authoring["temporal_stability"] = "mip-biased-parallax";
  signal_asset.preview["environment"] = "material_lab";
  signal_asset.preview["reflection_probe"] = "static-local";
  const aster::AsterMaterialSignalSummary signal_summary =
      aster::summarizeAsterMaterialSignals(signal_asset, validation, production);
  assert(signal_summary.image_proof_ready);
  assert(signal_summary.ready_signals == signal_summary.rows.size());
  assert(signal_summary.score >= 70u);
  assert(std::any_of(signal_summary.rows.begin(), signal_summary.rows.end(),
                     [](const aster::AsterMaterialSignalRow &row) {
                       return row.kind == aster::AsterMaterialSignalKind::RustWetness &&
                              row.ready && row.strength > 0.5f;
                     }));
  assert(std::any_of(signal_summary.rows.begin(), signal_summary.rows.end(),
                     [](const aster::AsterMaterialSignalRow &row) {
                       return row.kind ==
                                  aster::AsterMaterialSignalKind::NormalHeightCoupling &&
                              row.ready && row.strength > 0.5f;
                     }));

  const aster::MaterialAssetLoadResult broken =
      aster::parseMaterialAsset(R"mat(
material BrokenPbr {
  shading_model: LitPBR
  params {
    roughness: 1.0
  }
}
)mat",
                                "broken_quality.astermat");
  assert(broken.ok());
  const aster::MaterialQualityReport broken_report =
      aster::evaluateMaterialQuality(broken.value, {}, production);
  assert(!broken_report.production_ready);
  assert(!broken_report.issues.empty());
}

void testRenderStyleProfileContracts() {
  const aster::RenderStyleProfile neutral =
      aster::makeRenderStyleProfile(aster::RenderStylePreset::Neutral);
  assert(neutral.preset == aster::RenderStylePreset::Neutral);
  assert(neutral.unlit_mix == 0.0f);
  assert(neutral.emissive_gain == 1.0f);
  assert(neutral.color_quantization_steps == 0.0f);
  assert(aster::renderStylePresetName(neutral.preset) == "neutral");
  assert(aster::parseRenderStylePreset("retro_horror").value() ==
         aster::RenderStylePreset::RetroHorrorReadable);

  const aster::RenderStyleProfile retro =
      aster::makeRenderStyleProfile(aster::RenderStylePreset::RetroHorrorReadable);
  assert(retro.unlit_mix > 0.0f);
  assert(retro.unlit_mix < 0.25f);
  assert(retro.emissive_gain > 1.0f);
  assert(retro.luma_crush > 0.0f);
  assert(retro.color_quantization_steps >= 16.0f);
  assert(retro.procedural_sample_snap == 0.0f);
  assert(aster::renderStylePresetName(retro.preset) == "retro-horror");

  aster::RendererSettings settings;
  settings.atmosphere.fog_color = {0.20f, 0.24f, 0.32f};
  settings.atmosphere.fog_start = 6.0f;
  settings.atmosphere.fog_end = 30.0f;
  settings.sun_light.intensity = 2.0f;
  aster::applyRenderStyleProfile(settings, retro);
  assert(settings.style.preset == aster::RenderStylePreset::RetroHorrorReadable);
  assert(settings.atmosphere.enabled);
  assert(settings.atmosphere.fog_color.x > settings.atmosphere.fog_color.y);
  assert(settings.atmosphere.fog_falloff == aster::AtmosphereFogFalloff::Exponential);
  assert(settings.atmosphere.fog_power > 1.0f);
  assert(settings.sun_light.intensity <= 0.85f);
  assert(settings.atmosphere.saturation >= 1.0f);

  aster::applyRenderStyleProfile(settings, neutral);
  assert(settings.style.preset == aster::RenderStylePreset::Neutral);
  assert(settings.atmosphere.fog_falloff == aster::AtmosphereFogFalloff::SmoothLinear);
  assert(settings.atmosphere.fog_power == 1.0f);
}

void testNativeRenderStyleShaderContracts() {
  const std::filesystem::path source_root = std::filesystem::path(ASTER_SOURCE_DIR);
  const std::string metal = readText(source_root / "src" / "render" / "render_device_metal.mm");
  const std::string d3d12 = readText(source_root / "src" / "render" / "render_device_d3d12.cpp");
  assert(metal.find("style_params") != std::string::npos);
  assert(metal.find("style_sample_world") != std::string::npos);
  assert(metal.find("style_fog") != std::string::npos);
  assert(metal.find("biological_integument") != std::string::npos);
  assert(metal.find("is_pattern(pattern, 19.0)") != std::string::npos);
  assert(d3d12.find("scene_style_params") != std::string::npos);
  assert(d3d12.find("style_sample_world") != std::string::npos);
  assert(d3d12.find("style_fog") != std::string::npos);
  assert(d3d12.find("biological_integument") != std::string::npos);
  assert(d3d12.find("is_pattern(pattern, 19.0)") != std::string::npos);
}

void testHotReloadSnapshot() {
  const std::filesystem::path dir = tempDir();
  const std::filesystem::path path = dir / "shader.astsl";
  writeText(path, "float4 fs_main() { return float4(1.0); }\n");
  const aster::ShaderHotReloadState before = aster::snapshotShaderHotReloadInputs({path});
  assert(before.diagnostics.empty());
  std::this_thread::sleep_for(std::chrono::milliseconds(20));
  writeText(path, "float4 fs_main() { return float4(0.0, 0.0, 0.0, 1.0); }\n");
  const aster::ShaderHotReloadState after = aster::snapshotShaderHotReloadInputs({path});
  assert(aster::shaderHotReloadInputsChanged(before, after));
}

void testInvalidMaterialDiagnostics() {
  const aster::MaterialAssetLoadResult loaded = aster::parseMaterialAsset(R"mat(
material Broken {
  shading_model: LitPBR
  layers {
    bad: unknown_op(albedo)
  }
}
)mat",
                                                                         "broken.astermat");
  assert(!loaded.ok());
}

void testProceduralAssetGraphPackageRuntimeMaterial() {
  const std::filesystem::path dir = tempDir();
  const std::filesystem::path path = dir / "wet_rock.assetgraphbin";
  writeText(path, R"json({
  "schema_version": 1,
  "asset_guid": "graph-guid-wet-rock",
  "id": "asset_graph.wet_rock",
  "name": "Graph Wet Rock",
  "kind": "asset_graph",
  "source_path": "wet_rock.astergraph",
  "runtime_model": "runtime-procedural",
  "material": {
    "id": "material.graph_wet_rock",
    "surface_profile": "stratified-rock",
    "feature_mask": 127,
    "shader_variant_key": 123456,
    "shader_variant_tag": "AssetGraph.material.graph_wet_rock.runtime-procedural",
    "pipeline_tag": "material:material.graph_wet_rock:stratified-rock:runtime-procedural",
    "fallback": {
      "base_color": [0.19, 0.17, 0.145],
      "emission_color": [0.0, 0.0, 0.0],
      "roughness": 0.78,
      "metallic": 0.0,
      "emission_strength": 0.0,
      "opacity": 1.0,
      "double_sided": false,
      "alpha_mode": "Opaque",
      "receives_shadows": true,
      "surface_profile": "stratified-rock"
    },
    "params": {
      "wetness": 0.52,
      "macro_variation": 0.42,
      "micro_normal_strength": 0.50,
      "roughness_variation": 0.16,
      "height_shading": 0.28
    },
    "features": {
      "triplanar": true,
      "normal_map": true,
      "parallax": true
    }
  },
  "mesh": {
    "primitive": "rock",
    "uv_policy": "triplanar",
    "tangent_policy": "validate-or-generate",
    "collision_proxy": "convex-hull",
    "lod_policy": "single-lod"
  },
  "nodes": [
    {
      "id": "mat.noise",
      "kind": "noise",
      "role": "base_color",
      "label": "base noise",
      "params": { "scale": "3.2" },
      "capability_status": "runtime-procedural-reference"
    }
  ],
  "edges": [],
  "preview": { "environment": "cave-dark" },
  "quality": { "score": 92, "production_ready": true, "issues": [] },
  "derived_hashes": {
    "pipeline_cache_key": "0x0000000000000001"
  },
  "diagnostics": []
})json");

  const aster::ProceduralAssetGraphPackage package =
      aster::loadProceduralAssetGraphPackage(path);
  assert(package.asset_guid == "graph-guid-wet-rock");
  assert(package.nodes.size() == 1u);
  assert(package.quality.score == 92u);
  assert(package.pipeline_key == 1u);
  assert(package.material.procedural_graph_guid == "graph-guid-wet-rock");
  assert(package.material.procedural_pipeline_key == 1u);

  const aster::Material material = aster::proceduralAssetGraphMaterial(package);
  assert(material.asset_id == "material.graph_wet_rock");
  assert(material.shader_variant_key == 123456u);
  assert(material.procedural_graph_guid == "graph-guid-wet-rock");
  assert(material.procedural_graph_node == "mat.noise");
  assert(material.procedural_pipeline_key == 1u);
  assert(material.procedural.wetness > 0.51f);
  assert(material.procedural.micro_normal_strength > 0.49f);
  assert(aster::resolveMaterialSurfaceProfile(material) ==
         aster::MaterialSurfaceProfile::StratifiedRock);
}

struct TestCase {
  const char *name = "";
  void (*run)() = nullptr;
};

constexpr TestCase kTestCases[] = {
    {"material_asset_parser_and_compiler", testMaterialAssetParserAndCompiler},
    {"biological_integument_material_aliases", testBiologicalIntegumentMaterialAliases},
    {"material_asset_metadata_round_trip", testMaterialAssetMetadataRoundTrip},
    {"material_authoring_graph_and_lab_audit", testMaterialAuthoringGraphAndLabAudit},
    {"material_lab_preview_debug_views", testMaterialLabPreviewRendersDebugViews},
    {"shader_library_and_reflection", testShaderLibraryAndReflection},
    {"texture_validation_and_debug", testTextureValidationAndDebugContracts},
    {"render_quality_profile", testRenderQualityProfileContracts},
    {"render_style_profile", testRenderStyleProfileContracts},
    {"native_render_style_shaders", testNativeRenderStyleShaderContracts},
    {"hot_reload_snapshot", testHotReloadSnapshot},
    {"invalid_material_diagnostics", testInvalidMaterialDiagnostics},
    {"procedural_asset_graph_package", testProceduralAssetGraphPackageRuntimeMaterial},
};

int runTestCase(const TestCase &test_case) {
  std::cout << "material_shader_system_tests: " << test_case.name << '\n';
  test_case.run();
  std::cout << "material_shader_system_tests: " << test_case.name << " passed.\n";
  return 0;
}

} // namespace

int main(const int argc, const char **argv) {
  if (argc > 1) {
    for (const TestCase &test_case : kTestCases) {
      if (std::strcmp(argv[1], test_case.name) == 0) {
        return runTestCase(test_case);
      }
    }
    std::cerr << "Unknown material_shader_system_tests case: " << argv[1] << '\n';
    return 1;
  }
  for (const TestCase &test_case : kTestCases) {
    runTestCase(test_case);
  }
  std::cout << "material_shader_system_tests passed.\n";
  return 0;
}
