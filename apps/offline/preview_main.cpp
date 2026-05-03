// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/math/color.hpp"
#include "aster/render/camera.hpp"
#include "aster/render/mesh.hpp"
#include "aster/render/render_device.hpp"
#include "aster/scene/scene.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

namespace {

struct Ray {
  aster::Vec3 origin{};
  aster::Vec3 direction{};
};

struct Hit {
  bool valid = false;
  float distance = std::numeric_limits<float>::max();
  aster::Vec3 position{};
  aster::Vec3 normal{};
  aster::Material material{};
};

struct TraceTriangle {
  aster::Vec3 a{};
  aster::Vec3 b{};
  aster::Vec3 c{};
  aster::Vec3 normal{};
};

struct PreparedObject {
  aster::RenderObject object{};
  std::vector<TraceTriangle> triangles;
  aster::Vec3 bounds_min{};
  aster::Vec3 bounds_max{};
};

aster::Vec3 transformPoint(const aster::Mat4 &matrix, const aster::Vec3 point) {
  return {
      matrix.m[0] * point.x + matrix.m[4] * point.y + matrix.m[8] * point.z + matrix.m[12],
      matrix.m[1] * point.x + matrix.m[5] * point.y + matrix.m[9] * point.z + matrix.m[13],
      matrix.m[2] * point.x + matrix.m[6] * point.y + matrix.m[10] * point.z + matrix.m[14],
  };
}

void expandBounds(PreparedObject &out, const aster::Vec3 point) {
  out.bounds_min.x = std::min(out.bounds_min.x, point.x);
  out.bounds_min.y = std::min(out.bounds_min.y, point.y);
  out.bounds_min.z = std::min(out.bounds_min.z, point.z);
  out.bounds_max.x = std::max(out.bounds_max.x, point.x);
  out.bounds_max.y = std::max(out.bounds_max.y, point.y);
  out.bounds_max.z = std::max(out.bounds_max.z, point.z);
}

void prepareTriangleMesh(PreparedObject &out, const aster::CpuMesh &mesh) {
  const aster::Mat4 model = out.object.transform.matrix();
  out.bounds_min = {std::numeric_limits<float>::max(), std::numeric_limits<float>::max(),
                    std::numeric_limits<float>::max()};
  out.bounds_max = {-std::numeric_limits<float>::max(), -std::numeric_limits<float>::max(),
                    -std::numeric_limits<float>::max()};
  out.triangles.reserve(mesh.indices.size() / 3u);

  for (std::size_t i = 0; i < mesh.indices.size(); i += 3u) {
    const aster::Vec3 a = transformPoint(model, mesh.vertices[mesh.indices[i + 0u]].position);
    const aster::Vec3 b = transformPoint(model, mesh.vertices[mesh.indices[i + 1u]].position);
    const aster::Vec3 c = transformPoint(model, mesh.vertices[mesh.indices[i + 2u]].position);
    const aster::Vec3 normal = aster::normalize(aster::cross(b - a, c - a));
    if (aster::length(normal) <= 0.0f) {
      continue;
    }
    out.triangles.push_back({a, b, c, normal});
    expandBounds(out, a);
    expandBounds(out, b);
    expandBounds(out, c);
  }
}

std::vector<PreparedObject> prepareScene(const aster::Scene &scene) {
  std::vector<PreparedObject> prepared;
  prepared.reserve(scene.objects().size());
  static const aster::CpuMesh box = aster::makeBox();

  for (const aster::RenderObject &object : scene.objects()) {
    PreparedObject out;
    out.object = object;
    if (object.custom_mesh != nullptr) {
      prepareTriangleMesh(out, *object.custom_mesh);
    } else if (object.primitive == aster::MeshPrimitive::Box) {
      prepareTriangleMesh(out, box);
    }
    prepared.push_back(std::move(out));
  }
  return prepared;
}

float representativeSphereRadius(const aster::Transform &transform) {
  return (std::abs(transform.scale.x) + std::abs(transform.scale.y) + std::abs(transform.scale.z)) /
         3.0f;
}

bool intersectSphere(const Ray &ray, const PreparedObject &prepared, Hit &hit) {
  const aster::RenderObject &object = prepared.object;
  const aster::Vec3 center = object.transform.position;
  const float radius = representativeSphereRadius(object.transform);
  const aster::Vec3 oc = ray.origin - center;
  const float a = aster::dot(ray.direction, ray.direction);
  const float b = 2.0f * aster::dot(oc, ray.direction);
  const float c = aster::dot(oc, oc) - radius * radius;
  const float discriminant = b * b - 4.0f * a * c;
  if (discriminant < 0.0f) {
    return false;
  }
  const float root = std::sqrt(discriminant);
  const float near_t = (-b - root) / (2.0f * a);
  const float far_t = (-b + root) / (2.0f * a);
  const float t = near_t > 0.001f ? near_t : far_t;
  if (t <= 0.001f || t >= hit.distance) {
    return false;
  }
  hit.valid = true;
  hit.distance = t;
  hit.position = ray.origin + ray.direction * t;
  hit.normal = aster::normalize(hit.position - center);
  hit.material = object.material;
  return true;
}

bool intersectGround(const Ray &ray, const aster::RenderObject &object, Hit &hit) {
  if (std::abs(ray.direction.y) <= 0.0001f) {
    return false;
  }
  const float t = -ray.origin.y / ray.direction.y;
  if (t <= 0.001f || t >= hit.distance) {
    return false;
  }
  const aster::Vec3 position = ray.origin + ray.direction * t;
  if (std::abs(position.x) > 6.0f || std::abs(position.z) > 6.0f) {
    return false;
  }
  hit.valid = true;
  hit.distance = t;
  hit.position = position;
  hit.normal = {0.0f, 1.0f, 0.0f};
  hit.material = object.material;
  return true;
}

float axisValue(const aster::Vec3 value, const int axis) {
  if (axis == 0) {
    return value.x;
  }
  if (axis == 1) {
    return value.y;
  }
  return value.z;
}

bool intersectBounds(const Ray &ray, const aster::Vec3 bounds_min, const aster::Vec3 bounds_max,
                     const float max_distance) {
  float t_min = 0.001f;
  float t_max = max_distance;
  for (int axis = 0; axis < 3; ++axis) {
    const float origin = axisValue(ray.origin, axis);
    const float direction = axisValue(ray.direction, axis);
    const float low = axisValue(bounds_min, axis);
    const float high = axisValue(bounds_max, axis);
    if (std::abs(direction) <= 0.000001f) {
      if (origin < low || origin > high) {
        return false;
      }
      continue;
    }
    float near_t = (low - origin) / direction;
    float far_t = (high - origin) / direction;
    if (near_t > far_t) {
      std::swap(near_t, far_t);
    }
    t_min = std::max(t_min, near_t);
    t_max = std::min(t_max, far_t);
    if (t_min > t_max) {
      return false;
    }
  }
  return true;
}

bool intersectTriangle(const Ray &ray, const TraceTriangle &triangle, float &t,
                       aster::Vec3 &normal) {
  constexpr float kEpsilon = 0.000001f;
  const aster::Vec3 edge1 = triangle.b - triangle.a;
  const aster::Vec3 edge2 = triangle.c - triangle.a;
  const aster::Vec3 p = aster::cross(ray.direction, edge2);
  const float det = aster::dot(edge1, p);
  if (std::abs(det) <= kEpsilon) {
    return false;
  }

  const float inv_det = 1.0f / det;
  const aster::Vec3 s = ray.origin - triangle.a;
  const float u = inv_det * aster::dot(s, p);
  if (u < 0.0f || u > 1.0f) {
    return false;
  }

  const aster::Vec3 q = aster::cross(s, edge1);
  const float v = inv_det * aster::dot(ray.direction, q);
  if (v < 0.0f || u + v > 1.0f) {
    return false;
  }

  t = inv_det * aster::dot(edge2, q);
  if (t <= 0.001f) {
    return false;
  }
  normal =
      aster::dot(triangle.normal, ray.direction) > 0.0f ? triangle.normal * -1.0f : triangle.normal;
  return true;
}

bool intersectPreparedMesh(const Ray &ray, const PreparedObject &prepared, Hit &hit) {
  if (prepared.triangles.empty() ||
      !intersectBounds(ray, prepared.bounds_min, prepared.bounds_max, hit.distance)) {
    return false;
  }

  bool found = false;
  for (const TraceTriangle &triangle : prepared.triangles) {
    float t = 0.0f;
    aster::Vec3 normal{};
    if (!intersectTriangle(ray, triangle, t, normal) || t >= hit.distance) {
      continue;
    }
    hit.valid = true;
    hit.distance = t;
    hit.position = ray.origin + ray.direction * t;
    hit.normal = normal;
    hit.material = prepared.object.material;
    found = true;
  }
  return found;
}

Hit trace(const Ray &ray, const std::vector<PreparedObject> &scene) {
  Hit closest;
  for (const PreparedObject &prepared : scene) {
    if (!prepared.triangles.empty()) {
      intersectPreparedMesh(ray, prepared, closest);
      continue;
    }
    const aster::RenderObject &object = prepared.object;
    switch (object.primitive) {
    case aster::MeshPrimitive::Box:
      break;
    case aster::MeshPrimitive::Sphere:
    case aster::MeshPrimitive::Rock:
    case aster::MeshPrimitive::Crystal:
    case aster::MeshPrimitive::RuinBlock:
    case aster::MeshPrimitive::Pillar:
      intersectSphere(ray, prepared, closest);
      break;
    case aster::MeshPrimitive::Plane:
      intersectGround(ray, object, closest);
      break;
    }
  }
  return closest;
}

float fractValue(const float value) {
  return value - std::floor(value);
}

float hashCell(const float x, const float y) {
  return fractValue(std::sin(x * 127.1f + y * 311.7f) * 43758.5453f);
}

aster::Vec3 mixColor(const aster::Vec3 a, const aster::Vec3 b, const float t) {
  return a * (1.0f - t) + b * t;
}

aster::Vec2 patternCoordinatesAt(const aster::Vec3 position, const aster::Vec3 normal) {
  const aster::Vec3 abs_normal{std::abs(normal.x), std::abs(normal.y), std::abs(normal.z)};
  if (abs_normal.y >= abs_normal.x && abs_normal.y >= abs_normal.z) {
    return {position.x, position.z};
  }
  if (abs_normal.x >= abs_normal.z) {
    return {position.z, position.y};
  }
  return {position.x, position.y};
}

float courseCellMaskAt(aster::Vec2 uv, const aster::Material &material) {
  uv.x *= std::max(material.pattern_scale.x, 0.001f);
  uv.y *= std::max(material.pattern_scale.y, 0.001f);
  const float row = std::floor(uv.y);
  uv.x += std::fmod(row, 2.0f) * 0.5f;
  const aster::Vec2 cell{fractValue(uv.x), fractValue(uv.y)};
  const float mortar = aster::clamp(material.pattern_mortar, 0.001f, 0.45f);
  const float edge_distance =
      std::min(std::min(cell.x, 1.0f - cell.x), std::min(cell.y, 1.0f - cell.y));
  return aster::clamp((edge_distance - mortar * 0.45f) / std::max(mortar * 0.55f, 0.001f), 0.0f,
                      1.0f);
}

aster::Vec3 previewAlbedo(const Hit &hit) {
  aster::Vec3 color = hit.material.base_color;
  if (hit.material.surface_pattern == aster::SurfacePattern::CourseCells) {
    const aster::Vec2 uv = patternCoordinatesAt(hit.position, hit.normal);
    const float cell = courseCellMaskAt(uv, hit.material);
    const float variation =
        hashCell(std::floor(uv.x * std::max(hit.material.pattern_scale.x, 0.001f)),
                 std::floor(uv.y * std::max(hit.material.pattern_scale.y, 0.001f)));
    const aster::Vec3 brick =
        color * (0.78f + variation * 0.34f) + aster::Vec3{0.035f, 0.024f, 0.014f};
    const aster::Vec3 mortar = color * (0.32f + variation * 0.08f);
    color = mixColor(mortar, brick, cell);
    color = mixColor(color, color * (0.74f + 0.36f * cell),
                     aster::clamp(hit.material.pattern_contrast, 0.0f, 1.0f));
  }
  return color;
}

aster::Vec3 shade(const Hit &hit, const Ray &ray) {
  const std::array<aster::Light, 3> lights = {
      aster::Light{{-3.7f, 5.4f, 2.4f}, {1.0f, 0.86f, 0.68f}, 8.2f},
      aster::Light{{4.4f, 3.1f, -2.7f}, {0.34f, 0.56f, 1.0f}, 4.5f},
      aster::Light{{0.0f, 5.9f, -4.6f}, {1.0f, 0.72f, 0.46f}, 6.0f},
  };

  const aster::Vec3 albedo = previewAlbedo(hit);
  aster::Vec3 color = albedo * 0.08f + hit.material.emission_color * hit.material.emission_strength;
  const aster::Vec3 view = aster::normalize(ray.origin - hit.position);
  for (const aster::Light &light : lights) {
    const aster::Vec3 light_vector = light.position - hit.position;
    const float distance_sq = std::max(aster::dot(light_vector, light_vector), 0.001f);
    const aster::Vec3 light_dir = aster::normalize(light_vector);
    const aster::Vec3 half_vector = aster::normalize(light_dir + view);
    const float n_dot_l = std::max(aster::dot(hit.normal, light_dir), 0.0f);
    const float n_dot_h = std::max(aster::dot(hit.normal, half_vector), 0.0f);
    const float specular_power = 2.0f + (1.0f - hit.material.roughness) * 96.0f;
    const float specular = std::pow(n_dot_h, specular_power) * (0.08f + hit.material.metallic);
    const aster::Vec3 radiance = light.color * (light.intensity / distance_sq);
    color = color + (albedo * n_dot_l + radiance * specular) * radiance;
  }
  return aster::gamma_encode(aster::aces_tonemap(color * 1.2f));
}

aster::Vec3 sky(const Ray &ray) {
  const float t = aster::clamp(ray.direction.y * 0.5f + 0.5f, 0.0f, 1.0f);
  return aster::gamma_encode({0.04f + 0.32f * t, 0.055f + 0.36f * t, 0.075f + 0.42f * t});
}

std::string argumentValue(int argc, char **argv, const std::string &name,
                          const std::string &fallback) {
  for (int i = 1; i + 1 < argc; ++i) {
    if (argv[i] == name) {
      return argv[i + 1];
    }
  }
  return fallback;
}

} // namespace

