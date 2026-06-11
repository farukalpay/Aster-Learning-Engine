// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#pragma once

#include "aster/learning/learning_session.hpp"
#include "aster/samples/lumen_run/lumen_run.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace aster {

struct LumenLearningProofResult {
  bool passed = false;
  LearningSessionReport report;
  LearningSessionArtifacts artifacts;
  std::vector<std::string> diagnostics;
};

[[nodiscard]] LumenLearningProofResult
runLumenMiningLearningProof(const LumenAuthoringData &authoring, LearningSession session,
                            const std::filesystem::path &output_dir);

} // namespace aster
