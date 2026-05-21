// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#include "aster/game_sdk/game_sdk.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

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
  assert(cave.value.validation.probe_agent.has_value());
  assert(!cave.value.validation.resource_probes.empty());
  assert(!cave.value.validation.encounter_probes.empty());
  assert(cave.value.validation.perceptual_budget.has_value());
  assert(cave.value.validation.perceptual_continuity_budget.has_value());
  assert(cave.value.validation.perceptual_continuity_budget->id == "entry_world_reaction");
  assert(!cave.value.validation.perceptual_continuity_budget->required_channels.empty());
  assert(!cave.value.validation.perceptual_continuity_budget->reaction_packages.empty());
  assert(cave.value.validation.perception_ledger.has_value());
  assert(cave.value.validation.perception_ledger->id == "entry_sensory_state_graph");
  assert(cave.value.validation.perception_ledger->minimum_score > 0.0f);
  assert(cave.value.validation.perception_ledger->required_channels.size() == 9u);
  assert(cave.value.validation.perception_ledger->cells.size() == 2u);
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
  assert(execution.contract_stamp == aster::sdk::actionGraphContractStamp(graph.value));
  assert(execution.events.front().type == "emit_event");
  assert(execution.events.front().target == "supply_chest");
  assert(execution.events.front().parameters.at("event") == "container.open_requested");
  assert(execution.events.front().deterministic_stamp != 0u);

  const auto mining_graph =
      aster::sdk::loadActionGraphDocument(project_root / "actions" / "mine_coal_ore.action_graph");
  assert(mining_graph.ok());
  assert(mining_graph.value.reaction_contracts.size() == 1u);
  assert(mining_graph.value.reaction_contracts[0].id == "coal_mining_reaction");
  assert(!mining_graph.value.reaction_contracts[0].required_events.empty());

  const auto input = aster::sdk::loadInputMapDocument(project_root / "inputs" / "lumen_run.input");
  assert(input.ok());
  assert(input.value.id == "input.lumen_run");
  assert(input.value.bindings.size() == 3u);
  assert(input.value.bindings.front().command == "world.interact");
  assert(input.value.bindings.front().device == "keyboard");
  assert(input.value.bindings.front().key == "E");
  assert(aster::sdk::inputMapContractStamp(input.value) != 0u);
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

  const auto bad_input = aster::sdk::parseInputMapDocument(R"json({
    "schema_version": 1,
    "id": "input.bad",
    "bindings": [
      { "command": "world.interact", "device": "mouse" }
    ]
  })json");
  assert(!bad_input.ok());

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

void testGameplayContractFixtureDocuments() {
  const std::filesystem::path fixture_root =
      sourceRoot() / "tests" / "fixtures" / "gameplay_contract";
  const auto project = aster::sdk::loadProjectDocument(fixture_root / "demo.asterproj");
  assert(project.ok());
  assert(project.value.name == "Aster Gameplay Contract Demo");
  assert(project.value.assets.size() == 3u);

  const auto scene = aster::sdk::loadSceneDocument(fixture_root / "scenes" / "entry.scene");
  assert(scene.ok());
  aster::sdk::World world;
  assert(world.instantiate(scene.value).ok());
  const aster::sdk::EntityInstance *door = world.findEntity("demo.door");
  assert(door != nullptr);
  assert(door->definition.components.interactable.has_value());
  assert(door->definition.components.interactable->action_graph == "action.demo.door_open");

  const auto action =
      aster::sdk::loadActionGraphDocument(fixture_root / "actions" / "door_open.action_graph");
  assert(action.ok());
  assert(aster::sdk::actionGraphContractStamp(action.value) != 0u);
  const aster::sdk::ActionExecution execution =
      aster::sdk::ActionGraphRuntime{}.execute(
          action.value, {.actor = "demo.player", .target = "demo.door", .input = "world.interact"});
  assert(execution.ok());
  assert(execution.events.size() == 2u);
  assert(execution.events.front().parameters.at("input") == "world.interact");

  const auto input = aster::sdk::loadInputMapDocument(fixture_root / "inputs" / "demo.input");
  assert(input.ok());
  assert(input.value.bindings.size() == 2u);
  assert(input.value.bindings.front().tags.front().value == "input.primary");
  assert(aster::sdk::inputMapContractStamp(input.value) != 0u);
}

