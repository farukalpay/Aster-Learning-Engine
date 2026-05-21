// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#pragma once

#include "aster/math/transform.hpp"
#include "aster/render/camera.hpp"
#include "aster/render/render_device.hpp"
#include "aster/scene/scene.hpp"

#include <filesystem>
#include <memory>
#include <string>
#include <string_view>

namespace aster {

struct AsterAppConfig {
  int width = 1280;
  int height = 720;
  std::string title = "Aster";
  bool headless = false;
  bool vsync = true;
  int max_frames = 0;
};

struct Camera3D {
  OrbitCamera orbit;
};

struct Mesh {
  std::string name;
  MeshPrimitive primitive = MeshPrimitive::Sphere;
  std::shared_ptr<const CpuMesh> custom_mesh;
};

void InitAster(int width, int height, std::string_view title);
void InitAster(const AsterAppConfig &config);
void CloseAster();

[[nodiscard]] bool Frame();
void BeginScene(const Camera3D &camera);
void BeginScene(const OrbitCamera &camera);
void DrawMesh(const Mesh &mesh, const Material &material, const Transform &transform = {});
void DrawLight(Vec3 position, Vec3 color, float intensity = 1.0f, float source_radius = 0.5f);
void EndScene();
void CaptureFrame(const std::filesystem::path &path);

[[nodiscard]] Camera3D MakeOrbitCamera(Vec3 target, float radius,
                                       float yaw_degrees = 36.0f,
                                       float pitch_degrees = 18.0f);
[[nodiscard]] Material LoadMaterial(const std::filesystem::path &path_or_name);
[[nodiscard]] Mesh LoadMesh(const std::filesystem::path &path_or_name);
[[nodiscard]] Transform MakeTransform(Vec3 position,
                                      Vec3 rotation_degrees = {},
                                      Vec3 scale = {1.0f, 1.0f, 1.0f});

[[nodiscard]] const FrameStats &LastFrameStats();
[[nodiscard]] const FrameForensics &LastFrameForensics();
[[nodiscard]] const char *AsterBackendName();

} // namespace aster
