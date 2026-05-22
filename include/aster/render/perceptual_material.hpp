// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#pragma once

#include "aster/scene/scene.hpp"

#include <algorithm>
#include <cmath>

namespace aster {

[[nodiscard]] inline Material
applyWorldPerceptualMaterialMemory(const RenderObject &object, const Material &base_material) {
  const WorldPerceptualPrimitive &primitive = object.perceptual_primitive;
  if (primitive.truth_hash == 0u) {
    return base_material;
  }
  const WorldPerceptualSignals &signals = primitive.signals;
  Material material = base_material;
  const float wet_history =
      std::clamp(signals.material_memory * 0.20f + signals.interaction_residue * 0.16f +
                     signals.light_history * 0.06f +
                     primitive.neural_irradiance_confidence * 0.06f,
                 0.0f, 0.42f);
  const float dirt_history =
      std::clamp(signals.interaction_residue * 0.28f + signals.contact_field * 0.24f +
                     signals.ecology_pressure * 0.12f,
                 0.0f, 0.50f);
  const float age_history =
      std::clamp(signals.light_history * 0.18f + signals.material_memory * 0.16f +
                     (1.0f - primitive.material_stability) * 0.20f,
                 0.0f, 0.44f);
  material.procedural.wetness = std::max(material.procedural.wetness, wet_history);
  material.procedural.cavity_grime =
      std::clamp(material.procedural.cavity_grime + dirt_history, 0.0f, 1.0f);
  material.procedural.height_shading =
      std::clamp(material.procedural.height_shading + signals.contact_field * 0.16f, 0.0f, 1.5f);
  material.edge_wear = std::clamp(material.edge_wear + age_history * 0.35f, 0.0f, 1.0f);
  material.ambient_occlusion =
      std::clamp(material.ambient_occlusion * (1.0f - dirt_history * 0.18f), 0.0f, 1.0f);
  material.roughness =
      std::clamp(std::lerp(material.roughness, 0.92f, dirt_history * 0.35f), 0.045f, 1.0f);
  material.detail_strength =
      std::clamp(material.detail_strength + signals.player_readable_cause * 0.08f, 0.0f, 2.0f);
  const float neural_warmth =
      std::clamp(primitive.neural_irradiance_confidence * signals.light_history, 0.0f, 1.0f);
  material.base_color.value.x =
      std::clamp(material.base_color.value.x + primitive.neural_irradiance.x * 0.08f * neural_warmth,
                 0.0f, 1.0f);
  material.base_color.value.y =
      std::clamp(material.base_color.value.y + primitive.neural_irradiance.y * 0.045f * neural_warmth,
                 0.0f, 1.0f);
  material.base_color.value.z =
      std::clamp(material.base_color.value.z + primitive.neural_irradiance.z * 0.025f * neural_warmth,
                 0.0f, 1.0f);
  return material;
}

} // namespace aster