void testAgentWorkspacePlanning() {
  const std::filesystem::path project_root = sourceRoot() / "projects" / "lumen_run";
  const auto project = aster::sdk::loadProjectDocument(project_root / "lumen_run.asterproj");
  assert(project.ok());

  aster::sdk::AsterAgentWorkspaceOptions options;
  options.objective = "Make Lumen Run easier for an agent to extend in batches.";
  options.project_file = project_root / "lumen_run.asterproj";
  const aster::sdk::AsterAgentWorkspaceProfile profile =
      aster::sdk::createAsterAgentWorkspaceProfile(project.value, project_root, options);
  assert(profile.name == "Lumen Run Agent Workspace");
  assert(profile.metadata.at("kernel_changes") == "locked");
  assert(!profile.scopes.empty());
  assert(!profile.validation.empty());
  assert(!profile.output_contracts.empty());

  const aster::sdk::AsterAgentWorkspaceAudit audit =
      aster::sdk::auditAsterAgentWorkspace(project.value, profile);
  assert(audit.ok());
  assert(audit.assets.size() == project.value.assets.size());
  assert(!audit.recommended_batches.empty());

  aster::sdk::AsterAgentTaskBoard board =
      aster::sdk::planAsterAgentAuthoringBatches(project.value, profile);
  assert(board.task("agent.map_project_contracts") != nullptr);
  assert(board.task("agent.scene_foundation") != nullptr);
  assert(board.task("agent.visual_proof_surface") != nullptr);
  assert(board.task("agent.gameplay_loop_contracts") != nullptr);
  assert(!board.readyTasks().empty());
  assert(board.readyTasks().front().id == "agent.map_project_contracts");
  assert(!board.blockedTasks().empty());
  assert(board.setStatus("agent.map_project_contracts", aster::sdk::AsterAgentTaskStatus::Complete));
  assert(board.setStatus("agent.normalize_authoring_surface",
                         aster::sdk::AsterAgentTaskStatus::Complete));
  const std::vector<aster::sdk::AsterAgentTask> ready_after_contracts = board.readyTasks();
  assert(!ready_after_contracts.empty());
  assert(ready_after_contracts.front().id == "agent.scene_foundation");
  assert(board.contractStamp() != 0u);

  const std::string schema = aster::sdk::asterAgentBatchOutputSchemaJson();
  assert(schema.find("Aster Agent Batch Report") != std::string::npos);
  assert(schema.find("changed_files") != std::string::npos);

  const std::string prompt = aster::sdk::makeAsterAgentPrompt(profile, project.value, board);
  assert(prompt.find("Lumen Run") != std::string::npos);
  assert(prompt.find("Do not add third-party notice files") != std::string::npos);

  aster::sdk::AsterAgentHandoff handoff;
  handoff.session_id = "test-session";
  handoff.summary = "Mapped Aster authoring contracts.";
  handoff.decisions.push_back("Keep gameplay in action graphs before sample C++.");
  handoff.changed_paths.push_back("projects/lumen_run/lumen_run.asterproj");
  handoff.remaining_tasks = ready_after_contracts;
  const std::string handoff_markdown = aster::sdk::summarizeAsterAgentHandoffMarkdown(handoff);
  assert(handoff_markdown.find("Aster Agent Handoff") != std::string::npos);
  assert(handoff_markdown.find("test-session") != std::string::npos);
  assert(aster::sdk::parseAsterAgentDomain("action-graph") ==
         aster::sdk::AsterAgentDomain::ActionGraph);
}

void testAgentRunbookInstructionsAndCommandPolicy() {
  const std::filesystem::path temp_root =
      std::filesystem::temp_directory_path() / "aster_agent_runbook_public_consumer";
  std::filesystem::remove_all(temp_root);
  std::filesystem::create_directories(temp_root / "projects" / "demo" / "scenes");
  {
    std::ofstream root_instructions(temp_root / "AGENTS.md");
    root_instructions << "# Root\n\n";
    root_instructions << "- Preserve Aster ownership boundaries.\n";
    root_instructions << "- Never add third-party notice files in agent batches.\n";
  }
  {
    std::ofstream project_instructions(temp_root / "projects" / "demo" / "AGENTS.md");
    project_instructions << "# Demo\n\n";
    project_instructions << "1. Prefer .scene edits before runtime shortcuts.\n";
  }

  const aster::sdk::AsterAgentInstructionStack instructions =
      aster::sdk::loadAsterAgentInstructions(
          temp_root / "projects" / "demo" / "scenes" / "entry.scene",
          {.workspace_root = temp_root});
  assert(instructions.ok());
  assert(instructions.files.size() == 2u);
  const std::vector<std::string> rules = instructions.flattenedRules();
  assert(rules.size() == 3u);
  assert(rules.front().find("Aster ownership") != std::string::npos);
  assert(instructions.contractStamp() != 0u);
  assert(instructions.summarizeMarkdown().find("Aster Agent Instruction Stack") !=
         std::string::npos);

  const aster::sdk::AsterAgentCommandPolicy policy =
      aster::sdk::createDefaultAsterAgentCommandPolicy();
  assert(policy.reviewShellCommand("cmake --build build --target aster_game_sdk_public_consumer")
             .decision == aster::sdk::AsterAgentCommandDecision::Allow);
  assert(policy.reviewShellCommand("git reset --hard").decision ==
         aster::sdk::AsterAgentCommandDecision::Deny);
  assert(policy.reviewShellCommand("python custom_tool.py").decision ==
         aster::sdk::AsterAgentCommandDecision::Review);
  assert(policy.summarizeMarkdown().find("third-party notice") != std::string::npos);
  assert(aster::sdk::asterAgentCommandDecisionName(aster::sdk::AsterAgentCommandDecision::Deny) ==
         "deny");

  const auto project = aster::sdk::parseProjectDocument(R"json({
    "schema_version": 1,
    "name": "Demo",
    "startup_scene": "scene.entry",
    "assets": [
      { "id": "scene.entry", "kind": "scene", "path": "scenes/entry.scene" }
    ]
  })json");
  assert(project.ok());
  aster::sdk::AsterAgentRunbookOptions runbook_options;
  runbook_options.workspace_root = temp_root;
  aster::sdk::AsterAgentWorkspaceOptions workspace_options;
  workspace_options.project_file = temp_root / "projects" / "demo" / "demo.asterproj";
  const aster::sdk::AsterAgentRunbook runbook = aster::sdk::createAsterAgentRunbook(
      project.value, temp_root / "projects" / "demo", runbook_options, workspace_options);
  assert(runbook.instructions.files.size() == 2u);
  assert(!runbook.command_policy.rules().empty());
  assert(runbook.contractStamp() != 0u);
  assert(runbook.summarizeMarkdown().find("Aster Agent Runbook") != std::string::npos);
  std::filesystem::remove_all(temp_root);
}

