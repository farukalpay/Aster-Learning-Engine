// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/geometry/primate_anatomy.hpp"

#include "aster/geometry/mesh_modeling.hpp"
#include "aster/geometry/procedural_modeling.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace aster {
namespace {

[[nodiscard]] int detailSegments(const CercopithecidaeMorphologySpec &spec, const int minimum) {
  return std::max(minimum, spec.surface_segments);
}

[[nodiscard]] int detailRings(const CercopithecidaeMorphologySpec &spec, const int minimum) {
  return std::max(minimum, spec.surface_rings);
}

[[nodiscard]] Vec3 scaled(const Vec3 value, const float scale) {
  return value * scale;
}

[[nodiscard]] CpuMesh ellipsoid(const Vec3 center, const Vec3 radius,
                                const CercopithecidaeMorphologySpec &spec,
                                const Vec2 uv_origin = {}) {
  return makeEllipsoidSection({.center = scaled(center, spec.scale),
                               .radius = scaled(radius, spec.scale),
                               .segments = detailSegments(spec, 12),
                               .rings = detailRings(spec, 6),
                               .uv_origin = uv_origin,
                               .uv_scale = {0.18f, 0.18f}});
}

[[nodiscard]] CpuMesh tube(const std::vector<Vec3> &points, const float radius,
                           const CercopithecidaeMorphologySpec &spec,
                           const float vertical_scale = 1.0f) {
  SweepPath path;
  path.points.reserve(points.size());
  for (std::size_t i = 0u; i < points.size(); ++i) {
    path.points.push_back({.position = scaled(points[i], spec.scale),
                           .up = {0.0f, 1.0f, 0.0f},
                           .radius_scale = 1.0f,
                           .uv = {0.0f, static_cast<float>(i)}});
  }
  return makeSweptTube({.path = std::move(path),
                        .profile = makeCircularSweepProfile(
                            std::max(6, detailSegments(spec, 12) / 2), 1.0f, vertical_scale),
                        .radius = radius * spec.scale});
}

[[nodiscard]] CpuMesh ovalTube(const Vec3 center, const float radius_x, const float radius_y,
                               const float z, const float tube_radius,
                               const CercopithecidaeMorphologySpec &spec) {
  std::vector<Vec3> points;
  const int segments = std::max(18, detailSegments(spec, 24));
  points.reserve(static_cast<std::size_t>(segments + 1));
  for (int i = 0; i <= segments; ++i) {
    const float u = static_cast<float>(i) / static_cast<float>(segments);
    const float theta = u * pi() * 2.0f;
    points.push_back({center.x + std::cos(theta) * radius_x,
                      center.y + std::sin(theta) * radius_y,
                      z});
  }
  return tube(points, tube_radius, spec, 0.72f);
}

void appendMesh(CpuMesh &target, const CpuMesh &source) {
  mergeMesh(target, source);
}

void finishPart(AnatomicalModelPart &part) {
  rebuildAngleWeightedNormals(part.mesh);
}

void addLandmark(CercopithecidaeMorphologyReport &report, std::string id, const Vec3 position,
                 const float value) {
  report.landmarks.push_back({.id = std::move(id), .position = position, .value = value});
}

void pushPart(AnatomicalModel &model, AnatomicalModelPart part) {
  finishPart(part);
  for (const AnatomicalLandmark &landmark : part.landmarks) {
    model.report.landmarks.push_back(landmark);
  }
  model.parts.push_back(std::move(part));
}

[[nodiscard]] AnatomicalModelPart craniofacialComplex(const CercopithecidaeMorphologySpec &spec,
                                                      CercopithecidaeMorphologyReport &report) {
  AnatomicalModelPart part{.name = "cercopithecidae craniofacial skeleton",
                           .tissue = AnatomicalTissue::Bone};
  appendMesh(part.mesh, ellipsoid({0.0f, 1.68f, 0.18f}, {0.34f, 0.31f, 0.39f}, spec));
  appendMesh(part.mesh, ellipsoid({0.0f, 1.48f, 0.62f}, {0.25f, 0.13f, 0.24f}, spec,
                                  {0.20f, 0.0f}));
  appendMesh(part.mesh, ellipsoid({0.0f, 1.42f, 0.38f}, {0.22f, 0.10f, 0.20f}, spec,
                                  {0.40f, 0.0f}));
  appendMesh(part.mesh,
             tube({{-0.22f, 1.35f, 0.58f}, {-0.12f, 1.30f, 0.72f},
                   {0.0f, 1.29f, 0.76f}, {0.12f, 1.30f, 0.72f}, {0.22f, 1.35f, 0.58f}},
                  0.025f, spec, 0.78f));
  appendMesh(part.mesh, tube({{-0.23f, 1.35f, 0.58f}, {-0.25f, 1.55f, 0.46f}},
                             0.030f, spec, 0.86f));
  appendMesh(part.mesh, tube({{0.23f, 1.35f, 0.58f}, {0.25f, 1.55f, 0.46f}},
                             0.030f, spec, 0.86f));

  CpuMesh brow = makeExtrudedRidge({.spine = {{-0.31f, 1.66f, 0.52f},
                                              {-0.18f, 1.69f, 0.56f},
                                              {0.0f, 1.70f, 0.58f},
                                              {0.18f, 1.69f, 0.56f},
                                              {0.31f, 1.66f, 0.52f}},
                                    .up = {0.0f, 0.0f, 1.0f},
                                    .width = 0.09f * spec.scale,
                                    .height = 0.082f * spec.scale});
  appendMesh(part.mesh, brow);

  report.craniofacial.neurocranium_volume_proxy =
      4.0f / 3.0f * pi() * 0.34f * 0.31f * 0.39f * spec.scale * spec.scale * spec.scale;
  report.craniofacial.supraorbital_ridge_projection = 0.082f * spec.scale;
  report.craniofacial.maxilla_prognathism = (0.62f + 0.24f) - (0.18f + 0.39f);
  report.craniofacial.mandibular_ramus_height = 0.20f * spec.scale;
  part.landmarks.push_back({"neurocranium.volume_proxy", scaled({0.0f, 1.68f, 0.18f}, spec.scale),
                            report.craniofacial.neurocranium_volume_proxy});
  part.landmarks.push_back({"supraorbital.ridge_projection",
                            scaled({0.0f, 1.70f, 0.60f}, spec.scale),
                            report.craniofacial.supraorbital_ridge_projection});
  part.landmarks.push_back({"maxilla.prognathism", scaled({0.0f, 1.48f, 0.86f}, spec.scale),
                            report.craniofacial.maxilla_prognathism});
  part.landmarks.push_back({"mandible.ramus_height", scaled({0.25f, 1.45f, 0.50f}, spec.scale),
                            report.craniofacial.mandibular_ramus_height});
  return part;
}

[[nodiscard]] AnatomicalModelPart dentition(const CercopithecidaeMorphologySpec &spec,
                                            CercopithecidaeMorphologyReport &report) {
  AnatomicalModelPart part{.name = "bilophodont dentition system",
                           .tissue = AnatomicalTissue::Enamel};
  int molars = 0;
  int loph_pairs = 0;
  for (const float jaw_y : {1.405f, 1.335f}) {
    for (const float side : {-1.0f, 1.0f}) {
      for (int tooth = 0; tooth < 2; ++tooth) {
        const float z = 0.54f - static_cast<float>(tooth) * 0.085f;
        const float x = side * (0.090f + static_cast<float>(tooth) * 0.055f);
        appendMesh(part.mesh, ellipsoid({x, jaw_y, z}, {0.040f, 0.018f, 0.052f}, spec,
                                        {0.64f, 0.0f}));
        for (const float loph_offset : {-0.018f, 0.018f}) {
          appendMesh(part.mesh,
                     makeExtrudedRidge({.spine = {{x - side * 0.030f, jaw_y + 0.012f,
                                                   z + loph_offset},
                                                  {x + side * 0.030f, jaw_y + 0.012f,
                                                   z + loph_offset}},
                                        .up = {0.0f, 1.0f, 0.0f},
                                        .width = 0.018f * spec.scale,
                                        .height = 0.010f * spec.scale}));
          ++loph_pairs;
        }
        ++molars;
      }
      appendMesh(part.mesh, ellipsoid({side * 0.035f, jaw_y, 0.68f},
                                      {0.020f, 0.014f, 0.028f}, spec, {0.72f, 0.0f}));
      appendMesh(part.mesh, ellipsoid({side * 0.075f, jaw_y, 0.64f},
                                      {0.018f, 0.030f, 0.020f}, spec, {0.78f, 0.0f}));
    }
  }
  report.craniofacial.molar_count = molars;
  report.craniofacial.bilophodont_loph_pairs = loph_pairs;
  part.landmarks.push_back({"dentition.molar_count", scaled({0.0f, 1.38f, 0.49f}, spec.scale),
                            static_cast<float>(molars)});
  part.landmarks.push_back({"dentition.bilophodont_loph_pairs",
                            scaled({0.0f, 1.42f, 0.49f}, spec.scale),
                            static_cast<float>(loph_pairs)});
  return part;
}

[[nodiscard]] AnatomicalModelPart periocularAndMuscle(const CercopithecidaeMorphologySpec &spec) {
  AnatomicalModelPart part{.name = "periocular tissues zygomatic temporalis insertions",
                           .tissue = AnatomicalTissue::SoftTissue};
  if (!spec.include_soft_tissue) {
    return part;
  }
  for (const float side : {-1.0f, 1.0f}) {
    appendMesh(part.mesh, ovalTube({side * 0.18f, 1.61f, 0.0f}, 0.105f, 0.066f, 0.545f, 0.011f,
                                   spec));
    appendMesh(part.mesh, tube({{side * 0.20f, 1.52f, 0.55f},
                                {side * 0.32f, 1.47f, 0.43f},
                                {side * 0.39f, 1.40f, 0.25f}},
                               0.012f, spec, 0.62f));
    appendMesh(part.mesh, tube({{side * 0.17f, 1.49f, 0.58f},
                                {side * 0.26f, 1.42f, 0.51f},
                                {side * 0.31f, 1.37f, 0.42f}},
                               0.009f, spec, 0.58f));
    appendMesh(part.mesh, ellipsoid({side * 0.28f, 1.68f, 0.18f},
                                    {0.070f, 0.16f, 0.11f}, spec, {0.82f, 0.0f}));
  }
  part.landmarks.push_back({"periocular.orbital_cavity_rim", {0.0f, 1.61f * spec.scale,
                                                               0.545f * spec.scale},
                            2.0f});
  part.landmarks.push_back({"temporalis.insertions", {0.28f * spec.scale, 1.68f * spec.scale,
                                                       0.18f * spec.scale},
                            2.0f});
  return part;
}

[[nodiscard]] AnatomicalModelPart muscleTendonSystem(const CercopithecidaeMorphologySpec &spec) {
  AnatomicalModelPart part{.name = "temporalis zygomatic gastrocnemius flexor tendon system",
                           .tissue = AnatomicalTissue::Muscle};
  if (!spec.include_muscle_insertions) {
    return part;
  }
  for (const float side : {-1.0f, 1.0f}) {
    appendMesh(part.mesh, ellipsoid({side * 0.30f, 1.68f, 0.16f},
                                    {0.058f, 0.18f, 0.12f}, spec, {0.82f, 0.18f}));
    appendMesh(part.mesh, tube({{side * 0.18f, 1.49f, 0.58f},
                                {side * 0.28f, 1.42f, 0.49f},
                                {side * 0.34f, 1.35f, 0.38f}},
                               0.010f, spec, 0.50f));
    appendMesh(part.mesh, ellipsoid({side * 0.42f, 0.28f, -0.96f},
                                    {0.052f, 0.13f, 0.085f}, spec, {0.0f, 0.66f}));
    for (const float tendon_spread : {-0.035f, 0.0f, 0.035f}) {
      appendMesh(part.mesh, tube({{side * (0.39f + tendon_spread), 0.070f, -0.54f},
                                  {side * (0.40f + tendon_spread), 0.060f, -0.36f},
                                  {side * (0.40f + tendon_spread), 0.045f, -0.20f}},
                                 0.0038f, spec, 0.36f));
    }
  }
  part.landmarks.push_back({"muscle.temporalis_insertio", scaled({0.30f, 1.68f, 0.16f}, spec.scale),
                            2.0f});
  part.landmarks.push_back({"tendon.flexor_tension_bands", scaled({0.40f, 0.06f, -0.35f}, spec.scale),
                            6.0f});
  return part;
}

[[nodiscard]] AnatomicalModelPart axialSkeleton(const CercopithecidaeMorphologySpec &spec,
                                                CercopithecidaeMorphologyReport &report) {
  AnatomicalModelPart part{.name = "axial skeleton cervical thoracic lumbar chain",
                           .tissue = AnatomicalTissue::Bone};
  constexpr int cervical = 7;
  constexpr int thoracic = 12;
  constexpr int lumbar = 7;
  const int total = cervical + thoracic + lumbar;
  for (int i = 0; i < total; ++i) {
    const float t = static_cast<float>(i) / static_cast<float>(total - 1);
    const float y = 1.30f - t * 0.45f + 0.035f * std::sin(t * pi() * 1.5f);
    const float z = 0.08f - t * 1.86f;
    const float radius = i < cervical ? 0.045f : (i < cervical + thoracic ? 0.058f : 0.066f);
    appendMesh(part.mesh, ellipsoid({0.0f, y, z}, {radius, radius * 0.72f, radius * 1.18f}, spec,
                                    {0.0f, 0.28f}));
    appendMesh(part.mesh, tube({{-radius * 0.8f, y, z}, {radius * 0.8f, y, z}}, radius * 0.20f,
                               spec, 0.70f));
  }
  appendMesh(part.mesh, tube({{0.0f, 1.30f, 0.08f}, {0.0f, 1.14f, -0.55f},
                              {0.0f, 0.95f, -1.28f}, {0.0f, 0.90f, -1.78f}},
                             0.018f, spec, 0.86f));
  report.postcranial.cervical_vertebrae = cervical;
  report.postcranial.thoracic_vertebrae = thoracic;
  report.postcranial.lumbar_vertebrae = lumbar;
  part.landmarks.push_back({"axial.cervical_count", scaled({0.0f, 1.20f, -0.12f}, spec.scale),
                            static_cast<float>(cervical)});
  part.landmarks.push_back({"axial.thoracic_count", scaled({0.0f, 1.02f, -0.80f}, spec.scale),
                            static_cast<float>(thoracic)});
  part.landmarks.push_back({"axial.lumbar_count", scaled({0.0f, 0.90f, -1.54f}, spec.scale),
                            static_cast<float>(lumbar)});
  return part;
}

[[nodiscard]] AnatomicalModelPart shoulderAndForelimb(const CercopithecidaeMorphologySpec &spec,
                                                      CercopithecidaeMorphologyReport &report) {
  AnatomicalModelPart part{.name = "scapula clavicula humerus radius ulna complex",
                           .tissue = AnatomicalTissue::Bone};
  for (const float side : {-1.0f, 1.0f}) {
    appendMesh(part.mesh, ellipsoid({side * 0.34f, 1.03f, -0.58f},
                                    {0.040f, 0.20f, 0.16f}, spec, {0.22f, 0.28f}));
    appendMesh(part.mesh, tube({{side * 0.06f, 1.20f, -0.35f},
                                {side * 0.34f, 1.08f, -0.52f}},
                               0.018f, spec, 0.72f));
    appendMesh(part.mesh, tube({{side * 0.35f, 0.96f, -0.58f},
                                {side * 0.45f, 0.64f, -0.45f}},
                               0.030f, spec, 0.78f));
    appendMesh(part.mesh, tube({{side * 0.45f, 0.64f, -0.45f},
                                {side * 0.50f, 0.31f, -0.23f}},
                               0.020f, spec, 0.62f));
    appendMesh(part.mesh, tube({{side * 0.42f, 0.63f, -0.49f},
                                {side * 0.45f, 0.31f, -0.27f}},
                               0.015f, spec, 0.56f));
  }
  report.postcranial.pronation_supination_range_degrees = 148.0f;
  part.landmarks.push_back({"antebrachium.pronation_supination",
                            scaled({0.48f, 0.48f, -0.32f}, spec.scale), 148.0f});
  return part;
}

[[nodiscard]] AnatomicalModelPart manus(const CercopithecidaeMorphologySpec &spec,
                                        CercopithecidaeMorphologyReport &report) {
  AnatomicalModelPart part{.name = "manus metacarpals opposable pollex curved phalanges",
                           .tissue = AnatomicalTissue::Bone};
  for (const float side : {-1.0f, 1.0f}) {
    appendMesh(part.mesh, ellipsoid({side * 0.48f, 0.25f, -0.17f}, {0.070f, 0.035f, 0.075f},
                                    spec, {0.42f, 0.28f}));
    for (int digit = 0; digit < 5; ++digit) {
      const float spread = (static_cast<float>(digit) - 2.0f) * 0.032f;
      const bool pollex = digit == 0;
      const float length_scale = pollex ? 0.70f : 1.0f + 0.08f * static_cast<float>(digit == 2);
      const Vec3 root{side * (0.48f + spread * 0.5f), 0.225f, -0.10f - spread};
      const Vec3 mid{side * (0.49f + spread), 0.185f,
                     -0.02f + (pollex ? 0.030f : 0.050f) * length_scale};
      const Vec3 tip{side * (0.50f + spread * (pollex ? -2.2f : 1.2f)), 0.150f,
                     0.055f + (pollex ? 0.018f : 0.080f) * length_scale};
      appendMesh(part.mesh, tube({root, mid, tip}, pollex ? 0.010f : 0.012f, spec, 0.54f));
      appendMesh(part.mesh, ellipsoid(tip, {0.011f, 0.007f, 0.018f}, spec, {0.54f, 0.28f}));
    }
  }
  report.postcranial.opposable_pollex_angle_degrees = 54.0f;
  part.landmarks.push_back({"manus.opposable_pollex_angle",
                            scaled({0.43f, 0.18f, 0.02f}, spec.scale), 54.0f});
  return part;
}

[[nodiscard]] AnatomicalModelPart pelvisAndHindlimb(const CercopithecidaeMorphologySpec &spec,
                                                    CercopithecidaeMorphologyReport &report) {
  AnatomicalModelPart part{.name = "pelvis femoroacetabular crus gastrocnemius complex",
                           .tissue = AnatomicalTissue::Bone};
  for (const float side : {-1.0f, 1.0f}) {
    appendMesh(part.mesh, ellipsoid({side * 0.24f, 0.82f, -1.72f},
                                    {0.11f, 0.20f, 0.15f}, spec, {0.62f, 0.28f}));
    appendMesh(part.mesh, ellipsoid({side * 0.19f, 0.67f, -1.52f},
                                    {0.090f, 0.048f, 0.065f}, spec, {0.72f, 0.28f}));
    appendMesh(part.mesh, tube({{side * 0.25f, 0.70f, -1.60f},
                                {side * 0.38f, 0.42f, -1.20f}},
                               0.035f, spec, 0.80f));
    appendMesh(part.mesh, tube({{side * 0.38f, 0.42f, -1.20f},
                                {side * 0.42f, 0.15f, -0.78f}},
                               0.027f, spec, 0.64f));
    appendMesh(part.mesh, tube({{side * 0.34f, 0.42f, -1.23f},
                                {side * 0.36f, 0.15f, -0.82f}},
                               0.017f, spec, 0.52f));
    if (spec.include_muscle_insertions) {
      appendMesh(part.mesh, ellipsoid({side * 0.42f, 0.28f, -0.96f},
                                      {0.048f, 0.12f, 0.075f}, spec, {0.82f, 0.28f}));
    }
  }
  report.postcranial.iliac_crest_width = 0.60f * spec.scale;
  report.postcranial.femoroacetabular_angle_degrees = 132.0f;
  part.landmarks.push_back({"pelvis.iliac_crest_width", scaled({0.0f, 0.88f, -1.72f}, spec.scale),
                            report.postcranial.iliac_crest_width});
  part.landmarks.push_back({"pelvis.femoroacetabular_angle",
                            scaled({0.25f, 0.70f, -1.60f}, spec.scale), 132.0f});
  return part;
}

[[nodiscard]] AnatomicalModelPart pesAndPads(const CercopithecidaeMorphologySpec &spec,
                                             CercopithecidaeMorphologyReport &report) {
  AnatomicalModelPart part{.name = "pes elongated phalanges flexor tendons plantar pads",
                           .tissue = AnatomicalTissue::PlantarPad};
  for (const float side : {-1.0f, 1.0f}) {
    appendMesh(part.mesh, ellipsoid({side * 0.39f, 0.075f, -0.66f},
                                    {0.080f, 0.030f, 0.13f}, spec, {0.0f, 0.48f}));
    for (int digit = 0; digit < 5; ++digit) {
      const float spread = (static_cast<float>(digit) - 2.0f) * 0.035f;
      const float elongation = digit == 2 ? 1.22f : 1.0f;
      const Vec3 root{side * (0.39f + spread * 0.4f), 0.060f, -0.54f - std::abs(spread) * 0.25f};
      const Vec3 mid{side * (0.40f + spread), 0.045f, -0.40f + 0.035f * elongation};
      const Vec3 tip{side * (0.40f + spread * 1.15f), 0.032f, -0.25f + 0.085f * elongation};
      appendMesh(part.mesh, tube({root, mid, tip}, 0.011f, spec, 0.50f));
      appendMesh(part.mesh, tube({root + Vec3{0.0f, 0.010f, -0.020f},
                                  mid + Vec3{0.0f, 0.012f, -0.005f},
                                  tip + Vec3{0.0f, 0.008f, -0.012f}},
                                 0.004f, spec, 0.42f));
    }
    if (spec.include_surface_pads) {
      appendMesh(part.mesh, ellipsoid({side * 0.39f, 0.030f, -0.48f},
                                      {0.095f, 0.026f, 0.15f}, spec, {0.18f, 0.48f}));
    }
  }
  report.postcranial.plantar_pad_thickness = 0.052f * spec.scale;
  report.postcranial.pes_phalanx_elongation = 1.22f;
  part.landmarks.push_back({"pes.plantar_pad_thickness", scaled({0.39f, 0.03f, -0.48f}, spec.scale),
                            report.postcranial.plantar_pad_thickness});
  part.landmarks.push_back({"pes.phalanx_elongation", scaled({0.40f, 0.04f, -0.25f}, spec.scale),
                            report.postcranial.pes_phalanx_elongation});
  return part;
}

[[nodiscard]] AnatomicalModelPart furSkinEnvelope(const CercopithecidaeMorphologySpec &spec) {
  AnatomicalModelPart part{.name = "fur skin anatomical envelope",
                           .tissue = AnatomicalTissue::FurSkin};
  appendMesh(part.mesh, ellipsoid({0.0f, 0.88f, -0.78f}, {0.34f, 0.30f, 0.78f}, spec,
                                  {0.36f, 0.48f}));
  appendMesh(part.mesh, ellipsoid({0.0f, 1.55f, 0.44f}, {0.29f, 0.20f, 0.31f}, spec,
                                  {0.54f, 0.48f}));
  part.landmarks.push_back({"skin.surface_envelope", scaled({0.0f, 0.88f, -0.78f}, spec.scale),
                            1.0f});
  return part;
}

} // namespace

