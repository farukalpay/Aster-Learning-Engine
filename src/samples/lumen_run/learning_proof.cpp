// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#include "aster/samples/lumen_run/learning_proof.hpp"

#include <algorithm>

namespace aster {
namespace {

[[nodiscard]] bool focusCoalOre(LumenRun &game, const Vec3 ore_position) {
  const Vec3 player_position = ore_position + Vec3{0.0f, 0.05f, 1.45f};
  game.relocatePlayer(player_position, radians(180.0f));
  const Vec3 focus_origin = player_position + Vec3{0.0f, 0.42f, 0.0f};
  game.updateInteractionFocus(focus_origin, normalize(ore_position - focus_origin), 1.0f / 60.0f);
  return game.focusPromptModel().visible;
}

void ingestSignals(LumenRun &game, LearningSession &session) {
  session.observe(game.drainLearningSignals());
}

} // namespace

LumenLearningProofResult runLumenMiningLearningProof(const LumenAuthoringData &authoring,
                                                     LearningSession session,
                                                     const std::filesystem::path &output_dir) {
  LumenLearningProofResult result;
  if (!session.valid()) {
    result.diagnostics.push_back("learning session is invalid");
    return result;
  }

  LumenRun game(authoring, {.shard_count = 3, .sentinel_count = 0});
  if (!game.caveWorldGateAccepted()) {
    result.diagnostics.push_back("Lumen cave world gate rejected the learning proof route");
    return result;
  }

  const auto ore =
      std::find_if(game.scene().objects().begin(), game.scene().objects().end(),
                   [](const RenderObject &object) { return object.name == "Coal ore vein node"; });
  if (ore == game.scene().objects().end()) {
    result.diagnostics.push_back("learning proof route could not find the authored coal ore node");
    return result;
  }
  const Vec3 ore_position = ore->transform.position;

  if (!focusCoalOre(game, ore_position)) {
    result.diagnostics.push_back("learning proof route could not focus the coal ore node");
    return result;
  }
  ingestSignals(game, session);
  game.interactFocused();
  ingestSignals(game, session);

  const std::vector<LearningScaffoldCandidate> scaffolds = session.scaffoldCandidates();
  const auto pickaxe_scaffold =
      std::find_if(scaffolds.begin(), scaffolds.end(),
                   [](const auto &row) { return row.scaffold_id == "scaffold.pickaxe_prompt"; });
  if (pickaxe_scaffold == scaffolds.end() ||
      !session.selectScaffold(pickaxe_scaffold->scaffold_id)) {
    result.diagnostics.push_back(
        "learning proof route did not produce the grounded pickaxe scaffold");
    return result;
  }

  if (!game.takeChestItem("pickaxe") || !game.takeChestItem("torch")) {
    result.diagnostics.push_back("learning proof route could not transfer pickaxe and torch items");
    return result;
  }
  ingestSignals(game, session);

  bool torch_equipped = false;
  for (std::size_t slot = 0u; slot < 6u && !torch_equipped; ++slot) {
    game.selectHotbarSlot(slot);
    game.update(1.0f / 60.0f, {}, false, false);
    ingestSignals(game, session);
    torch_equipped = game.equippedLight().has_value();
  }
  if (!torch_equipped) {
    result.diagnostics.push_back("learning proof route could not equip the authored torch");
    return result;
  }

  bool mined = false;
  for (std::size_t slot = 0u; slot < 6u && !mined; ++slot) {
    game.selectHotbarSlot(slot);
    if (!focusCoalOre(game, ore_position)) {
      continue;
    }
    game.interactFocused();
    game.update(1.0f / 60.0f, {}, false, false);
    ingestSignals(game, session);
    mined = game.worldForensics().coal_mining_reaction.accepted;
  }
  if (!mined) {
    result.diagnostics.push_back("learning proof route could not complete a supported mining hit");
    return result;
  }

  if (!session.claimMastery()) {
    result.diagnostics.push_back("learning session refused mastery after the gameplay route");
    return result;
  }
  result.report = session.evaluate();
  result.artifacts = session.writeArtifacts(output_dir);
  if (!result.artifacts.written) {
    result.diagnostics.push_back(result.artifacts.diagnostic);
  }
  result.passed = result.report.passed && result.artifacts.written;
  return result;
}

} // namespace aster
