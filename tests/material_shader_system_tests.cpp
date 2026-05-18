// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/material/material_compiler.hpp"
#include "aster/material/material_graph.hpp"
#include "aster/shader/shader_compiler.hpp"
#include "aster/shader/shader_hot_reload.hpp"
#include "aster/render/render_quality.hpp"
#include "aster/texture/texture_atlas.hpp"
#include "aster/texture/texture_debug.hpp"
#include "aster/texture/texture_importer.hpp"

#include <cassert>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
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
  assert(compiled.binding_layout.bindings.size() == loaded.value.textures.size() + 2u);
  assert(compiled.fallback_material.material.asset_id == loaded.value.id);
  assert(compiled.fallback_material.material.double_sided);
	  assert(compiled.fallback_material.material.surface_profile ==
	         aster::MaterialSurfaceProfile::StratifiedRock);
	  assert(aster::resolveMaterialSurfaceProfile(compiled.fallback_material.material) ==
	         aster::MaterialSurfaceProfile::StratifiedRock);
  assert((compiled.fallback_material.permutation_flags &
          aster::materialPermutationFlagBit(aster::MaterialPermutationFlag::ShaderVariant)) != 0u);

  const aster::MaterialGraph graph = aster::materialGraphForAsset(loaded.value);
  assert(graph.nodes.size() == 2u);
  assert(graph.nodes.front().operation == "triplanar");
  assert(graph.nodes.front().op == aster::MaterialGraphOperation::TriplanarSample);
  assert(graph.nodes.front().value_type == aster::MaterialGraphValueType::MaterialLayer);
  assert(aster::materialGraphOperationName(graph.nodes.back().op) == "height-blend");
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

  std::vector<aster::TextureAssetMetadata> atlas_inputs = validation.textures;
  for (aster::TextureAssetMetadata &texture : atlas_inputs) {
    texture.width = 16u;
    texture.height = 8u;
  }
  const aster::TextureAtlasPlan atlas = aster::packTextureAtlasRows(atlas_inputs, 32u);
  assert(atlas.width == 32u);
  assert(atlas.height >= 8u);
  assert(atlas.entries.size() == atlas_inputs.size());
}

void testRenderQualityProfileContracts() {
  const aster::RenderQualityProfile production =
      aster::makeRenderQualityProfile(aster::RenderQualityTier::Production);
  assert(aster::renderQualityTierName(production.tier) == "production");
  assert(production.shadows.technique == aster::ShadowTechnique::CascadedDirectional);
  assert(production.reflections.mode == aster::ReflectionProbeMode::StaticLocal);
  assert(production.textures.require_mip_chain);

  aster::RendererSettings settings;
  aster::applyRenderQualityProfile(settings, production);
  assert(settings.sun_light.enabled);
  assert(settings.grounding.contact_shadows);
  assert(settings.atmosphere.enabled);
  assert(settings.pipeline.tone_mapper == production.post.tone_mapper);

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
  assert(report.score >= 80u);

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

struct TestCase {
  const char *name = "";
  void (*run)() = nullptr;
};

constexpr TestCase kTestCases[] = {
    {"material_asset_parser_and_compiler", testMaterialAssetParserAndCompiler},
    {"shader_library_and_reflection", testShaderLibraryAndReflection},
    {"texture_validation_and_debug", testTextureValidationAndDebugContracts},
    {"render_quality_profile", testRenderQualityProfileContracts},
    {"hot_reload_snapshot", testHotReloadSnapshot},
    {"invalid_material_diagnostics", testInvalidMaterialDiagnostics},
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
