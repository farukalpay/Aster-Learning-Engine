// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#include "aster/asset/asset_io.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <unordered_map>

namespace aster {
namespace {

[[nodiscard]] std::string lowerExtension(const std::filesystem::path &path) {
  std::string extension = path.extension().string();
  std::transform(extension.begin(), extension.end(), extension.begin(), [](const unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  return extension;
}

[[nodiscard]] std::string hex64(const std::uint64_t value) {
  std::ostringstream out;
  out << "0x" << std::hex << std::setfill('0') << std::setw(16) << value;
  return out.str();
}

[[nodiscard]] std::string hashFile(const std::filesystem::path &path) {
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    return {};
  }
  std::uint64_t hash = 1469598103934665603ull;
  std::array<char, 8192> buffer{};
  while (file) {
    file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
    const std::streamsize count = file.gcount();
    for (std::streamsize i = 0; i < count; ++i) {
      hash ^= static_cast<std::uint8_t>(buffer[static_cast<std::size_t>(i)]);
      hash *= 1099511628211ull;
    }
  }
  return hex64(hash);
}

[[nodiscard]] Vec3 axisVector(const AssetMeshAxis axis) {
  switch (axis) {
  case AssetMeshAxis::PositiveX:
    return {1.0f, 0.0f, 0.0f};
  case AssetMeshAxis::PositiveY:
    return {0.0f, 1.0f, 0.0f};
  case AssetMeshAxis::PositiveZ:
    return {0.0f, 0.0f, 1.0f};
  case AssetMeshAxis::NegativeX:
    return {-1.0f, 0.0f, 0.0f};
  case AssetMeshAxis::NegativeY:
    return {0.0f, -1.0f, 0.0f};
  case AssetMeshAxis::NegativeZ:
    return {0.0f, 0.0f, -1.0f};
  }
  return {0.0f, 1.0f, 0.0f};
}

struct AxisBasis {
  Vec3 right{1.0f, 0.0f, 0.0f};
  Vec3 up{0.0f, 1.0f, 0.0f};
  Vec3 forward{0.0f, 0.0f, 1.0f};
};

[[nodiscard]] AxisBasis makeAxisBasis(const AssetMeshAxis up_axis,
                                      const AssetMeshAxis forward_axis) {
  AxisBasis basis;
  basis.up = normalizeOr(axisVector(up_axis), {0.0f, 1.0f, 0.0f});
  basis.forward = normalizeOr(axisVector(forward_axis), {0.0f, 0.0f, 1.0f});
  if (std::abs(dot(basis.up, basis.forward)) > 0.99f) {
    basis.forward = std::abs(basis.up.z) > 0.5f ? Vec3{1.0f, 0.0f, 0.0f}
                                                : Vec3{0.0f, 0.0f, 1.0f};
  }
  basis.right = normalizeOr(cross(basis.up, basis.forward), {1.0f, 0.0f, 0.0f});
  basis.forward = normalizeOr(cross(basis.right, basis.up), basis.forward);
  return basis;
}

[[nodiscard]] Vec3 convertAxisVector(const Vec3 value, const AxisBasis &source,
                                     const AxisBasis &target) {
  const Vec3 world = source.right * value.x + source.up * value.y + source.forward * value.z;
  return {dot(world, target.right), dot(world, target.up), dot(world, target.forward)};
}

void flipTriangleWinding(CpuMesh &mesh) {
  for (std::size_t i = 0u; i + 2u < mesh.indices.size(); i += 3u) {
    std::swap(mesh.indices[i + 1u], mesh.indices[i + 2u]);
  }
}

void applyImportOptions(CpuMesh &mesh, const AssetMeshImportOptions &options) {
  const AxisBasis source = makeAxisBasis(options.source_up, options.source_forward);
  const AxisBasis target = makeAxisBasis(options.target_up, options.target_forward);
  const bool identity_axes = options.source_up == options.target_up &&
                             options.source_forward == options.target_forward;
  if (!identity_axes || options.unit_scale != 1.0f) {
    for (Vertex &vertex : mesh.vertices) {
      vertex.position = convertAxisVector(vertex.position, source, target) * options.unit_scale;
      vertex.normal = normalizeOr(convertAxisVector(vertex.normal, source, target),
                                  {0.0f, 1.0f, 0.0f});
      const Vec3 tangent =
          normalizeOr(convertAxisVector({vertex.tangent.x, vertex.tangent.y, vertex.tangent.z},
                                        source, target),
                      {1.0f, 0.0f, 0.0f});
      vertex.tangent = {tangent.x, tangent.y, tangent.z, vertex.tangent.w};
    }
  }
  if (options.flip_winding) {
    flipTriangleWinding(mesh);
    for (Vertex &vertex : mesh.vertices) {
      vertex.normal = vertex.normal * -1.0f;
      vertex.tangent.w *= -1.0f;
    }
  }
}

void addUnique(std::vector<std::string> &values, const std::string &value) {
  if (value.empty()) {
    return;
  }
  if (std::find(values.begin(), values.end(), value) == values.end()) {
    values.push_back(value);
  }
}

void addSourceTriangles(AssetMeshSourceSummary &summary, const std::string &object,
                        const std::string &group, const std::string &material,
                        const std::size_t first_triangle,
                        const std::size_t triangle_count) {
  if (triangle_count == 0u) {
    return;
  }
  addUnique(summary.objects, object);
  addUnique(summary.groups, group);
  addUnique(summary.materials, material);
  if (!summary.facets.empty()) {
    AssetMeshSourceFacet &last = summary.facets.back();
    if (last.object == object && last.group == group && last.material == material &&
        last.first_triangle + last.triangle_count == first_triangle) {
      last.triangle_count += triangle_count;
      return;
    }
  }
  summary.facets.push_back({.object = object,
                            .group = group,
                            .material = material,
                            .first_triangle = first_triangle,
                            .triangle_count = triangle_count});
}

void rebuildFlatNormals(CpuMesh &mesh) {
  for (Vertex &vertex : mesh.vertices) {
    vertex.normal = {};
  }
  for (std::size_t i = 0u; i + 2u < mesh.indices.size(); i += 3u) {
    Vertex &a = mesh.vertices[mesh.indices[i]];
    Vertex &b = mesh.vertices[mesh.indices[i + 1u]];
    Vertex &c = mesh.vertices[mesh.indices[i + 2u]];
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

[[nodiscard]] std::int32_t parseObjIndex(const std::string &token, const std::size_t size) {
  if (token.empty()) {
    return -1;
  }
  const int value = std::stoi(token);
  if (value > 0) {
    return value - 1;
  }
  if (value < 0) {
    return static_cast<std::int32_t>(size) + value;
  }
  return -1;
}

[[nodiscard]] AssetMeshImportResult importObj(const std::filesystem::path &path,
                                              const AssetMeshImportOptions &options) {
  AssetMeshImportResult result;
  result.report.path = path;
  result.report.format = AssetMeshFormat::Obj;
  result.report.stable_source_hash = hashFile(path);
  std::ifstream file(path);
  if (!file) {
    result.report.diagnostics.push_back("error: could not open OBJ");
    return result;
  }
  std::vector<Vec3> positions;
  std::vector<Vec3> normals;
  std::vector<Vec2> uvs;
  std::unordered_map<std::string, std::uint32_t> vertex_for_token;
  std::string current_object = path.stem().string();
  std::string current_group = "default";
  std::string current_material = "default";
  std::string line;
  while (std::getline(file, line)) {
    std::stringstream row(line);
    std::string kind;
    row >> kind;
    if (kind == "v") {
      Vec3 value;
      row >> value.x >> value.y >> value.z;
      positions.push_back(value);
    } else if (kind == "vn") {
      Vec3 value;
      row >> value.x >> value.y >> value.z;
      normals.push_back(normalizeOr(value, {0.0f, 1.0f, 0.0f}));
    } else if (kind == "vt") {
      Vec2 value;
      row >> value.x >> value.y;
      uvs.push_back(value);
    } else if (kind == "o") {
      std::string value;
      row >> value;
      if (!value.empty()) {
        current_object = value;
        addUnique(result.report.source.objects, current_object);
      }
    } else if (kind == "g") {
      std::string value;
      row >> value;
      if (!value.empty()) {
        current_group = value;
        addUnique(result.report.source.groups, current_group);
      }
    } else if (kind == "usemtl") {
      std::string value;
      row >> value;
      if (!value.empty()) {
        current_material = value;
        addUnique(result.report.source.materials, current_material);
      }
    } else if (kind == "f") {
      std::vector<std::uint32_t> face;
      std::string token;
      while (row >> token) {
        const auto [it, inserted] =
            vertex_for_token.emplace(token, static_cast<std::uint32_t>(result.mesh.vertices.size()));
        if (inserted) {
          std::array<std::string, 3> parts{};
          std::size_t start = 0u;
          for (std::size_t i = 0u; i < parts.size(); ++i) {
            const std::size_t slash = token.find('/', start);
            parts[i] = token.substr(start, slash == std::string::npos ? std::string::npos
                                                                      : slash - start);
            if (slash == std::string::npos) {
              break;
            }
            start = slash + 1u;
          }
          const std::int32_t position = parseObjIndex(parts[0], positions.size());
          const std::int32_t uv = parseObjIndex(parts[1], uvs.size());
          const std::int32_t normal = parseObjIndex(parts[2], normals.size());
          if (position < 0 || static_cast<std::size_t>(position) >= positions.size()) {
            result.report.diagnostics.push_back("error: OBJ face references a missing position");
            continue;
          }
          result.mesh.vertices.push_back(
              {.position = positions[static_cast<std::size_t>(position)],
               .normal = normal >= 0 && static_cast<std::size_t>(normal) < normals.size()
                             ? normals[static_cast<std::size_t>(normal)]
                             : Vec3{0.0f, 1.0f, 0.0f},
               .uv = uv >= 0 && static_cast<std::size_t>(uv) < uvs.size()
                         ? uvs[static_cast<std::size_t>(uv)]
                         : Vec2{}});
        }
        face.push_back(it->second);
      }
      const std::size_t first_triangle = result.mesh.indices.size() / 3u;
      for (std::size_t i = 1u; i + 1u < face.size(); ++i) {
        result.mesh.indices.insert(result.mesh.indices.end(), {face[0u], face[i], face[i + 1u]});
      }
      addSourceTriangles(result.report.source, current_object, current_group, current_material,
                         first_triangle, face.size() < 3u ? 0u : face.size() - 2u);
    }
  }
  if (normals.empty() && options.rebuild_missing_normals) {
    rebuildFlatNormals(result.mesh);
  }
  applyImportOptions(result.mesh, options);
  result.report.vertices = result.mesh.vertices.size();
  result.report.indices = result.mesh.indices.size();
  result.report.ok = !result.mesh.vertices.empty() && !result.mesh.indices.empty() &&
                     std::none_of(result.report.diagnostics.begin(),
                                  result.report.diagnostics.end(), [](const std::string &message) {
                                    return message.rfind("error:", 0u) == 0u;
                                  });
  return result;
}

[[nodiscard]] AssetMeshImportResult importPly(const std::filesystem::path &path,
                                              const AssetMeshImportOptions &options) {
  AssetMeshImportResult result;
  result.report.path = path;
  result.report.format = AssetMeshFormat::Ply;
  result.report.stable_source_hash = hashFile(path);
  std::ifstream file(path);
  if (!file) {
    result.report.diagnostics.push_back("error: could not open PLY");
    return result;
  }
  std::string line;
  std::size_t vertex_count = 0u;
  std::size_t face_count = 0u;
  bool ascii = false;
  while (std::getline(file, line)) {
    std::stringstream row(line);
    std::string a;
    row >> a;
    if (a == "format") {
      std::string format;
      row >> format;
      ascii = format == "ascii";
    } else if (a == "element") {
      std::string element;
      row >> element;
      if (element == "vertex") {
        row >> vertex_count;
      } else if (element == "face") {
        row >> face_count;
      }
    } else if (a == "end_header") {
      break;
    }
  }
  if (!ascii) {
    result.report.diagnostics.push_back("error: only ASCII PLY import is supported");
    return result;
  }
  result.mesh.vertices.reserve(vertex_count);
  for (std::size_t i = 0u; i < vertex_count && std::getline(file, line); ++i) {
    std::stringstream row(line);
    Vertex vertex;
    row >> vertex.position.x >> vertex.position.y >> vertex.position.z;
    vertex.normal = {0.0f, 1.0f, 0.0f};
    result.mesh.vertices.push_back(vertex);
  }
  for (std::size_t i = 0u; i < face_count && std::getline(file, line); ++i) {
    std::stringstream row(line);
    std::size_t count = 0u;
    row >> count;
    std::vector<std::uint32_t> face(count);
    for (std::uint32_t &index : face) {
      row >> index;
    }
    for (std::size_t corner = 1u; corner + 1u < face.size(); ++corner) {
      result.mesh.indices.insert(result.mesh.indices.end(),
                                 {face[0u], face[corner], face[corner + 1u]});
    }
  }
  if (options.rebuild_missing_normals) {
    rebuildFlatNormals(result.mesh);
  }
  applyImportOptions(result.mesh, options);
  addSourceTriangles(result.report.source, path.stem().string(), "default", "default", 0u,
                     result.mesh.indices.size() / 3u);
  result.report.vertices = result.mesh.vertices.size();
  result.report.indices = result.mesh.indices.size();
  result.report.ok = !result.mesh.vertices.empty() && !result.mesh.indices.empty();
  return result;
}

[[nodiscard]] AssetMeshImportResult importStl(const std::filesystem::path &path,
                                              const AssetMeshImportOptions &options) {
  AssetMeshImportResult result;
  result.report.path = path;
  result.report.format = AssetMeshFormat::Stl;
  result.report.stable_source_hash = hashFile(path);
  std::ifstream file(path);
  if (!file) {
    result.report.diagnostics.push_back("error: could not open STL");
    return result;
  }
  std::string line;
  std::string solid = path.stem().string();
  Vec3 current_normal{0.0f, 1.0f, 0.0f};
  std::vector<std::uint32_t> face;
  while (std::getline(file, line)) {
    std::stringstream row(line);
    std::string a;
    row >> a;
    if (a == "solid") {
      std::string value;
      row >> value;
      if (!value.empty()) {
        solid = value;
      }
    } else if (a == "facet") {
      std::string normal_word;
      row >> normal_word >> current_normal.x >> current_normal.y >> current_normal.z;
      current_normal = normalizeOr(current_normal, {0.0f, 1.0f, 0.0f});
    } else if (a == "vertex") {
      Vertex vertex;
      row >> vertex.position.x >> vertex.position.y >> vertex.position.z;
      vertex.normal = current_normal;
      const std::uint32_t index = static_cast<std::uint32_t>(result.mesh.vertices.size());
      result.mesh.vertices.push_back(vertex);
      face.push_back(index);
      if (face.size() == 3u) {
        const std::size_t first_triangle = result.mesh.indices.size() / 3u;
        result.mesh.indices.insert(result.mesh.indices.end(), face.begin(), face.end());
        addSourceTriangles(result.report.source, solid, "default", "default", first_triangle,
                           1u);
        face.clear();
      }
    }
  }
  applyImportOptions(result.mesh, options);
  result.report.vertices = result.mesh.vertices.size();
  result.report.indices = result.mesh.indices.size();
  result.report.ok = !result.mesh.vertices.empty() && !result.mesh.indices.empty();
  return result;
}

} // namespace

std::string_view assetMeshFormatName(const AssetMeshFormat format) {
  switch (format) {
  case AssetMeshFormat::Obj:
    return "obj";
  case AssetMeshFormat::Ply:
    return "ply";
  case AssetMeshFormat::Stl:
    return "stl";
  case AssetMeshFormat::Fbx:
    return "fbx";
  case AssetMeshFormat::Gltf:
    return "gltf";
  case AssetMeshFormat::Usd:
    return "usd";
  case AssetMeshFormat::Alembic:
    return "alembic";
  case AssetMeshFormat::Auto:
  default:
    return "auto";
  }
}

std::string_view assetMeshAxisName(const AssetMeshAxis axis) {
  switch (axis) {
  case AssetMeshAxis::PositiveX:
    return "+x";
  case AssetMeshAxis::PositiveY:
    return "+y";
  case AssetMeshAxis::PositiveZ:
    return "+z";
  case AssetMeshAxis::NegativeX:
    return "-x";
  case AssetMeshAxis::NegativeY:
    return "-y";
  case AssetMeshAxis::NegativeZ:
    return "-z";
  }
  return "unknown";
}

AssetMeshFormat assetMeshFormatFromPath(const std::filesystem::path &path) {
  const std::string extension = lowerExtension(path);
  if (extension == ".obj") {
    return AssetMeshFormat::Obj;
  }
  if (extension == ".ply") {
    return AssetMeshFormat::Ply;
  }
  if (extension == ".stl") {
    return AssetMeshFormat::Stl;
  }
  if (extension == ".fbx") {
    return AssetMeshFormat::Fbx;
  }
  if (extension == ".gltf" || extension == ".glb") {
    return AssetMeshFormat::Gltf;
  }
  if (extension == ".usd" || extension == ".usda" || extension == ".usdc") {
    return AssetMeshFormat::Usd;
  }
  if (extension == ".abc" || extension == ".alembic") {
    return AssetMeshFormat::Alembic;
  }
  return AssetMeshFormat::Auto;
}

AssetMeshImportResult importMeshAsset(const std::filesystem::path &path,
                                      AssetMeshFormat format) {
  AssetMeshImportOptions options;
  options.format = format;
  return importMeshAsset(path, options);
}

AssetMeshImportResult importMeshAsset(const std::filesystem::path &path,
                                      AssetMeshImportOptions options) {
  if (options.format == AssetMeshFormat::Auto) {
    options.format = assetMeshFormatFromPath(path);
  }
  switch (options.format) {
  case AssetMeshFormat::Obj:
    return importObj(path, options);
  case AssetMeshFormat::Ply:
    return importPly(path, options);
  case AssetMeshFormat::Stl:
    return importStl(path, options);
  case AssetMeshFormat::Fbx:
  case AssetMeshFormat::Gltf:
  case AssetMeshFormat::Usd:
  case AssetMeshFormat::Alembic: {
    AssetMeshImportResult result;
    result.report.path = path;
    result.report.format = options.format;
    result.report.stable_source_hash = hashFile(path);
    result.report.diagnostics.push_back(
        "error: unsupported mesh import format: " +
        std::string(assetMeshFormatName(options.format)) + " importer is not vendored in Aster");
    return result;
  }
  case AssetMeshFormat::Auto:
  default: {
    AssetMeshImportResult result;
    result.report.path = path;
    result.report.diagnostics.push_back("error: unsupported mesh import format");
    return result;
  }
  }
}

AssetMeshIoReport exportMeshAssetObj(const CpuMesh &mesh, const std::filesystem::path &path) {
  AssetMeshIoReport report;
  report.path = path;
  report.format = AssetMeshFormat::Obj;
  std::ofstream file(path);
  if (!file) {
    report.diagnostics.push_back("error: could not open OBJ for writing");
    return report;
  }
  file << "# Aster mesh export\n";
  for (const Vertex &vertex : mesh.vertices) {
    file << "v " << vertex.position.x << ' ' << vertex.position.y << ' ' << vertex.position.z
         << '\n';
  }
  for (const Vertex &vertex : mesh.vertices) {
    file << "vt " << vertex.uv.x << ' ' << vertex.uv.y << '\n';
  }
  for (const Vertex &vertex : mesh.vertices) {
    file << "vn " << vertex.normal.x << ' ' << vertex.normal.y << ' ' << vertex.normal.z
         << '\n';
  }
  for (std::size_t i = 0u; i + 2u < mesh.indices.size(); i += 3u) {
    file << "f";
    for (std::size_t corner = 0u; corner < 3u; ++corner) {
      const std::uint32_t obj_index = mesh.indices[i + corner] + 1u;
      file << ' ' << obj_index << '/' << obj_index << '/' << obj_index;
    }
    file << '\n';
  }
  report.vertices = mesh.vertices.size();
  report.indices = mesh.indices.size();
  report.ok = file.good();
  report.stable_source_hash = report.ok ? hashFile(path) : std::string();
  return report;
}

AssetMeshIoReport exportMeshAssetPly(const CpuMesh &mesh, const std::filesystem::path &path) {
  AssetMeshIoReport report;
  report.path = path;
  report.format = AssetMeshFormat::Ply;
  std::ofstream file(path);
  if (!file) {
    report.diagnostics.push_back("error: could not open PLY for writing");
    return report;
  }
  file << "ply\nformat ascii 1.0\nelement vertex " << mesh.vertices.size() << '\n';
  file << "property float x\nproperty float y\nproperty float z\n";
  file << "property float nx\nproperty float ny\nproperty float nz\n";
  file << "property float s\nproperty float t\n";
  file << "element face " << mesh.indices.size() / 3u << '\n';
  file << "property list uchar uint vertex_indices\nend_header\n";
  for (const Vertex &vertex : mesh.vertices) {
    file << vertex.position.x << ' ' << vertex.position.y << ' ' << vertex.position.z << ' '
         << vertex.normal.x << ' ' << vertex.normal.y << ' ' << vertex.normal.z << ' '
         << vertex.uv.x << ' ' << vertex.uv.y << '\n';
  }
  for (std::size_t i = 0u; i + 2u < mesh.indices.size(); i += 3u) {
    file << "3 " << mesh.indices[i] << ' ' << mesh.indices[i + 1u] << ' '
         << mesh.indices[i + 2u] << '\n';
  }
  report.vertices = mesh.vertices.size();
  report.indices = mesh.indices.size();
  report.ok = file.good();
  report.stable_source_hash = report.ok ? hashFile(path) : std::string();
  return report;
}

AssetMeshIoReport exportMeshAssetStl(const CpuMesh &mesh, const std::filesystem::path &path) {
  AssetMeshIoReport report;
  report.path = path;
  report.format = AssetMeshFormat::Stl;
  std::ofstream file(path);
  if (!file) {
    report.diagnostics.push_back("error: could not open STL for writing");
    return report;
  }
  file << "solid aster_mesh\n";
  for (std::size_t i = 0u; i + 2u < mesh.indices.size(); i += 3u) {
    const Vertex &a = mesh.vertices[mesh.indices[i]];
    const Vertex &b = mesh.vertices[mesh.indices[i + 1u]];
    const Vertex &c = mesh.vertices[mesh.indices[i + 2u]];
    const Vec3 normal = normalizeOr(cross(b.position - a.position, c.position - a.position),
                                    {0.0f, 1.0f, 0.0f});
    file << "  facet normal " << normal.x << ' ' << normal.y << ' ' << normal.z << '\n';
    file << "    outer loop\n";
    file << "      vertex " << a.position.x << ' ' << a.position.y << ' ' << a.position.z
         << '\n';
    file << "      vertex " << b.position.x << ' ' << b.position.y << ' ' << b.position.z
         << '\n';
    file << "      vertex " << c.position.x << ' ' << c.position.y << ' ' << c.position.z
         << '\n';
    file << "    endloop\n";
    file << "  endfacet\n";
  }
  file << "endsolid aster_mesh\n";
  report.vertices = mesh.vertices.size();
  report.indices = mesh.indices.size();
  report.ok = file.good();
  report.stable_source_hash = report.ok ? hashFile(path) : std::string();
  return report;
}

} // namespace aster
