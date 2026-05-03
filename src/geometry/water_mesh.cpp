// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/geometry/water_mesh.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace {

constexpr float kPi = 3.14159265358979323846f;
constexpr float kGravity = 9.807f;

float smoothstep(const float edge0, const float edge1, const float value) {
  const float t = aster::clamp((value - edge0) / std::max(edge1 - edge0, 0.000001f), 0.0f, 1.0f);
  return t * t * (3.0f - 2.0f * t);
}

float hash1(const float value) {
  const float x = std::sin(value * 127.1f + 19.19f) * 43758.5453123f;
  return x - std::floor(x);
}

aster::Vec2 rotate(const aster::Vec2 value, const float radians) {
  const float c = std::cos(radians);
  const float s = std::sin(radians);
  return {value.x * c - value.y * s, value.x * s + value.y * c};
}

float contourScale(const float theta, const float irregularity) {
  if (irregularity <= 0.0f) {
    return 1.0f;
  }
  const float profile = std::sin(theta * 1.7f + 0.42f) * 0.46f +
                        std::sin(theta * 2.9f - 1.16f) * 0.32f +
                        std::sin(theta * 5.3f + 2.06f) * 0.22f;
  return std::max(0.55f, 1.0f + profile * irregularity);
}

aster::Vec2 safeWindDirection(const aster::WaterWaveSpectrum &spectrum) {
  aster::Vec2 wind = aster::normalize(spectrum.wind_direction);
  if (aster::length(wind) <= 0.0001f) {
    wind = {1.0f, 0.0f};
  }
  return wind;
}

aster::WaterWaveSpectrum resolvedSpectrum(aster::WaterWaveSpectrum spectrum, const float radius_x,
                                          const float radius_z) {
  spectrum.component_count = std::max(spectrum.component_count, 1);
  spectrum.amplitude = std::max(spectrum.amplitude, 0.0f);
  spectrum.min_wavelength = std::max(spectrum.min_wavelength, 0.001f);
  spectrum.dominant_wavelength =
      spectrum.dominant_wavelength > 0.0f
          ? spectrum.dominant_wavelength
          : std::max(std::min(radius_x, radius_z) * 0.72f, spectrum.min_wavelength * 2.0f);
  spectrum.wind_speed = std::max(spectrum.wind_speed, 0.0f);
  spectrum.choppiness = std::max(spectrum.choppiness, 0.0f);
  spectrum.directional_spread = std::max(spectrum.directional_spread, 0.0f);
  spectrum.counter_wave_strength = std::clamp(spectrum.counter_wave_strength, 0.0f, 1.0f);
  return spectrum;
}

struct WaveComponent {
  aster::Vec2 direction{};
  float wave_number = 1.0f;
  float frequency = 1.0f;
  float amplitude = 0.0f;
  float phase = 0.0f;
};

WaveComponent componentAt(const aster::WaterWaveSpectrum &spectrum, const int component) {
  const int count = std::max(spectrum.component_count, 1);
  const float fill = (static_cast<float>(component) + 0.5f) / static_cast<float>(count);
  const float spread_jitter = hash1(spectrum.seed + static_cast<float>(component) * 13.17f) - 0.5f;
  const float back_jitter = hash1(spectrum.seed + static_cast<float>(component) * 7.91f);
  const float octave = std::pow(2.0f, (0.5f - fill) * 2.2f);
  const float wavelength = std::max(spectrum.dominant_wavelength * octave, spectrum.min_wavelength);
  const float wave_number = kPi * 2.0f / wavelength;
  const float wind_length =
      std::max(spectrum.wind_speed * spectrum.wind_speed / kGravity, spectrum.min_wavelength);
  const float swell =
      std::exp(-1.0f / std::max(wave_number * wind_length * wave_number * wind_length, 0.0001f));
  const float band =
      std::exp(-std::pow(std::log(wavelength / spectrum.dominant_wavelength), 2.0f) * 1.35f);
  const float counter = back_jitter < spectrum.counter_wave_strength ? kPi : 0.0f;
  const float spread = spread_jitter * spectrum.directional_spread + counter;
  const aster::Vec2 direction = rotate(safeWindDirection(spectrum), spread);
  const float alignment =
      std::pow(std::max(aster::dot(direction, safeWindDirection(spectrum)), 0.0f), 2.0f);
  const float amplitude = spectrum.amplitude * band * (0.28f + alignment * 0.72f) * swell /
                          std::sqrt(static_cast<float>(component + 1));
  const float phase = hash1(spectrum.seed + static_cast<float>(component) * 23.73f) * kPi * 2.0f;
  return {direction, wave_number, std::sqrt(kGravity * wave_number), amplitude, phase};
}

void accumulateNormal(aster::CpuMesh &mesh, const std::uint32_t a, const std::uint32_t b,
                      const std::uint32_t c) {
  const aster::Vec3 normal =
      aster::normalize(aster::cross(mesh.vertices[b].position - mesh.vertices[a].position,
                                    mesh.vertices[c].position - mesh.vertices[a].position));
  mesh.vertices[a].normal = mesh.vertices[a].normal + normal;
  mesh.vertices[b].normal = mesh.vertices[b].normal + normal;
  mesh.vertices[c].normal = mesh.vertices[c].normal + normal;
}

void finalizeNormals(aster::CpuMesh &mesh) {
  for (aster::Vertex &vertex : mesh.vertices) {
    vertex.normal = aster::normalize(vertex.normal);
    if (aster::length(vertex.normal) <= 0.0001f) {
      vertex.normal = {0.0f, 1.0f, 0.0f};
    }
  }
}

} // namespace

