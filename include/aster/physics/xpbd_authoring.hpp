// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/asset/asset_io.hpp"
#include "aster/physics/xpbd_constraints.hpp"
#include "aster/render/mesh.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace aster {

enum class XpbdPinAxis {
  X,
  Y,
  Z,
};

struct XpbdPinRule {
  XpbdPinAxis axis = XpbdPinAxis::Y;
  float threshold = 0.55f;
  bool pin_greater_equal = true;
  bool enabled = true;
};

struct XpbdMeshAuthoringSettings {
  std::uint32_t frames = 12u;
  XpbdSimulationSettings simulation{
      .dt = 1.0f / 60.0f, .iterations = 10, .gravity = {0.0f, -9.81f, 0.0f}};
  float compliance = 0.00025f;
  float damping = 0.04f;
  float linear_damping = 0.03f;
  XpbdPinRule pin_rule{};
};

struct XpbdMeshAuthoringSession {
  CpuMesh source_mesh;
  CpuMesh preview_mesh;
  XpbdMeshAuthoringSettings settings{};
  std::vector<XpbdParticle> particles;
  std::vector<XpbdDistanceConstraint> constraints;
  XpbdSimulationReport last_simulation{};
  std::size_t pinned_particles = 0u;
  bool source_loaded = false;
  std::vector<std::string> diagnostics;
};

struct XpbdSourceEditReport {
  std::filesystem::path source_path;
  std::filesystem::path backup_path;
  AssetMeshIoReport import_report;
  AssetMeshIoReport export_report;
  bool editable_source = false;
  bool applied = false;
  bool ok = false;
  std::vector<std::string> diagnostics;
};

[[nodiscard]] std::string_view xpbdPinAxisName(XpbdPinAxis axis);
[[nodiscard]] bool isXpbdEditableMeshSource(const std::filesystem::path &path);
[[nodiscard]] std::vector<XpbdParticle> makeXpbdParticlesFromMesh(const CpuMesh &mesh);
[[nodiscard]] std::size_t applyXpbdPinRule(std::vector<XpbdParticle> &particles,
                                           XpbdPinRule rule);
[[nodiscard]] std::vector<XpbdDistanceConstraint>
makeXpbdEdgeDistanceConstraints(const CpuMesh &mesh, float compliance, float damping);
[[nodiscard]] CpuMesh applyXpbdParticlesToMesh(const CpuMesh &mesh,
                                               const std::vector<XpbdParticle> &particles);
[[nodiscard]] XpbdMeshAuthoringSession
makeXpbdMeshAuthoringSession(const CpuMesh &mesh, XpbdMeshAuthoringSettings settings = {});
[[nodiscard]] XpbdSimulationReport simulateXpbdMeshAuthoringSession(
    XpbdMeshAuthoringSession &session);
[[nodiscard]] XpbdSourceEditReport writeXpbdMeshSourceEdit(
    const std::filesystem::path &source_path, const CpuMesh &mesh);
[[nodiscard]] XpbdSourceEditReport simulateXpbdMeshSourceEdit(
    const std::filesystem::path &source_path, XpbdMeshAuthoringSettings settings = {});

} // namespace aster
