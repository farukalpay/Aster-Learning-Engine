// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#include "aster/aster.hpp"

#include <exception>
#include <filesystem>
#include <iostream>
#include <string_view>

#ifndef ASTER_SOURCE_DIR
#define ASTER_SOURCE_DIR "."
#endif

namespace {

std::filesystem::path argumentPath(const int argc, char **argv, const std::string_view name) {
  for (int i = 1; i + 1 < argc; ++i) {
    if (std::string_view(argv[i]) == name) {
      return argv[i + 1];
    }
  }
  return {};
}

} // namespace

int main(int argc, char **argv) {
  try {
    const std::filesystem::path capture_path = argumentPath(argc, argv, "--capture");
    const std::filesystem::path root = ASTER_SOURCE_DIR;

    aster::AsterAppConfig config;
    config.width = 1280;
    config.height = 720;
    config.title = "Aster Quickstart";
    config.headless = !capture_path.empty();
    config.max_frames = config.headless ? 1 : 0;
    aster::InitAster(config);

    const aster::Camera3D cam =
        aster::MakeOrbitCamera({0.0f, 0.58f, 0.0f}, 5.0f, 64.0f, 15.0f);
    const aster::Material rust =
        aster::LoadMaterial(root / "showcases/material_lab/weathered_metal.astermat");
    const aster::Mesh pipe = aster::LoadMesh(root / "showcases/pipe_lab/rusted_pipe.astergraph");

    while (aster::Frame()) {
      aster::BeginScene(cam);
      aster::DrawMesh(pipe, rust);
      aster::DrawLight({-3.6f, 3.2f, 2.4f}, {8.0f, 6.4f, 4.8f}, 1.0f, 0.8f);
      aster::EndScene();
      if (!capture_path.empty()) {
        aster::CaptureFrame(capture_path);
      }
    }

    aster::CloseAster();
    return 0;
  } catch (const std::exception &error) {
    std::cerr << "aster_quickstart: " << error.what() << '\n';
    aster::CloseAster();
    return 1;
  }
}
