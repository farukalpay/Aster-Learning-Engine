// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/geometry/brush_level_mesh.hpp"

#include <detail_noise.hpp>
#include <solid_ops.hpp>

#include <AsterTopology/Core/Mesh/TriMesh_ArrayKernelT.hh>

#include <aster_linear/gtc/matrix_transform.hpp>
#include <aster_linear/gtc/type_ptr.hpp>
#include <aster_linear/linear.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <unordered_map>

namespace aster {
namespace {

constexpr solid_ops::volume_t kAirVolume = 0;
constexpr solid_ops::volume_t kSolidVolume = 1;

struct AsterTopologyTraits : public AsterTopology::DefaultTraits {
  VertexAttributes(AsterTopology::Attributes::Normal | AsterTopology::Attributes::Status);
  FaceAttributes(AsterTopology::Attributes::Normal | AsterTopology::Attributes::Status);
  EdgeAttributes(AsterTopology::Attributes::Status);
  HalfedgeAttributes(AsterTopology::Attributes::PrevHalfedge | AsterTopology::Attributes::Status);
};

using AsterTopologyTriMesh = AsterTopology::TriMesh_ArrayKernelT<AsterTopologyTraits>;

struct VertexKey {
  std::array<const void *, 3> faces{};

  bool operator==(const VertexKey &other) const {
    return faces == other.faces;
  }
};

struct VertexKeyHash {
  std::size_t operator()(const VertexKey &key) const {
    std::size_t hash = 0;
    for (const void *face : key.faces) {
      hash ^= std::hash<const void *>{}(face) + 0x9e3779b9u + (hash << 6u) + (hash >> 2u);
    }
    return hash;
  }
};

Vec3 dominantProjectionUv(const Vec3 position, const Vec3 normal, const float uv_scale) {
  const float scale = uv_scale <= 0.0001f ? 1.0f : uv_scale;
  const Vec3 abs_normal{std::abs(normal.x), std::abs(normal.y), std::abs(normal.z)};
  if (abs_normal.y >= abs_normal.x && abs_normal.y >= abs_normal.z) {
    return {position.x / scale, position.z / scale, 0.0f};
  }
  if (abs_normal.x >= abs_normal.z) {
    return {position.z / scale, position.y / scale, 0.0f};
  }
  return {position.x / scale, position.y / scale, 0.0f};
}

Vec2 projectUv(const Vec3 position, const Vec3 normal, const float uv_scale) {
  const Vec3 uv = dominantProjectionUv(position, normal, uv_scale);
  return {uv.x, uv.y};
}

aster_linear::vec3 toLinear(const Vec3 value) {
  return {value.x, value.y, value.z};
}

Vec3 fromLinear(const aster_linear::vec3 value) {
  return {value.x, value.y, value.z};
}

solid_ops::plane_t makePlane(const aster_linear::vec3 point, const aster_linear::vec3 normal) {
  return {aster_linear::normalize(normal),
          -aster_linear::dot(point, aster_linear::normalize(normal))};
}

aster_linear::vec3 projectPoint(const aster_linear::vec3 point, const solid_ops::plane_t &plane) {
  const float distance = aster_linear::dot(plane.normal, point) + plane.offset;
  return point - distance * plane.normal;
}

solid_ops::plane_t transformPlane(const solid_ops::plane_t &plane,
                                  const aster_linear::mat4 &transform) {
  const aster_linear::vec3 origin = aster_linear::vec3(
      transform * aster_linear::vec4(projectPoint({0.0f, 0.0f, 0.0f}, plane), 1.0f));
  const aster_linear::vec3 normal = aster_linear::normalize(
      aster_linear::vec3(aster_linear::transpose(aster_linear::inverse(transform)) *
                         aster_linear::vec4(plane.normal, 0.0f)));
  return makePlane(origin, normal);
}

aster_linear::mat4 composeBoxTransform(const BrushBox &brush) {
  aster_linear::mat4 transform(1.0f);
  transform = aster_linear::translate(transform, toLinear(brush.center));
  transform =
      aster_linear::rotate(transform, brush.rotation.y, aster_linear::vec3(0.0f, 1.0f, 0.0f));
  transform =
      aster_linear::rotate(transform, brush.rotation.x, aster_linear::vec3(1.0f, 0.0f, 0.0f));
  transform =
      aster_linear::rotate(transform, brush.rotation.z, aster_linear::vec3(0.0f, 0.0f, 1.0f));
  transform = aster_linear::scale(transform, toLinear(brush.half_extents));
  return transform;
}

std::vector<solid_ops::plane_t> unitBoxPlanes(const float chamfer) {
  std::vector<solid_ops::plane_t> planes = {
      makePlane({1.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}),
      makePlane({-1.0f, 0.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}),
      makePlane({0.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}),
      makePlane({0.0f, -1.0f, 0.0f}, {0.0f, -1.0f, 0.0f}),
      makePlane({0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}),
      makePlane({0.0f, 0.0f, -1.0f}, {0.0f, 0.0f, -1.0f}),
  };

  if (chamfer <= 0.0001f) {
    return planes;
  }

  const float cut = std::clamp(chamfer, 0.0f, 0.45f);
  const float inset = 1.0f - cut;
  const int signs[2] = {-1, 1};
  for (const int sx : signs) {
    for (const int sy : signs) {
      planes.push_back(
          makePlane({static_cast<float>(sx) * inset, static_cast<float>(sy) * inset, 0.0f},
                    aster_linear::vec3(static_cast<float>(sx), static_cast<float>(sy), 0.0f)));
      planes.push_back(
          makePlane({static_cast<float>(sx) * inset, 0.0f, static_cast<float>(sy) * inset},
                    aster_linear::vec3(static_cast<float>(sx), 0.0f, static_cast<float>(sy))));
      planes.push_back(
          makePlane({0.0f, static_cast<float>(sx) * inset, static_cast<float>(sy) * inset},
                    aster_linear::vec3(0.0f, static_cast<float>(sx), static_cast<float>(sy))));
    }
  }
  return planes;
}

void configureBrush(solid_ops::brush_t &brush, const BrushBox &definition) {
  const aster_linear::mat4 transform = composeBoxTransform(definition);
  std::vector<solid_ops::plane_t> planes = unitBoxPlanes(definition.chamfer);
  for (solid_ops::plane_t &plane : planes) {
    plane = transformPlane(plane, transform);
  }

  brush.set_planes(planes);
  brush.set_time(definition.time);
  brush.set_volume_operation(solid_ops::make_fill_operation(
      definition.volume == BrushVolume::Solid ? kSolidVolume : kAirVolume));
}

VertexKey vertexKey(const solid_ops::vertex_t &vertex) {
  VertexKey key{{vertex.faces[0], vertex.faces[1], vertex.faces[2]}};
  std::sort(key.faces.begin(), key.faces.end());
  return key;
}

struct TopologyMesh {
  std::vector<Vec3> vertices;
  std::vector<std::uint32_t> indices;
};

TopologyMesh buildTopologyMesh(const std::vector<BrushBox> &brushes) {
  solid_ops::world_t world;
  world.set_void_volume(kAirVolume);

  std::vector<solid_ops::brush_t *> created;
  created.reserve(brushes.size());
  for (const BrushBox &brush_definition : brushes) {
    solid_ops::brush_t *brush = world.add();
    configureBrush(*brush, brush_definition);
    created.push_back(brush);
  }

  world.rebuild();

  TopologyMesh topology;
  std::unordered_map<VertexKey, std::uint32_t, VertexKeyHash> vertex_lookup;

  for (solid_ops::brush_t *brush = world.first(); brush != nullptr; brush = world.next(brush)) {
    for (const solid_ops::face_t &face : brush->get_faces()) {
      for (const solid_ops::fragment_t &fragment : face.fragments) {
        if (fragment.front_volume == fragment.back_volume) {
          continue;
        }

        const bool flip_face = fragment.back_volume == kAirVolume;
        const std::vector<solid_ops::triangle_t> triangles = solid_ops::triangulate(fragment);
        for (const solid_ops::triangle_t &triangle : triangles) {
          const int order[3] = {triangle.i, flip_face ? triangle.k : triangle.j,
                                flip_face ? triangle.j : triangle.k};
          for (const int vertex_index : order) {
            const solid_ops::vertex_t &source =
                fragment.vertices[static_cast<std::size_t>(vertex_index)];
            const VertexKey key = vertexKey(source);
            const auto existing = vertex_lookup.find(key);
            if (existing != vertex_lookup.end()) {
              topology.indices.push_back(existing->second);
              continue;
            }

            const std::uint32_t new_index = static_cast<std::uint32_t>(topology.vertices.size());
            topology.vertices.push_back(fromLinear(source.position));
            topology.indices.push_back(new_index);
            vertex_lookup.emplace(key, new_index);
          }
        }
      }
    }
  }

  if (topology.indices.empty()) {
    throw std::runtime_error("Brush world produced no renderable geometry.");
  }
  return topology;
}

AsterTopologyTriMesh makeAsterTopology(const TopologyMesh &topology) {
  AsterTopologyTriMesh mesh;
  mesh.request_face_normals();
  mesh.request_vertex_normals();

  std::vector<AsterTopologyTriMesh::VertexHandle> handles;
  handles.reserve(topology.vertices.size());
  for (const Vec3 &position : topology.vertices) {
    handles.push_back(
        mesh.add_vertex(AsterTopologyTriMesh::Point(position.x, position.y, position.z)));
  }

  for (std::size_t i = 0; i < topology.indices.size(); i += 3u) {
    std::vector<AsterTopologyTriMesh::VertexHandle> face{
        handles[topology.indices[i + 0u]],
        handles[topology.indices[i + 1u]],
        handles[topology.indices[i + 2u]],
    };
    mesh.add_face(face);
  }

  mesh.update_face_normals();
  mesh.update_vertex_normals();
  return mesh;
}

void softenFeatureEdges(AsterTopologyTriMesh &mesh, const BrushLevelMeshOptions &options) {
  if (options.edge_softening <= 0.0001f) {
    return;
  }

  (void)mesh.find_feature_edges(AsterTopology::deg_to_rad(42.0f));
  std::vector<AsterTopologyTriMesh::Point> updated(mesh.n_vertices());

  for (auto vertex_it = mesh.vertices_begin(); vertex_it != mesh.vertices_end(); ++vertex_it) {
    const AsterTopologyTriMesh::VertexHandle vertex = *vertex_it;
    AsterTopologyTriMesh::Normal feature_sum(0.0f, 0.0f, 0.0f);
    int feature_edges = 0;

    for (auto edge_it = mesh.cve_begin(vertex); edge_it != mesh.cve_end(vertex); ++edge_it) {
      if (!mesh.status(*edge_it).feature()) {
        continue;
      }
      ++feature_edges;
      const AsterTopologyTriMesh::HalfedgeHandle halfedge = mesh.halfedge_handle(*edge_it, 0);
      const AsterTopologyTriMesh::HalfedgeHandle opposite = mesh.opposite_halfedge_handle(halfedge);
      if (mesh.is_valid_handle(halfedge) && !mesh.is_boundary(halfedge)) {
        feature_sum += mesh.normal(mesh.face_handle(halfedge));
      }
      if (mesh.is_valid_handle(opposite) && !mesh.is_boundary(opposite)) {
        feature_sum += mesh.normal(mesh.face_handle(opposite));
      }
    }

    AsterTopologyTriMesh::Point position = mesh.point(vertex);
    if (feature_edges >= 2 && feature_sum.sqrnorm() > 0.000001f) {
      const AsterTopologyTriMesh::Normal inward = AsterTopology::normalize(feature_sum);
      position -= inward * options.edge_softening;
    }
    updated[static_cast<std::size_t>(vertex.idx())] = position;
  }

  for (auto vertex_it = mesh.vertices_begin(); vertex_it != mesh.vertices_end(); ++vertex_it) {
    mesh.set_point(*vertex_it, updated[static_cast<std::size_t>(vertex_it->idx())]);
  }
  mesh.update_face_normals();
  mesh.update_vertex_normals();
}

void applyNoise(AsterTopologyTriMesh &mesh, const BrushLevelMeshOptions &options) {
  if (options.noise_strength <= 0.0001f) {
    return;
  }

  AsterDetailNoise noise(options.noise_seed);
  noise.SetNoiseType(AsterDetailNoise::NoiseType_OpenSimplex2);
  noise.SetFractalType(AsterDetailNoise::FractalType_Ridged);
  noise.SetFractalOctaves(3);
  noise.SetFrequency(std::max(options.noise_frequency, 0.001f));

  for (auto vertex_it = mesh.vertices_begin(); vertex_it != mesh.vertices_end(); ++vertex_it) {
    const AsterTopologyTriMesh::VertexHandle vertex = *vertex_it;
    const AsterTopologyTriMesh::Point position = mesh.point(vertex);
    const AsterTopologyTriMesh::Normal normal = mesh.normal(vertex);
    const float sample = noise.GetNoise(position[0], position[1], position[2]);
    mesh.set_point(vertex, position + normal * (sample * options.noise_strength));
  }

  mesh.update_face_normals();
  mesh.update_vertex_normals();
}

CpuMesh expandRenderMesh(const AsterTopologyTriMesh &mesh, const BrushLevelMeshOptions &options) {
  CpuMesh out;
  out.vertices.reserve(mesh.n_faces() * 3u);
  out.indices.reserve(mesh.n_faces() * 3u);

  for (auto face_it = mesh.faces_begin(); face_it != mesh.faces_end(); ++face_it) {
    std::array<Vec3, 3> positions{};
    int index = 0;
    for (auto vertex_it = mesh.cfv_begin(*face_it); vertex_it != mesh.cfv_end(*face_it);
         ++vertex_it) {
      const AsterTopologyTriMesh::Point point = mesh.point(*vertex_it);
      positions[static_cast<std::size_t>(index++)] = {point[0], point[1], point[2]};
    }
    if (index != 3) {
      continue;
    }

    const Vec3 normal = normalize(cross(positions[1] - positions[0], positions[2] - positions[0]));
    const std::uint32_t base = static_cast<std::uint32_t>(out.vertices.size());
    out.vertices.push_back(
        {positions[0], normal, projectUv(positions[0], normal, options.uv_scale)});
    out.vertices.push_back(
        {positions[1], normal, projectUv(positions[1], normal, options.uv_scale)});
    out.vertices.push_back(
        {positions[2], normal, projectUv(positions[2], normal, options.uv_scale)});
    out.indices.insert(out.indices.end(), {base, base + 1u, base + 2u});
  }

  if (out.indices.empty()) {
    throw std::runtime_error("Expanded brush mesh contains no triangles.");
  }
  return out;
}

} // namespace

CpuMesh buildBrushLevelMesh(const std::vector<BrushBox> &brushes,
                            const BrushLevelMeshOptions options) {
  if (brushes.empty()) {
    throw std::invalid_argument("Brush mesh build requires at least one brush.");
  }

  const TopologyMesh topology = buildTopologyMesh(brushes);
  AsterTopologyTriMesh mesh = makeAsterTopology(topology);
  softenFeatureEdges(mesh, options);
  applyNoise(mesh, options);
  return prepareMeshForRendering(expandRenderMesh(mesh, options), options.mesh_options);
}

} // namespace aster
