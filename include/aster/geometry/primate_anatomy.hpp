// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/math/vec.hpp"
#include "aster/render/mesh.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace aster {

enum class AnatomicalTissue {
  Bone,
  Enamel,
  Muscle,
  Tendon,
  SoftTissue,
  PlantarPad,
  FurSkin,
};

struct AnatomicalLandmark {
  std::string id;
  Vec3 position{};
  float value = 0.0f;
};

struct AnatomicalModelPart {
  std::string name;
  AnatomicalTissue tissue = AnatomicalTissue::Bone;
  CpuMesh mesh;
  std::vector<AnatomicalLandmark> landmarks;
};

struct CercopithecidaeCraniofacialMetrics {
  float neurocranium_volume_proxy = 0.0f;
  float supraorbital_ridge_projection = 0.0f;
  float zygomatic_arch_span = 0.0f;
  float maxilla_prognathism = 0.0f;
  float mandibular_ramus_height = 0.0f;
  int molar_count = 0;
  int bilophodont_loph_pairs = 0;
  int cranial_suture_count = 0;
  int dentition_cusp_count = 0;
};

struct CercopithecidaePostcranialMetrics {
  int cervical_vertebrae = 0;
  int thoracic_vertebrae = 0;
  int lumbar_vertebrae = 0;
  int rib_pairs = 0;
  float pronation_supination_range_degrees = 0.0f;
  float opposable_pollex_angle_degrees = 0.0f;
  float iliac_crest_width = 0.0f;
  float femoroacetabular_angle_degrees = 0.0f;
  float plantar_pad_thickness = 0.0f;
  float pes_phalanx_elongation = 0.0f;
  int tendon_band_count = 0;
  int fur_strand_guides = 0;
};

struct CercopithecidaeMorphologyReport {
  CercopithecidaeCraniofacialMetrics craniofacial;
  CercopithecidaePostcranialMetrics postcranial;
  std::vector<AnatomicalLandmark> landmarks;
};

struct CercopithecidaeMorphologySpec {
  float scale = 1.0f;
  int surface_segments = 24;
  int surface_rings = 12;
  bool include_soft_tissue = true;
  bool include_muscle_insertions = true;
  bool include_surface_pads = true;
  bool include_surface_detail = true;
  int fur_strand_guides = 72;
  float surface_detail_strength = 1.0f;
};

struct AnatomicalModel {
  std::vector<AnatomicalModelPart> parts;
  CercopithecidaeMorphologyReport report;

  [[nodiscard]] CpuMesh mergedMesh() const;
  [[nodiscard]] std::size_t vertexCount() const;
  [[nodiscard]] std::size_t indexCount() const;
};

[[nodiscard]] const char *anatomicalTissueName(AnatomicalTissue tissue);
[[nodiscard]] AnatomicalModel makeCercopithecidaeModel(CercopithecidaeMorphologySpec spec = {});
[[nodiscard]] CpuMesh makeCercopithecidaeMesh(CercopithecidaeMorphologySpec spec = {});

} // namespace aster
