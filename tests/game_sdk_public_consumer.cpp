// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/game_sdk/game_sdk.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace {

std::filesystem::path sourceRoot() {
  return std::filesystem::path(ASTER_SOURCE_DIR);
}

const aster::sdk::ProjectAssetRef *findAsset(const aster::sdk::ProjectDocument &project,
                                             const std::string &id) {
  for (const aster::sdk::ProjectAssetRef &asset : project.assets) {
    if (asset.id == id) {
      return &asset;
    }
  }
  return nullptr;
}

void testLumenProjectAuthoringDocumentsLoad() {
  const std::filesystem::path project_root = sourceRoot() / "projects" / "lumen_run";
  const auto project = aster::sdk::loadProjectDocument(project_root / "lumen_run.asterproj");
  assert(project.ok());
  assert(project.value.name == "Lumen Run");
  assert(project.value.startup_scene == "scene.cave_entry");
  assert(findAsset(project.value, "scene.cave_entry") != nullptr);
  assert(findAsset(project.value, "prefab.supply_chest") != nullptr);
  assert(findAsset(project.value, "action.chest.open") != nullptr);

  const auto scene = aster::sdk::loadSceneDocument(project_root / "scenes" / "cave_entry.scene");
  assert(scene.ok());
  assert(scene.value.id == "scene.cave_entry");
  assert(scene.value.entities.size() == 4u);

  const auto cave = aster::sdk::loadCaveDocument(project_root / "caves" / "cave_entry.cave");
  assert(cave.ok());
  assert(cave.value.id == "cave.lumen_entry");
  assert(cave.value.sections.size() == 2u);
  assert(!cave.value.seeds.empty());
  assert(!cave.value.validation.walkable_routes.empty());
  const std::vector<aster::sdk::Diagnostic> cave_diagnostics =
      aster::sdk::validateCaveDocument(cave.value, &project.value, &scene.value,
                                       project_root / "caves" / "cave_entry.cave");
  assert(cave_diagnostics.empty());

  aster::sdk::World world;
  const aster::sdk::InstantiateResult scene_instance = world.instantiate(scene.value);
  assert(scene_instance.ok());
  assert(scene_instance.created_entities == scene.value.entities.size());
  const aster::sdk::EntityInstance *chest = world.findEntity("supply_chest");
  assert(chest != nullptr);
  assert(chest->definition.components.interactable.has_value());
  assert(chest->definition.components.interactable->action_graph == "action.chest.open");
  assert(chest->definition.components.inventory.has_value());

  const auto prefab =
      aster::sdk::loadPrefabDocument(project_root / "prefabs" / "torch_pickup.prefab");
  assert(prefab.ok());
  aster::sdk::World prefab_world;
  const aster::sdk::InstantiateResult prefab_instance = prefab_world.instantiate(prefab.value);
  assert(prefab_instance.ok());
  assert(prefab_world.findEntity("torch_pickup.root") != nullptr);

  const auto material =
      aster::sdk::loadMaterialDocument(project_root / "materials" / "torch_flame.material");
  assert(material.ok());
  assert(material.value.emission_color.x > 0.0f);
  assert(material.value.emission_strength > 0.0f);
  assert(material.value.alpha_mode == "blend");
  assert(material.value.depth_write == "disabled");
  assert(material.value.double_sided);
  assert(material.value.opacity < 1.0f);
  assert(material.value.compiler_hints.at("permutation") == "transparent-emissive");

  const auto item = aster::sdk::loadItemDocument(project_root / "items" / "torch.item");
  assert(item.ok());
  assert(item.value.stackable);
  assert(item.value.use_action_graph == "action.item.use_torch");

  const auto graph =
      aster::sdk::loadActionGraphDocument(project_root / "actions" / "chest_open.action_graph");
  assert(graph.ok());
  aster::sdk::ActionGraphRuntime runtime;
  const aster::sdk::ActionExecution execution =
      runtime.execute(graph.value, {.actor = "player", .target = "supply_chest", .input = "use"});
  assert(execution.ok());
  assert(execution.events.size() == 2u);
  assert(execution.events.front().type == "emit_event");
  assert(execution.events.front().target == "supply_chest");
  assert(execution.events.front().parameters.at("event") == "container.open_requested");
}

void testSchemaDiagnosticsRejectInvalidDocuments() {
  const auto bad_project = aster::sdk::parseProjectDocument(R"json({
    "schema_version": 2,
    "name": "Bad",
    "startup_scene": "missing",
    "assets": []
  })json");
  assert(!bad_project.ok());

  const auto bad_scene = aster::sdk::parseSceneDocument(R"json({
    "schema_version": 1,
    "id": "scene.bad",
    "entities": [
      {
        "id": "entity.bad",
        "components": {
          "string_prefix_router": {}
        }
      }
    ]
  })json");
  assert(!bad_scene.ok());

  const auto bad_item = aster::sdk::parseItemDocument(R"json({
    "schema_version": 1,
    "id": "item.bad",
    "display_name": "Bad Item",
    "stackable": false,
    "max_stack": 4
  })json");
  assert(!bad_item.ok());

  const auto bad_cave = aster::sdk::parseCaveDocument(R"json({
    "schema_version": 1,
    "id": "cave.bad",
    "name": "Broken Cave",
    "required_assets": ["prefab.missing"],
    "sections": [
      {
        "id": "entry",
        "archetype": "prefab.missing",
        "tunnel": { "length_segments": 4, "radial_segments": 4, "collision_start_t": 0.8, "collision_end_t": 0.2 }
      }
    ],
    "placements": [
      { "id": "ore_without_prefab", "kind": "ore_node", "section": "missing" }
    ],
    "validation": {
      "walkable_routes": [{ "id": "dead_end", "points": [[0, 0, 0]] }],
      "spawn_volumes": [{ "id": "blocked", "center": [0, 0, 0], "half_extents": [0, 0.5, 0.5] }]
    }
  })json");
  assert(bad_cave.ok());
  const auto sparse_project = aster::sdk::parseProjectDocument(R"json({
    "schema_version": 1,
    "name": "Sparse",
    "startup_scene": "scene.only",
    "assets": [
      { "id": "scene.only", "kind": "scene", "path": "scenes/only.scene" }
    ]
  })json");
  assert(sparse_project.ok());
  const std::vector<aster::sdk::Diagnostic> bad_cave_diagnostics =
      aster::sdk::validateCaveDocument(bad_cave.value, &sparse_project.value, nullptr);
  assert(!bad_cave_diagnostics.empty());
}

void testWorldRejectsDuplicateEntityInstances() {
  const auto scene = aster::sdk::parseSceneDocument(R"json({
    "schema_version": 1,
    "id": "scene.dup",
    "entities": [
      { "id": "same", "components": { "transform": {} } }
    ]
  })json");
  assert(scene.ok());

  aster::sdk::World world;
  assert(world.instantiate(scene.value).ok());
  const aster::sdk::InstantiateResult duplicate = world.instantiate(scene.value);
  assert(!duplicate.ok());
  assert(world.entities().size() == 1u);
}

} // namespace

int main() {
  testLumenProjectAuthoringDocumentsLoad();
  testSchemaDiagnosticsRejectInvalidDocuments();
  testWorldRejectsDuplicateEntityInstances();
  std::cout << "game_sdk_public_consumer passed.\n";
  return 0;
}