namespace aster {

Vec3 spectralWaterDisplacement(const Vec2 position, const WaterWaveSpectrum &input_spectrum,
                               const float radial_mask) {
  WaterWaveSpectrum spectrum = input_spectrum;
  spectrum.component_count = std::max(spectrum.component_count, 1);
  spectrum.amplitude = std::max(spectrum.amplitude, 0.0f);
  spectrum.dominant_wavelength =
      std::max(spectrum.dominant_wavelength, std::max(spectrum.min_wavelength, 0.001f));
  const float mask = std::clamp(radial_mask, 0.0f, 1.0f);
  Vec3 displacement{};
  for (int i = 0; i < spectrum.component_count; ++i) {
    const WaveComponent wave = componentAt(spectrum, i);
    const float theta =
        (position.x * wave.direction.x + position.y * wave.direction.y) * wave.wave_number +
        wave.phase;
    const float amplitude = wave.amplitude * mask;
    displacement.y += std::sin(theta) * amplitude;
    const float chop = std::cos(theta) * amplitude * spectrum.choppiness;
    displacement.x += wave.direction.x * chop;
    displacement.z += wave.direction.y * chop;
  }
  return displacement;
}

Vec3 spectralWaterNormal(const Vec2 position, const WaterWaveSpectrum &input_spectrum,
                         const float radial_mask) {
  WaterWaveSpectrum spectrum = input_spectrum;
  spectrum.component_count = std::max(spectrum.component_count, 1);
  spectrum.amplitude = std::max(spectrum.amplitude, 0.0f);
  spectrum.dominant_wavelength =
      std::max(spectrum.dominant_wavelength, std::max(spectrum.min_wavelength, 0.001f));
  const float mask = std::clamp(radial_mask, 0.0f, 1.0f);
  float dhdx = 0.0f;
  float dhdz = 0.0f;
  for (int i = 0; i < spectrum.component_count; ++i) {
    const WaveComponent wave = componentAt(spectrum, i);
    const float theta =
        (position.x * wave.direction.x + position.y * wave.direction.y) * wave.wave_number +
        wave.phase;
    const float slope = std::cos(theta) * wave.amplitude * wave.wave_number * mask;
    dhdx += slope * wave.direction.x;
    dhdz += slope * wave.direction.y;
  }
  return normalize({-dhdx, 1.0f, -dhdz});
}

CpuMesh makeEllipticalWaterMesh(EllipticalWaterMeshSpec spec) {
  if (spec.radial_segments < 12 || spec.rings < 1 || spec.radius_x <= 0.0f ||
      spec.radius_z <= 0.0f || spec.edge_softness < 0.0f || spec.edge_irregularity < 0.0f ||
      spec.shoreline_inset < 0.0f) {
    throw std::invalid_argument("Elliptical water mesh requires positive radii and enough rings.");
  }

  const float radius_x = std::max(spec.radius_x - spec.shoreline_inset, spec.radius_x * 0.12f);
  const float radius_z = std::max(spec.radius_z - spec.shoreline_inset, spec.radius_z * 0.12f);
  spec.spectrum = resolvedSpectrum(spec.spectrum, radius_x, radius_z);

  CpuMesh mesh;
  const int ring_vertices = spec.radial_segments + 1;
  mesh.vertices.reserve(static_cast<std::size_t>(ring_vertices * (spec.rings + 1)));
  mesh.indices.reserve(static_cast<std::size_t>(spec.radial_segments * spec.rings * 6));

  for (int ring = 0; ring <= spec.rings; ++ring) {
    const float r = static_cast<float>(ring) / static_cast<float>(spec.rings);
    const float edge = 1.0f - spec.edge_softness * smoothstep(0.70f, 1.0f, r);
    const float wave_mask =
        smoothstep(0.08f, 0.28f, r) * (1.0f - smoothstep(0.84f, 1.0f, r) * 0.72f);
    for (int segment = 0; segment <= spec.radial_segments; ++segment) {
      const float u = static_cast<float>(segment) / static_cast<float>(spec.radial_segments);
      const float theta = u * kPi * 2.0f;
      const float shape = contourScale(theta + 1.21f, spec.edge_irregularity);
      const float x = std::cos(theta) * radius_x * r * edge * shape;
      const float z = std::sin(theta) * radius_z * r * edge * shape;
      const Vec3 displacement = spectralWaterDisplacement({x, z}, spec.spectrum, wave_mask);
      mesh.vertices.push_back({{x + displacement.x, displacement.y, z + displacement.z},
                               spectralWaterNormal({x, z}, spec.spectrum, wave_mask),
                               {u, r}});
    }
  }

  for (int ring = 0; ring < spec.rings; ++ring) {
    for (int segment = 0; segment < spec.radial_segments; ++segment) {
      const std::uint32_t a = static_cast<std::uint32_t>(ring * ring_vertices + segment);
      const std::uint32_t b = static_cast<std::uint32_t>((ring + 1) * ring_vertices + segment);
      const std::uint32_t c = static_cast<std::uint32_t>((ring + 1) * ring_vertices + segment + 1);
      const std::uint32_t d = static_cast<std::uint32_t>(ring * ring_vertices + segment + 1);
      mesh.indices.insert(mesh.indices.end(), {a, d, b, d, c, b});
      accumulateNormal(mesh, a, d, b);
      accumulateNormal(mesh, d, c, b);
    }
  }

  finalizeNormals(mesh);
  return mesh;
}

} // namespace aster
