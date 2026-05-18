// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/material/material_compiler.hpp"
#include "aster/material/material_graph.hpp"
#include "aster/shader/shader_compiler.hpp"
#include "aster/shader/shader_hot_reload.hpp"
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
  assert(compiled.fallback_material.material.surface_pattern == aster::SurfacePattern::CaveRock);
  assert((compiled.fallback_material.permutation_flags &
          aster::materialPermutationFlagBit(aster::MaterialPermutationFlag::ShaderVariant)) != 0u);

  const aster::MaterialGraph graph = aster::materialGraphForAsset(loaded.value);
  assert(graph.nodes.size() == 2u);
  assert(graph.nodes.front().operation == "triplanar");
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
  assert(compiled.source.find("backend: metal-msl") != std::string::npos);
  assert(compiled.reflection.resources.size() >= 5u);
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
