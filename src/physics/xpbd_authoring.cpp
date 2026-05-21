// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#include "aster/physics/xpbd_authoring.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <string>
#include <system_error>
#include <unordered_set>

namespace aster {
namespace {

[[nodiscard]] std::string lowerExtension(const std::filesystem::path &path) {
  std::string extension = path.extension().string();
  std::transform(extension.begin(), extension.end(), extension.begin(), [](const unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  return extension;
}

[[nodiscard]] float pinAxisValue(const Vec3 value, const XpbdPinAxis axis) {
  switch (axis) {
  case XpbdPinAxis::X:
    return value.x;
  case XpbdPinAxis::Y:
    return value.y;
  case XpbdPinAxis::Z:
  default:
    return value.z;
  }
}

[[nodiscard]] bool shouldPin(const Vec3 position, const XpbdPinRule rule) {
  if (!rule.enabled) {
    return false;
  }
  const float value = pinAxisValue(position, rule.axis);
  return rule.pin_greater_equal ? value >= rule.threshold : value <= rule.threshold;
}

[[nodiscard]] std::uint64_t edgeKey(const std::uint32_t lhs, const std::uint32_t rhs) {
  const std::uint32_t a = std::min(lhs, rhs);
  const std::uint32_t b = std::max(lhs, rhs);
  return (static_cast<std::uint64_t>(a) << 32u) | b;
}

void rebuildNormals(CpuMesh &mesh) {
  for (Vertex &vertex : mesh.vertices) {
    vertex.normal = {};
  }
  for (std::size_t i = 0u; i + 2u < mesh.indices.size(); i += 3u) {
    const std::uint32_t ia = mesh.indices[i];
    const std::uint32_t ib = mesh.indices[i + 1u];
    const std::uint32_t ic = mesh.indices[i + 2u];
    if (ia >= mesh.vertices.size() || ib >= mesh.vertices.size() || ic >= mesh.vertices.size()) {
      continue;
    }
    Vertex &a = mesh.vertices[ia];
    Vertex &b = mesh.vertices[ib];
    Vertex &c = mesh.vertices[ic];
    const Vec3 normal = normalizeOr(cross(b.position - a.position, c.position - a.position),
                                    {0.0f, 1.0f, 0.0f});
    a.normal += normal;
    b.normal += normal;
    c.normal += normal;
  }
  for (Vertex &vertex : mesh.vertices) {
    vertex.normal = normalizeOr(vertex.normal, {0.0f, 1.0f, 0.0f});
  }
}

[[nodiscard]] std::filesystem::path nextBackupPath(const std::filesystem::path &source_path) {
  const std::filesystem::path first =
      source_path.parent_path() / (source_path.filename().string() + ".asterbak");
  std::error_code error;
  if (!std::filesystem::exists(first, error)) {
    return first;
  }
  for (int i = 1; i < 1000; ++i) {
    const std::filesystem::path candidate =
        source_path.parent_path() /
        (source_path.filename().string() + ".asterbak." + std::to_string(i));
    if (!std::filesystem::exists(candidate, error)) {
      return candidate;
    }
  }
  return source_path.parent_path() / (source_path.filename().string() + ".asterbak.latest");
}

void absorbDiagnostics(std::vector<std::string> &out, const std::vector<std::string> &source) {
  out.insert(out.end(), source.begin(), source.end());
}

[[nodiscard]] bool hasError(const std::vector<std::string> &diagnostics) {
  return std::any_of(diagnostics.begin(), diagnostics.end(), [](const std::string &message) {
    return message.rfind("error:", 0u) == 0u;
  });
}

} // namespace

std::string_view xpbdPinAxisName(const XpbdPinAxis axis) {
  switch (axis) {
  case XpbdPinAxis::X:
    return "x";
  case XpbdPinAxis::Y:
    return "y";
  case XpbdPinAxis::Z:
    return "z";
  }
  return "unknown";
}

bool isXpbdEditableMeshSource(const std::filesystem::path &path) {
  return lowerExtension(path) == ".obj";
}

std::vector<XpbdParticle> makeXpbdParticlesFromMesh(const CpuMesh &mesh) {
  std::vector<XpbdParticle> particles;
  particles.reserve(mesh.vertices.size());
  for (const Vertex &vertex : mesh.vertices) {
    particles.push_back({.position = vertex.position,
                         .previous_position = vertex.position,
                         .velocity = {},
                         .inverse_mass = 1.0f,
                         .pinned = false});
  }
  return particles;
}

std::size_t applyXpbdPinRule(std::vector<XpbdParticle> &particles, const XpbdPinRule rule) {
  std::size_t pinned = 0u;
  for (XpbdParticle &particle : particles) {
    particle.pinned = shouldPin(particle.position, rule);
    if (particle.pinned) {
      particle.inverse_mass = 0.0f;
      particle.velocity = {};
      ++pinned;
    } else if (particle.inverse_mass <= 0.0f) {
      particle.inverse_mass = 1.0f;
    }
  }
  return pinned;
}

std::vector<XpbdDistanceConstraint>
makeXpbdEdgeDistanceConstraints(const CpuMesh &mesh, const float compliance,
                                const float damping) {
  std::vector<XpbdDistanceConstraint> constraints;
  constraints.reserve(mesh.indices.size());
  std::unordered_set<std::uint64_t> seen;
  const auto add_edge = [&](const std::uint32_t a, const std::uint32_t b) {
    if (a >= mesh.vertices.size() || b >= mesh.vertices.size() || a == b) {
      return;
    }
    const std::uint64_t key = edgeKey(a, b);
    if (!seen.insert(key).second) {
      return;
    }
    constraints.push_back({.a = a,
                           .b = b,
                           .rest_length =
                               length(mesh.vertices[b].position - mesh.vertices[a].position),
                           .compliance = std::max(0.0f, compliance),
                           .damping = std::clamp(damping, 0.0f, 1.0f)});
  };
  for (std::size_t i = 0u; i + 2u < mesh.indices.size(); i += 3u) {
    add_edge(mesh.indices[i], mesh.indices[i + 1u]);
    add_edge(mesh.indices[i + 1u], mesh.indices[i + 2u]);
    add_edge(mesh.indices[i + 2u], mesh.indices[i]);
  }
  return constraints;
}

CpuMesh applyXpbdParticlesToMesh(const CpuMesh &mesh,
                                 const std::vector<XpbdParticle> &particles) {
  CpuMesh out = mesh;
  const std::size_t count = std::min(out.vertices.size(), particles.size());
  for (std::size_t i = 0u; i < count; ++i) {
    out.vertices[i].position = particles[i].position;
  }
  rebuildNormals(out);
  return out;
}

XpbdMeshAuthoringSession makeXpbdMeshAuthoringSession(
    const CpuMesh &mesh, XpbdMeshAuthoringSettings settings) {
  XpbdMeshAuthoringSession session;
  session.source_mesh = mesh;
  session.preview_mesh = mesh;
  session.settings = settings;
  session.particles = makeXpbdParticlesFromMesh(mesh);
  session.pinned_particles = applyXpbdPinRule(session.particles, settings.pin_rule);
  session.constraints =
      makeXpbdEdgeDistanceConstraints(mesh, settings.compliance, settings.damping);
  session.source_loaded = !mesh.vertices.empty() && !mesh.indices.empty();
  if (!session.source_loaded) {
    session.diagnostics.push_back("error: XPBD authoring requires a non-empty triangle mesh");
  }
  if (session.constraints.empty()) {
    session.diagnostics.push_back("warning: mesh has no valid edges for XPBD constraints");
  }
  if (settings.pin_rule.enabled && session.pinned_particles == 0u) {
    session.diagnostics.push_back("warning: pin rule did not match any vertices");
  }
  return session;
}

XpbdSimulationReport simulateXpbdMeshAuthoringSession(XpbdMeshAuthoringSession &session) {
  XpbdSimulationReport aggregate;
  aggregate.stable = true;
  if (!session.source_loaded || session.constraints.empty()) {
    aggregate.stable = false;
    session.last_simulation = aggregate;
    return aggregate;
  }
  const std::uint32_t frames = std::max(session.settings.frames, 1u);
  XpbdSimulationSettings step_settings = session.settings.simulation;
  step_settings.dt = std::max(step_settings.dt, 0.0001f);
  step_settings.iterations = std::max(step_settings.iterations, 1);
  const float damping = std::clamp(session.settings.linear_damping, 0.0f, 1.0f);
  for (std::uint32_t frame = 0u; frame < frames; ++frame) {
    if (damping > 0.0f) {
      for (XpbdParticle &particle : session.particles) {
        if (!particle.pinned) {
          particle.velocity *= (1.0f - damping);
        }
      }
    }
    const XpbdSimulationReport step =
        simulateXpbdDistanceConstraints(session.particles, session.constraints, step_settings);
    aggregate.constraints_solved += step.constraints_solved;
    aggregate.max_distance_error =
        std::max(aggregate.max_distance_error, step.max_distance_error);
    aggregate.stable = aggregate.stable && step.stable;
  }
  session.preview_mesh = applyXpbdParticlesToMesh(session.source_mesh, session.particles);
  session.last_simulation = aggregate;
  return aggregate;
}

XpbdSourceEditReport writeXpbdMeshSourceEdit(const std::filesystem::path &source_path,
                                             const CpuMesh &mesh) {
  XpbdSourceEditReport report;
  report.source_path = source_path;
  report.editable_source = isXpbdEditableMeshSource(source_path);
  if (!report.editable_source) {
    report.diagnostics.push_back("error: XPBD source edits are only supported for OBJ sources");
    return report;
  }
  std::error_code error;
  if (!std::filesystem::is_regular_file(source_path, error)) {
    report.diagnostics.push_back("error: source OBJ does not exist");
    return report;
  }
  report.backup_path = nextBackupPath(source_path);
  std::filesystem::copy_file(source_path, report.backup_path,
                             std::filesystem::copy_options::none, error);
  if (error) {
    report.diagnostics.push_back("error: could not create source backup: " + error.message());
    return report;
  }
  report.export_report = exportMeshAssetObj(mesh, source_path);
  absorbDiagnostics(report.diagnostics, report.export_report.diagnostics);
  report.applied = report.export_report.ok;
  if (!report.export_report.ok) {
    std::filesystem::copy_file(report.backup_path, source_path,
                               std::filesystem::copy_options::overwrite_existing, error);
    if (error) {
      report.diagnostics.push_back("error: could not restore source from backup: " +
                                   error.message());
    }
  }
  report.ok = report.applied && !hasError(report.diagnostics);
  if (!report.ok && report.applied) {
    report.diagnostics.push_back("error: source export completed with diagnostics");
  }
  return report;
}

XpbdSourceEditReport simulateXpbdMeshSourceEdit(const std::filesystem::path &source_path,
                                                XpbdMeshAuthoringSettings settings) {
  XpbdSourceEditReport report;
  report.source_path = source_path;
  report.editable_source = isXpbdEditableMeshSource(source_path);
  if (!report.editable_source) {
    report.diagnostics.push_back("error: XPBD source edits are only supported for OBJ sources");
    return report;
  }
  const AssetMeshImportResult imported = importMeshAsset(source_path, AssetMeshFormat::Obj);
  report.import_report = imported.report;
  absorbDiagnostics(report.diagnostics, imported.report.diagnostics);
  if (!imported.report.ok) {
    report.diagnostics.push_back("error: could not import OBJ for XPBD edit");
    return report;
  }
  XpbdMeshAuthoringSession session =
      makeXpbdMeshAuthoringSession(imported.mesh, settings);
  absorbDiagnostics(report.diagnostics, session.diagnostics);
  const XpbdSimulationReport simulation = simulateXpbdMeshAuthoringSession(session);
  if (!simulation.stable) {
    report.diagnostics.push_back("error: XPBD simulation did not remain stable");
    return report;
  }
  XpbdSourceEditReport write_report = writeXpbdMeshSourceEdit(source_path, session.preview_mesh);
  write_report.import_report = report.import_report;
  write_report.diagnostics.insert(write_report.diagnostics.begin(), report.diagnostics.begin(),
                                  report.diagnostics.end());
  write_report.ok = write_report.applied && !hasError(write_report.diagnostics);
  return write_report;
}

} // namespace aster