const char *anatomicalTissueName(const AnatomicalTissue tissue) {
  switch (tissue) {
  case AnatomicalTissue::Bone:
    return "bone";
  case AnatomicalTissue::Enamel:
    return "enamel";
  case AnatomicalTissue::Muscle:
    return "muscle";
  case AnatomicalTissue::Tendon:
    return "tendon";
  case AnatomicalTissue::SoftTissue:
    return "soft-tissue";
  case AnatomicalTissue::PlantarPad:
    return "plantar-pad";
  case AnatomicalTissue::FurSkin:
    return "fur-skin";
  }
  return "unknown";
}

CpuMesh AnatomicalModel::mergedMesh() const {
  CpuMesh merged;
  for (const AnatomicalModelPart &part : parts) {
    mergeMesh(merged, part.mesh);
  }
  return merged;
}

std::size_t AnatomicalModel::vertexCount() const {
  std::size_t count = 0u;
  for (const AnatomicalModelPart &part : parts) {
    count += part.mesh.vertices.size();
  }
  return count;
}

std::size_t AnatomicalModel::indexCount() const {
  std::size_t count = 0u;
  for (const AnatomicalModelPart &part : parts) {
    count += part.mesh.indices.size();
  }
  return count;
}

AnatomicalModel makeCercopithecidaeModel(CercopithecidaeMorphologySpec spec) {
  if (spec.scale <= 0.0f || spec.surface_segments < 8 || spec.surface_rings < 4) {
    throw std::invalid_argument(
        "Cercopithecidae morphology requires positive scale and enough surface detail.");
  }

  AnatomicalModel model;
  pushPart(model, craniofacialComplex(spec, model.report));
  pushPart(model, dentition(spec, model.report));
  pushPart(model, periocularAndMuscle(spec));
  pushPart(model, muscleTendonSystem(spec));
  pushPart(model, axialSkeleton(spec, model.report));
  pushPart(model, shoulderAndForelimb(spec, model.report));
  pushPart(model, manus(spec, model.report));
  pushPart(model, pelvisAndHindlimb(spec, model.report));
  pushPart(model, pesAndPads(spec, model.report));
  pushPart(model, furSkinEnvelope(spec));
  addLandmark(model.report, "model.part_count", {}, static_cast<float>(model.parts.size()));
  return model;
}

CpuMesh makeCercopithecidaeMesh(const CercopithecidaeMorphologySpec spec) {
  return makeCercopithecidaeModel(spec).mergedMesh();
}

} // namespace aster