void testAgentAssetBriefReviewGate() {
  const aster::sdk::AsterAgentAssetBrief brief =
      aster::sdk::makeIndustrialPipeAssetBrief("reference/industrial_pipe.png");
  assert(brief.target_asset == "asset_graph.pipe_lab.rusted_pipe");
  assert(!brief.required_signals.empty());
  assert(!brief.forbidden_signals.empty());
  assert(brief.minimum_score > 0.8f);

  const std::string prompt = aster::sdk::makeAsterAgentAssetPrompt(brief);
  assert(prompt.find("reference/industrial_pipe.png") != std::string::npos);
  assert(prompt.find("smooth_black_pipe") != std::string::npos);
  assert(prompt.find("z_fight_free_surface_attachments") != std::string::npos);
  assert(prompt.find("Aster Agent Asset Iteration Report") != std::string::npos);

  aster::sdk::AsterAgentAssetIteration weak_iteration;
  weak_iteration.id = "candidate.black_pipe";
  weak_iteration.artifact = "assets/screenshots/industrial_pipe.png";
  weak_iteration.notes = "smooth_black_pipe with missing_weld_rings";
  weak_iteration.claimed_signals = {"smooth_black_pipe"};
  const aster::sdk::AsterAgentAssetReview weak_review =
      aster::sdk::reviewAsterAgentAssetIteration(brief, weak_iteration);
  assert(!weak_review.passed());
  assert(!weak_review.missing_required_signals.empty());
  assert(!weak_review.present_forbidden_signals.empty());

  aster::sdk::AsterAgentAssetIteration strong_iteration;
  strong_iteration.id = "candidate.reference_matched";
  strong_iteration.artifact = "assets/screenshots/industrial_pipe.png";
  strong_iteration.claimed_signals = {"corroded_orange_brown_rust",
                                      "dark_oxide_cavities",
                                      "raised_weld_rings",
                                      "weld_contact_skirts",
                                      "open_hollow_rims",
                                      "soft_beveled_rims",
                                      "uneven_pitting",
                                      "layered_corrosion_stack",
                                      "physical_roughness_metalness_split",
                                      "axial_scratches",
                                      "z_fight_free_surface_attachments",
                                      "reference_silhouette"};
  strong_iteration.rejected_signals = {"smooth_black_pipe",
                                       "decorative_bolts_without_reference",
                                       "clean_plastic_surface",
                                       "monochrome_material",
                                       "missing_weld_rings",
                                       "floating_weld_rings",
                                       "coplanar_seam_stripe",
                                       "knife_edge_rims",
                                       "single_layer_orange_oxide"};
  const aster::sdk::AsterAgentAssetReview strong_review =
      aster::sdk::reviewAsterAgentAssetIteration(brief, strong_iteration);
  assert(strong_review.passed());
  assert(strong_review.score >= brief.minimum_score);
  assert(aster::sdk::asterAgentAssetReviewStatusName(strong_review.status) == "passed");
}

} // namespace

int main() {
  testLumenProjectAuthoringDocumentsLoad();
  testSchemaDiagnosticsRejectInvalidDocuments();
  testWorldRejectsDuplicateEntityInstances();
  testGameplayContractFixtureDocuments();
  testAgentWorkspacePlanning();
  testAgentRunbookInstructionsAndCommandPolicy();
  testAgentAssetBriefReviewGate();
  std::cout << "game_sdk_public_consumer passed.\n";
  return 0;
}
