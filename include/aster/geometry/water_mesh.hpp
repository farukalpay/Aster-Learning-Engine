// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/math/vec.hpp"
#include "aster/render/mesh.hpp"

namespace aster {

struct WaterWaveSpectrum {
  Vec2 wind_direction{1.0f, 0.35f};
  float wind_speed = 7.0f;
  float amplitude = 0.018f;
  float dominant_wavelength = 0.0f;
  float min_wavelength = 0.12f;
  float choppiness = 0.12f;
  float directional_spread = radians(58.0f);
  float counter_wave_strength = 0.16f;
  int component_count = 18;
  float seed = 0.0f;
};

struct EllipticalWaterMeshSpec {
  int radial_segments = 96;
  int rings = 10;
  float radius_x = 1.0f;
  float radius_z = 0.65f;
  float edge_softness = 0.045f;
  float edge_irregularity = 0.0f;
  float shoreline_inset = 0.0f;
  WaterWaveSpectrum spectrum{};
};

[[nodiscard]] Vec3 spectralWaterDisplacement(Vec2 position, const WaterWaveSpectrum &spectrum,
                                             float radial_mask = 1.0f);
[[nodiscard]] Vec3 spectralWaterNormal(Vec2 position, const WaterWaveSpectrum &spectrum,
                                       float radial_mask = 1.0f);
[[nodiscard]] CpuMesh makeEllipticalWaterMesh(EllipticalWaterMeshSpec spec = {});

} // namespace aster