int main(int argc, char **argv) {
  try {
    const int width = std::stoi(argumentValue(argc, argv, "--width", "960"));
    const int height = std::stoi(argumentValue(argc, argv, "--height", "540"));
    const std::filesystem::path output =
        argumentValue(argc, argv, "--output", "assets/screenshots/aster_preview.ppm");

    aster::Scene scene = aster::Scene::makeShowcase();
    const std::vector<PreparedObject> prepared_scene = prepareScene(scene);
    aster::OrbitCamera camera;
    const aster::Vec3 eye = camera.position();
    const aster::Vec3 forward = aster::normalize(camera.target - eye);
    const aster::Vec3 right = aster::normalize(aster::cross(forward, {0.0f, 1.0f, 0.0f}));
    const aster::Vec3 up = aster::normalize(aster::cross(right, forward));
    const float aspect = static_cast<float>(width) / static_cast<float>(height);
    const float view_height = 2.0f * std::tan(camera.vertical_fov * 0.5f);
    const float view_width = view_height * aspect;

    if (output.has_parent_path()) {
      std::filesystem::create_directories(output.parent_path());
    }
    std::ofstream file(output, std::ios::binary);
    if (!file) {
      throw std::runtime_error("Could not open preview output path.");
    }
    file << "P6\n" << width << ' ' << height << "\n255\n";

    for (int y = 0; y < height; ++y) {
      for (int x = 0; x < width; ++x) {
        const float u = (static_cast<float>(x) + 0.5f) / static_cast<float>(width);
        const float v = (static_cast<float>(y) + 0.5f) / static_cast<float>(height);
        const aster::Vec3 direction = aster::normalize(forward + right * ((u - 0.5f) * view_width) -
                                                       up * ((v - 0.5f) * view_height));
        const Ray ray{eye, direction};
        const Hit hit = trace(ray, prepared_scene);
        const aster::Vec3 color = hit.valid ? shade(hit, ray) : sky(ray);
        const std::uint8_t rgb[3] = {
            static_cast<std::uint8_t>(aster::clamp(color.x, 0.0f, 1.0f) * 255.0f),
            static_cast<std::uint8_t>(aster::clamp(color.y, 0.0f, 1.0f) * 255.0f),
            static_cast<std::uint8_t>(aster::clamp(color.z, 0.0f, 1.0f) * 255.0f),
        };
        file.write(reinterpret_cast<const char *>(rgb), sizeof(rgb));
      }
    }

    std::cout << "Wrote preview: " << output << '\n';
  } catch (const std::exception &error) {
    std::cerr << "Preview render failed: " << error.what() << '\n';
    return 1;
  }
  return 0;
}
