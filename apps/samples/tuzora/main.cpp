// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#include "aster/core/clock.hpp"
#include "aster/core/config.hpp"
#include "aster/core/fixed_timestep.hpp"
#include "aster/input/control_scheme.hpp"
#include "aster/input/input_codes.hpp"
#include "aster/net/lockstep_command_channel.hpp"
#include "aster/platform/window.hpp"
#include "aster/render/camera2d.hpp"
#include "aster/render/frame_capture.hpp"
#include "aster/render/mesh.hpp"
#include "aster/render/render_device.hpp"
#include "aster/samples/tuzora/tuzora.hpp"
#include "aster/scene/scene.hpp"
#include "aster/ui/ui_canvas.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

constexpr const char *kMoveLeft = "tuzora.move.left";
constexpr const char *kMoveRight = "tuzora.move.right";
constexpr const char *kJump = "tuzora.jump";
constexpr const char *kRun = "tuzora.run";
constexpr const char *kInteract = "tuzora.interact";
constexpr const char *kBreak = "tuzora.break";
constexpr const char *kPlace = "tuzora.place";
constexpr const char *kReset = "tuzora.reset";
constexpr const char *kQuit = "tuzora.quit";
constexpr const char *kHotbar1 = "tuzora.hotbar.1";
constexpr const char *kHotbar2 = "tuzora.hotbar.2";
constexpr const char *kHotbar3 = "tuzora.hotbar.3";
constexpr const char *kHotbar4 = "tuzora.hotbar.4";
constexpr const char *kHotbar5 = "tuzora.hotbar.5";

bool hasArgument(const int argc, char **argv, const std::string_view value) {
  for (int i = 1; i < argc; ++i) {
    if (std::string_view(argv[i]) == value) {
      return true;
    }
  }
  return false;
}

std::string argumentString(const int argc, char **argv, const std::string_view name,
                           const std::string &fallback = {}) {
  for (int i = 1; i + 1 < argc; ++i) {
    if (std::string_view(argv[i]) == name) {
      return argv[i + 1];
    }
  }
  return fallback;
}

int argumentInt(const int argc, char **argv, const std::string_view name, const int fallback) {
  const std::string value = argumentString(argc, argv, name);
  if (value.empty()) {
    return fallback;
  }
  return std::stoi(value);
}

std::uint32_t argumentU32(const int argc, char **argv, const std::string_view name,
                          const std::uint32_t fallback) {
  const std::string value = argumentString(argc, argv, name);
  if (value.empty()) {
    return fallback;
  }
  return static_cast<std::uint32_t>(std::stoul(value));
}

std::filesystem::path argumentPath(const int argc, char **argv, const std::string_view name) {
  const std::string value = argumentString(argc, argv, name);
  return value.empty() ? std::filesystem::path{} : std::filesystem::path(value);
}

std::filesystem::path defaultProjectPath() {
#if defined(ASTER_TUZORA_PROJECT)
  return ASTER_TUZORA_PROJECT;
#else
  return {};
#endif
}

aster::ControlScheme makeControls() {
  aster::ControlScheme scheme;
  for (const char *command : {kMoveLeft, kMoveRight, kJump, kRun, kInteract, kBreak, kPlace, kReset,
                              kQuit, kHotbar1, kHotbar2, kHotbar3, kHotbar4, kHotbar5}) {
    scheme.addCommand(command);
  }
  scheme.bind(kMoveLeft, aster::keyBinding(aster::Key::A));
  scheme.bind(kMoveLeft, aster::keyBinding(aster::Key::Left));
  scheme.bind(kMoveRight, aster::keyBinding(aster::Key::D));
  scheme.bind(kMoveRight, aster::keyBinding(aster::Key::Right));
  scheme.bind(kJump, aster::keyBinding(aster::Key::W));
  scheme.bind(kJump, aster::keyBinding(aster::Key::Up));
  scheme.bind(kJump, aster::keyBinding(aster::Key::Space));
  scheme.bind(kRun, aster::keyBinding(aster::Key::LeftShift));
  scheme.bind(kInteract, aster::keyBinding(aster::Key::E));
  scheme.bind(kBreak, aster::keyBinding(aster::Key::Q));
  scheme.bind(kBreak, aster::mouseBinding(aster::MouseButton::Left));
  scheme.bind(kPlace, aster::mouseBinding(aster::MouseButton::Right));
  scheme.bind(kReset, aster::keyBinding(aster::Key::R));
  scheme.bind(kQuit, aster::keyBinding(aster::Key::Escape));
  scheme.bind(kHotbar1, aster::keyBinding(aster::Key::Num1));
  scheme.bind(kHotbar2, aster::keyBinding(aster::Key::Num2));
  scheme.bind(kHotbar3, aster::keyBinding(aster::Key::Num3));
  scheme.bind(kHotbar4, aster::keyBinding(aster::Key::Num4));
  scheme.bind(kHotbar5, aster::keyBinding(aster::Key::Num5));
  return scheme;
}

aster::SimCommand commandFromControls(const aster::ControlState &controls) {
  aster::SimCommand command;
  const float x = controls.strength(kMoveRight) - controls.strength(kMoveLeft);
  command.strafe = static_cast<std::int16_t>(std::clamp(x, -1.0f, 1.0f) * 32767.0f);
  command.set(aster::SimCommandButton::Jump, controls.justPressed(kJump));
  command.set(aster::SimCommandButton::Run, controls.pressed(kRun));
  command.set(aster::SimCommandButton::Interact, controls.justPressed(kInteract));
  command.set(aster::SimCommandButton::Primary, controls.justPressed(kBreak));
  command.set(aster::SimCommandButton::Secondary, controls.justPressed(kPlace));
  if (controls.justPressed(kHotbar1)) {
    command.look = 1;
  } else if (controls.justPressed(kHotbar2)) {
    command.look = 2;
  } else if (controls.justPressed(kHotbar3)) {
    command.look = 3;
  } else if (controls.justPressed(kHotbar4)) {
    command.look = 4;
  } else if (controls.justPressed(kHotbar5)) {
    command.look = 5;
  }
  return command;
}

aster::RendererSettings tuzoraRendererSettings() {
  aster::RendererSettings settings;
  settings.exposure = 1.34f;
  settings.ambient_strength = 0.68f;
  settings.ambient_floor = 0.14f;
  settings.sky_ambient_color = {0.72f, 0.54f, 0.36f};
  settings.ground_ambient_color = {0.28f, 0.22f, 0.18f};
  settings.sun_light.enabled = true;
  settings.sun_light.direction_to_light = {-0.34f, 0.82f, 0.30f};
  settings.sun_light.color = {1.0f, 0.72f, 0.42f};
  settings.sun_light.intensity = 0.92f;
  settings.light_rig = {{{-6.0f, 7.0f, 6.0f}, {1.0f, 0.44f, 0.18f}, 0.38f, 8.0f},
                        {{10.0f, 5.5f, 5.0f}, {0.12f, 0.58f, 0.78f}, 0.24f, 9.0f},
                        {{33.0f, 7.4f, 6.7f}, {1.0f, 0.52f, 0.16f}, 0.40f, 5.0f}};
  settings.pipeline.clear_color = {0.38f, 0.57f, 0.76f};
  settings.pipeline.multisampling = false;
  settings.pipeline.tone_mapper = aster::ToneMapper::PbrNeutral;
  settings.procedural_surface_normals = false;
  settings.grounding.enabled = false;
  settings.grounding.contact_shadows = false;
  settings.atmosphere.enabled = true;
  settings.atmosphere.fog_color = {0.72f, 0.48f, 0.30f};
  settings.atmosphere.fog_start = 18.0f;
  settings.atmosphere.fog_end = 62.0f;
  settings.atmosphere.fog_strength = 0.014f;
  settings.atmosphere.saturation = 1.46f;
  settings.atmosphere.contrast = 1.22f;
  settings.post.bloom = true;
  settings.post.fxaa = false;
  settings.post.bloom_threshold = 0.92f;
  settings.post.bloom_intensity = 0.26f;
  settings.surface_scale.physical_texel_density = 256.0f;
  settings.style.unlit_mix = 0.58f;
  settings.style.emissive_gain = 1.55f;
  settings.style.color_quantization_steps = 42.0f;
  return settings;
}

aster::Camera2DConfig tuzoraCameraConfig() {
  aster::Camera2DConfig config;
  config.min_target = {5.0f, 2.7f};
  config.max_target = {46.0f, 9.0f};
  config.deadzone = {0.90f, 0.50f};
  config.smoothing = 0.20f;
  config.orthographic_height = 12.0f;
  config.distance = 28.0f;
  config.shake_decay = 0.82f;
  return config;
}

struct PixelImage {
  std::uint32_t width = 0u;
  std::uint32_t height = 0u;
  std::vector<std::uint8_t> rgba8;

  [[nodiscard]] bool valid() const {
    return width > 0u && height > 0u &&
           rgba8.size() >= static_cast<std::size_t>(width) * height * 4u;
  }
};

struct PixelAssets {
  std::unordered_map<std::string, PixelImage> images;
};

std::string readPpmToken(std::istream &input) {
  char ch = '\0';
  while (input.get(ch)) {
    if (std::isspace(static_cast<unsigned char>(ch))) {
      continue;
    }
    if (ch == '#') {
      std::string ignored;
      std::getline(input, ignored);
      continue;
    }
    std::string token(1u, ch);
    while (input.good()) {
      const int next = input.peek();
      if (next == std::char_traits<char>::eof() || std::isspace(static_cast<unsigned char>(next))) {
        break;
      }
      token.push_back(static_cast<char>(input.get()));
    }
    return token;
  }
  return {};
}

std::optional<PixelImage> loadPpmImage(const std::filesystem::path &path) {
  std::ifstream input(path, std::ios::binary);
  if (!input) {
    return std::nullopt;
  }
  const std::string magic = readPpmToken(input);
  const std::string width_text = readPpmToken(input);
  const std::string height_text = readPpmToken(input);
  const std::string max_text = readPpmToken(input);
  if (magic != "P6" || width_text.empty() || height_text.empty() || max_text != "255") {
    return std::nullopt;
  }
  const auto width = static_cast<std::uint32_t>(std::stoul(width_text));
  const auto height = static_cast<std::uint32_t>(std::stoul(height_text));
  if (width == 0u || height == 0u || width > 1024u || height > 1024u) {
    return std::nullopt;
  }
  if (input.peek() != std::char_traits<char>::eof()) {
    input.get();
  }
  std::vector<std::uint8_t> rgb(static_cast<std::size_t>(width) * height * 3u);
  input.read(reinterpret_cast<char *>(rgb.data()), static_cast<std::streamsize>(rgb.size()));
  if (input.gcount() != static_cast<std::streamsize>(rgb.size())) {
    return std::nullopt;
  }

  PixelImage image;
  image.width = width;
  image.height = height;
  image.rgba8.resize(static_cast<std::size_t>(width) * height * 4u);
  for (std::size_t src = 0u, dst = 0u; src + 2u < rgb.size(); src += 3u, dst += 4u) {
    const std::uint8_t r = rgb[src + 0u];
    const std::uint8_t g = rgb[src + 1u];
    const std::uint8_t b = rgb[src + 2u];
    image.rgba8[dst + 0u] = r;
    image.rgba8[dst + 1u] = g;
    image.rgba8[dst + 2u] = b;
    image.rgba8[dst + 3u] = (r == 255u && g == 0u && b == 255u) ? 0u : 255u;
  }
  return image.valid() ? std::optional<PixelImage>{std::move(image)} : std::nullopt;
}

std::filesystem::path projectRootFromPath(std::filesystem::path project_path) {
  if (project_path.empty()) {
    return std::filesystem::current_path() / "projects" / "tuzora";
  }
  if (project_path.extension() == ".asterproj") {
    project_path = project_path.parent_path();
  }
  return project_path.empty() ? std::filesystem::current_path() / "projects" / "tuzora"
                              : project_path;
}

PixelAssets loadPixelAssets(const std::filesystem::path &project_path) {
  const std::filesystem::path texture_root = projectRootFromPath(project_path) / "textures";
  PixelAssets assets;
  for (const char *name :
       {"tuzora_beach_edge", "tuzora_beach_sand", "tuzora_bucket", "tuzora_friend",
        "tuzora_lamp_item", "tuzora_lamp_post", "tuzora_player", "tuzora_rock_prop",
        "tuzora_roof_tile", "tuzora_scrap_part", "tuzora_sea_water", "tuzora_sea_water_alt",
        "tuzora_shade_cloth", "tuzora_shell", "tuzora_signal_mast", "tuzora_signal_part",
        "tuzora_umbrella", "tuzora_wood_crate", "tuzora_wood_plank"}) {
    if (std::optional<PixelImage> image = loadPpmImage(texture_root / (std::string(name) + ".ppm"));
        image.has_value()) {
      assets.images.emplace(name, std::move(*image));
    }
  }
  return assets;
}

bool startsWith(const std::string &value, const std::string_view prefix) {
  return value.rfind(std::string(prefix), 0u) == 0u;
}

aster::UiColor materialColor(const aster::Material &material) {
  const float glow = std::clamp(material.emission_strength, 0.0f, 2.8f) * 0.18f;
  return {std::clamp(material.base_color.x + material.emission_color.x * glow, 0.0f, 1.0f),
          std::clamp(material.base_color.y + material.emission_color.y * glow, 0.0f, 1.0f),
          std::clamp(material.base_color.z + material.emission_color.z * glow, 0.0f, 1.0f),
          std::clamp(material.opacity, 0.0f, 1.0f)};
}

aster::Vec2 meshSize2D(const aster::RenderObject &object) {
  if (object.custom_mesh == nullptr || object.custom_mesh->vertices.empty()) {
    return {1.0f, 1.0f};
  }
  float min_x = object.custom_mesh->vertices.front().position.x;
  float max_x = min_x;
  float min_z = object.custom_mesh->vertices.front().position.z;
  float max_z = min_z;
  for (const aster::Vertex &vertex : object.custom_mesh->vertices) {
    min_x = std::min(min_x, vertex.position.x);
    max_x = std::max(max_x, vertex.position.x);
    min_z = std::min(min_z, vertex.position.z);
    max_z = std::max(max_z, vertex.position.z);
  }
  return {std::max(max_x - min_x, 0.04f), std::max(max_z - min_z, 0.04f)};
}

bool skipWorldOverlayObject(const std::string &name) {
  return name.find("haze veil") != std::string::npos ||
         name.find("sky gradient") != std::string::npos ||
         name.find("peach horizon") != std::string::npos ||
         name.find("sunset disc") != std::string::npos ||
         name.find("sun hot center") != std::string::npos || startsWith(name, "Debug ");
}

const PixelImage *assetImage(const PixelAssets &assets, const std::string_view key) {
  const auto found = assets.images.find(std::string(key));
  return found == assets.images.end() || !found->second.valid() ? nullptr : &found->second;
}

bool drawSprite(aster::UiCanvas &canvas, const PixelAssets &assets, const std::string_view key,
                const aster::UiRect rect) {
  const PixelImage *image = assetImage(assets, key);
  if (image == nullptr) {
    return false;
  }
  canvas.image(rect, image->width, image->height, image->rgba8);
  return true;
}

bool drawTiledSprite(aster::UiCanvas &canvas, const PixelAssets &assets, const std::string_view key,
                     const aster::UiRect rect, const float tile_width, const float tile_height) {
  const PixelImage *image = assetImage(assets, key);
  if (image == nullptr || tile_width <= 0.0f || tile_height <= 0.0f) {
    return false;
  }
  const aster::Vec2 viewport = canvas.viewportSize();
  const float clip_x0 = std::max(rect.x, -tile_width);
  const float clip_y0 = std::max(rect.y, -tile_height);
  const float clip_x1 = std::min(rect.x + rect.width, viewport.x + tile_width);
  const float clip_y1 = std::min(rect.y + rect.height, viewport.y + tile_height);
  const float start_x = std::floor(clip_x0 / tile_width) * tile_width;
  const float start_y = std::floor(clip_y0 / tile_height) * tile_height;
  for (float y = start_y; y < clip_y1; y += tile_height) {
    for (float x = start_x; x < clip_x1; x += tile_width) {
      canvas.image({x, y, tile_width, tile_height}, image->width, image->height, image->rgba8);
    }
  }
  return true;
}

bool hasRequiredVisualAssets(const PixelAssets &assets) {
  for (const char *key :
       {"tuzora_beach_edge", "tuzora_beach_sand", "tuzora_friend", "tuzora_lamp_post",
        "tuzora_player", "tuzora_roof_tile", "tuzora_scrap_part", "tuzora_sea_water",
        "tuzora_sea_water_alt", "tuzora_shade_cloth", "tuzora_shell", "tuzora_signal_mast",
        "tuzora_signal_part", "tuzora_wood_crate", "tuzora_wood_plank"}) {
    if (assetImage(assets, key) == nullptr) {
      return false;
    }
  }
  return true;
}

std::string_view spriteForObjectName(const std::string &name) {
  if (startsWith(name, "Tuzora terrain tile ") || startsWith(name, "Tuzora placed block ")) {
    if (name.find("roof_tile") != std::string::npos) {
      return "tuzora_roof_tile";
    }
    if (name.find("wood") != std::string::npos || name.find("pier") != std::string::npos) {
      return "tuzora_wood_plank";
    }
    if (name.find("shade") != std::string::npos) {
      return "tuzora_shade_cloth";
    }
    if (name.find("crate") != std::string::npos) {
      return "tuzora_wood_crate";
    }
    return name.find("sand_step") != std::string::npos ? std::string_view{"tuzora_beach_edge"}
                                                       : std::string_view{"tuzora_beach_sand"};
  }
  if (startsWith(name, "Tuzora collectible ")) {
    if (name.find("shells") != std::string::npos) {
      return "tuzora_shell";
    }
    if (name.find("plank") != std::string::npos) {
      return "tuzora_wood_plank";
    }
    if (name.find("scrap") != std::string::npos) {
      return "tuzora_scrap_part";
    }
    if (name.find("cloth") != std::string::npos) {
      return "tuzora_shade_cloth";
    }
    if (name.find("signal_parts") != std::string::npos) {
      return "tuzora_signal_part";
    }
  }
  if (startsWith(name, "Tuzora rooftop signal")) {
    return "tuzora_signal_mast";
  }
  if (startsWith(name, "Tuzora rooftop shade awning")) {
    return "tuzora_shade_cloth";
  }
  if (startsWith(name, "Tuzora lamp glow placed")) {
    return "tuzora_lamp_post";
  }
  return {};
}

void drawCoastSpriteWorld(aster::UiCanvas &canvas, const aster::Scene &scene,
                          const aster::Camera2DState &camera_state,
                          const aster::Camera2DConfig &camera_config, const PixelAssets &assets,
                          const float width, const float height, const float day_fraction) {
  const float dusk = std::clamp((day_fraction - 0.58f) * 2.4f, 0.0f, 1.0f);
  const float night = std::clamp((day_fraction - 0.82f) * 5.0f, 0.0f, 1.0f);
  canvas.fillRect({0.0f, 0.0f, width, height},
                  {0.28f - night * 0.12f, 0.58f - night * 0.26f, 0.82f - night * 0.38f, 1.0f});
  canvas.fillRect({0.0f, height * 0.21f, width, height * 0.18f},
                  {0.96f, 0.58f - night * 0.18f, 0.28f - night * 0.10f, 0.92f});
  canvas.fillRect({0.0f, height * 0.33f, width, height * 0.18f},
                  {0.03f, 0.56f - night * 0.18f, 0.70f - night * 0.24f, 0.92f});
  canvas.fillRect({0.0f, height * 0.48f, width, height * 0.06f}, {0.66f, 0.80f, 0.66f, 0.34f});
  canvas.fillCircle({width * 0.78f, height * (0.18f + dusk * 0.08f)}, 34.0f,
                    {1.0f, 0.64f, 0.22f, 0.76f}, 24);
  canvas.fillCircle({width * 0.78f, height * (0.18f + dusk * 0.08f)}, 18.0f,
                    {1.0f, 0.82f, 0.32f, 0.96f}, 20);

  const float pixels_per_unit = height / std::max(camera_config.orthographic_height, 1.0f);
  const float world_center_y = height * 0.52f;
  const auto worldToScreen = [&](const aster::Vec2 world) {
    return aster::Vec2{width * 0.5f + (world.x - camera_state.target.x) * pixels_per_unit,
                       world_center_y - (world.y - camera_state.target.y) * pixels_per_unit};
  };

  std::vector<const aster::RenderObject *> objects;
  objects.reserve(scene.objects().size());
  for (const aster::RenderObject &object : scene.objects()) {
    if (!skipWorldOverlayObject(object.name)) {
      objects.push_back(&object);
    }
  }
  std::sort(objects.begin(), objects.end(),
            [](const aster::RenderObject *lhs, const aster::RenderObject *rhs) {
              if (lhs->transform.position.y != rhs->transform.position.y) {
                return lhs->transform.position.y < rhs->transform.position.y;
              }
              return lhs->transform.position.z > rhs->transform.position.z;
            });

  for (const aster::RenderObject *object : objects) {
    const aster::Vec2 mesh_size = meshSize2D(*object);
    const float rect_width =
        std::max(1.0f, mesh_size.x * std::abs(object->transform.scale.x) * pixels_per_unit);
    const float rect_height =
        std::max(1.0f, mesh_size.y * std::abs(object->transform.scale.z) * pixels_per_unit);
    const aster::Vec2 center =
        worldToScreen({object->transform.position.x, object->transform.position.z});
    aster::UiRect rect{std::floor(center.x - rect_width * 0.5f),
                       std::floor(center.y - rect_height * 0.5f), std::ceil(rect_width),
                       std::ceil(rect_height)};
    if (rect.x > width + 80.0f || rect.x + rect.width < -80.0f || rect.y > height + 80.0f ||
        rect.y + rect.height < -80.0f) {
      continue;
    }
    aster::UiColor color = materialColor(object->material);
    if (!assets.images.empty()) {
      if (startsWith(object->name, "Tuzora sea parallax band")) {
        canvas.fillRect(rect, {0.02f, 0.44f - night * 0.12f, 0.62f - night * 0.22f, 0.92f});
        drawTiledSprite(canvas, assets, "tuzora_sea_water", rect, pixels_per_unit * 1.18f,
                        pixels_per_unit * 0.92f);
        drawTiledSprite(canvas, assets, "tuzora_sea_water_alt",
                        {rect.x + pixels_per_unit * 0.50f, rect.y + pixels_per_unit * 0.36f,
                         rect.width, rect.height * 0.42f},
                        pixels_per_unit * 1.32f, pixels_per_unit * 0.46f);
        continue;
      }
      if (startsWith(object->name, "Tuzora player")) {
        if (object->name == "Tuzora player shirt") {
          const aster::Vec2 player_center =
              worldToScreen({object->transform.position.x, object->transform.position.z - 0.08f});
          drawSprite(canvas, assets, "tuzora_player",
                     {std::floor(player_center.x - pixels_per_unit * 0.52f),
                      std::floor(player_center.y - pixels_per_unit * 0.86f),
                      std::ceil(pixels_per_unit * 1.04f), std::ceil(pixels_per_unit * 1.72f)});
        }
        continue;
      }
      if (startsWith(object->name, "Tuzora friend silhouette")) {
        const aster::Vec2 friend_center =
            worldToScreen({object->transform.position.x, object->transform.position.z + 0.22f});
        drawSprite(canvas, assets, "tuzora_friend",
                   {std::floor(friend_center.x - pixels_per_unit * 0.32f),
                    std::floor(friend_center.y - pixels_per_unit * 0.58f),
                    std::ceil(pixels_per_unit * 0.64f), std::ceil(pixels_per_unit * 1.16f)});
        continue;
      }
      if (startsWith(object->name, "Tuzora friend head")) {
        continue;
      }
      if (startsWith(object->name, "Tuzora lamp glow placed")) {
        canvas.fillCircle(center, pixels_per_unit * 0.72f, {1.0f, 0.58f, 0.16f, 0.20f}, 20);
        drawSprite(canvas, assets, "tuzora_lamp_post",
                   {std::floor(center.x - pixels_per_unit * 0.36f),
                    std::floor(center.y - pixels_per_unit * 0.62f),
                    std::ceil(pixels_per_unit * 0.72f), std::ceil(pixels_per_unit * 1.24f)});
        continue;
      }
      if (startsWith(object->name, "Tuzora lamp pole placed") ||
          startsWith(object->name, "Tuzora lamp bulb placed")) {
        continue;
      }
      if (startsWith(object->name, "Tuzora rooftop signal")) {
        if (object->material.emission_strength > 0.8f) {
          canvas.fillCircle(
              {center.x + pixels_per_unit * 0.16f, center.y - pixels_per_unit * 0.38f},
              pixels_per_unit * 0.76f, {1.0f, 0.62f, 0.12f, 0.20f}, 20);
        }
        drawSprite(canvas, assets, "tuzora_signal_mast",
                   {std::floor(center.x - pixels_per_unit * 0.46f),
                    std::floor(center.y - pixels_per_unit * 0.95f),
                    std::ceil(pixels_per_unit * 0.92f), std::ceil(pixels_per_unit * 1.90f)});
        continue;
      }
      if (startsWith(object->name, "Tuzora collectible ")) {
        const std::string_view key = spriteForObjectName(object->name);
        if (!key.empty() &&
            drawSprite(canvas, assets, key,
                       {std::floor(center.x - pixels_per_unit * 0.34f),
                        std::floor(center.y - pixels_per_unit * 0.34f),
                        std::ceil(pixels_per_unit * 0.68f), std::ceil(pixels_per_unit * 0.68f)})) {
          continue;
        }
      }
      if (object->name == "Tuzora rooftop shade awning" &&
          drawSprite(
              canvas, assets, "tuzora_shade_cloth",
              {rect.x, rect.y - pixels_per_unit * 0.24f, rect.width, pixels_per_unit * 0.70f})) {
        continue;
      }
      const std::string_view key = spriteForObjectName(object->name);
      if (!key.empty() && drawSprite(canvas, assets, key, rect)) {
        continue;
      }
    }
    if (startsWith(object->name, "Tuzora terrain tile") ||
        startsWith(object->name, "Tuzora placed block")) {
      canvas.fillRect({rect.x + 2.0f, rect.y + 2.0f, rect.width, rect.height},
                      {0.10f, 0.08f, 0.05f, 0.18f});
    }
    canvas.fillRect(rect, color);
  }
  if (night > 0.0f) {
    canvas.fillRect({0.0f, 0.0f, width, height}, {0.02f, 0.04f, 0.10f, night * 0.28f});
  }
}

void drawHud(aster::UiCanvas &canvas, const aster::Scene &scene,
             const aster::Camera2DState &camera_state, const aster::Camera2DConfig &camera_config,
             const PixelAssets &assets, const aster::TuzoraHudModel &model,
             const aster::ControlSnapshot &input, const int width, const int height) {
  canvas.beginFrame({static_cast<float>(width), static_cast<float>(height)}, input);
  const float w = static_cast<float>(width);
  const float h = static_cast<float>(height);
  drawCoastSpriteWorld(canvas, scene, camera_state, camera_config, assets, w, h,
                       model.day_fraction);

  canvas.fillRect({0.0f, 0.0f, w, 78.0f}, {0.06f, 0.07f, 0.08f, 0.58f});
  canvas.fillRect({0.0f, 74.0f, w, 2.0f}, {1.0f, 0.54f, 0.18f, 0.42f});
  canvas.text(model.title, {20.0f, 15.0f}, {1.0f, 0.74f, 0.32f, 1.0f}, 1.74f);
  canvas.text(model.objective, {20.0f, 47.0f}, {0.94f, 0.86f, 0.70f, 1.0f}, 1.03f);

  const float status_width = canvas.textWidth(model.status_line, 1.16f);
  canvas.fillRect({w - status_width - 44.0f, 18.0f, status_width + 24.0f, 31.0f},
                  model.signal_lit ? aster::UiColor{0.48f, 0.22f, 0.04f, 0.74f}
                                   : aster::UiColor{0.05f, 0.10f, 0.12f, 0.70f});
  canvas.text(model.status_line, {w - status_width - 32.0f, 27.0f},
              model.signal_lit ? aster::UiColor{1.0f, 0.78f, 0.26f, 1.0f}
                               : aster::UiColor{0.78f, 0.92f, 0.90f, 1.0f},
              1.16f);
  canvas.progressBar({w - 328.0f, 54.0f, 296.0f, 10.0f}, model.day_fraction,
                     {1.0f, 0.54f, 0.18f, 0.95f}, {0.06f, 0.08f, 0.12f, 0.90f});

  const float bottom_h = 72.0f;
  canvas.fillRect({0.0f, h - bottom_h, w, bottom_h}, {0.025f, 0.030f, 0.034f, 0.66f});
  canvas.text(model.resource_line, {20.0f, h - 54.0f}, {0.90f, 0.84f, 0.68f, 1.0f}, 1.02f);
  canvas.text(model.hotbar_line, {20.0f, h - 27.0f}, {0.72f, 0.94f, 0.94f, 1.0f}, 1.05f);
  const float event_width = std::min(canvas.textWidth(model.event_line, 1.18f), w - 60.0f);
  canvas.fillRect({(w - event_width) * 0.5f - 14.0f, h - 118.0f, event_width + 28.0f, 30.0f},
                  {0.06f, 0.08f, 0.08f, 0.46f});
  canvas.text(model.event_line, {(w - event_width) * 0.5f, h - 110.0f}, {1.0f, 0.78f, 0.48f, 1.0f},
              1.18f);

  if (model.ended) {
    const char *label = model.signal_lit ? "ROOFTOP SIGNAL" : "NIGHTFALL";
    const aster::UiColor color = model.signal_lit ? aster::UiColor{1.0f, 0.74f, 0.20f, 1.0f}
                                                  : aster::UiColor{0.62f, 0.72f, 0.98f, 1.0f};
    const float text_width = canvas.textWidth(label, 2.15f);
    canvas.fillRect({0.0f, h * 0.42f - 34.0f, w, 92.0f}, {0.02f, 0.03f, 0.04f, 0.62f});
    canvas.text(label, {(w - text_width) * 0.5f, h * 0.42f}, color, 2.15f);
  }
  canvas.endFrame();
}

std::filesystem::path framePath(const std::filesystem::path &directory, const int frame) {
  std::ostringstream name;
  name << "frame_";
  name.width(4);
  name.fill('0');
  name << frame << ".ppm";
  return directory / name.str();
}

void writeCapture(const std::filesystem::path &path, const int width, const int height) {
  if (!path.parent_path().empty()) {
    std::filesystem::create_directories(path.parent_path());
  }
  if (path.extension() == ".png") {
    aster::writeFramebufferPng(path, width, height);
  } else {
    aster::writeFramebufferPpm(path, width, height);
  }
}

bool runReplaySelfTest(const std::uint32_t seed, const int ticks) {
  aster::Tuzora local({.seed = seed});
  aster::Tuzora remote({.seed = seed});
  aster::net::LockstepCommandChannel outgoing(91u);
  aster::net::LockstepCommandChannel incoming(91u);
  for (int tick = 0; tick < ticks && local.status().outcome == aster::TuzoraOutcome::Playing;
       ++tick) {
    const aster::SimCommand command = local.scriptedCommand(local.status().tick);
    const std::uint32_t command_tick = command.tick;
    outgoing.pushLocal(command);
    const aster::net::NetMessage message = outgoing.buildMessage(1u, 2u, 1u);
    if (!incoming.receive(message)) {
      return false;
    }
    const std::optional<aster::SimCommand> received = incoming.commandForTick(command_tick);
    if (!received.has_value()) {
      return false;
    }
    local.updateFixed(command);
    remote.updateFixed(*received);
    if (local.worldHash() != remote.worldHash()) {
      return false;
    }
  }
  std::cout << "Tuzora replay self-test passed: ticks=" << ticks << " hash=0x" << std::hex
            << local.worldHash() << std::dec << " checksum=" << local.replay().checksum() << '\n';
  return true;
}

} // namespace

int main(int argc, char **argv) {
  try {
    const bool replay_self_test = hasArgument(argc, argv, "--replay-self-test");
    const bool smoke_test = hasArgument(argc, argv, "--smoke-test");
    const bool frame_report = hasArgument(argc, argv, "--frame-report");
    const bool no_vsync = hasArgument(argc, argv, "--no-vsync");
    const bool debug_tiles = hasArgument(argc, argv, "--debug-tiles");
    const std::uint32_t seed = argumentU32(argc, argv, "--seed", 0x7A2026u);
    const int replay_ticks = argumentInt(argc, argv, "--ticks", 720);
    std::filesystem::path project_path = argumentPath(argc, argv, "--project");
    if (project_path.empty()) {
      project_path = defaultProjectPath();
    }
    if (replay_self_test) {
      return runReplaySelfTest(seed, replay_ticks) ? 0 : 2;
    }
    const PixelAssets pixel_assets = loadPixelAssets(project_path);
    if (!hasRequiredVisualAssets(pixel_assets)) {
      std::cerr << "Tuzora visual assets are incomplete under "
                << (projectRootFromPath(project_path) / "textures") << '\n';
      return 3;
    }

    const std::filesystem::path screenshot = argumentPath(argc, argv, "--screenshot");
    const int screenshot_frame = argumentInt(argc, argv, "--screenshot-frame", 24);
    const std::filesystem::path sequence_dir = argumentPath(argc, argv, "--capture-sequence");
    const int capture_frames = argumentInt(argc, argv, "--capture-frames", 90);
    const int run_frames =
        argumentInt(argc, argv, "--run-frames",
                    smoke_test ? 18 : (screenshot.empty() ? 0 : screenshot_frame + 1));
    const bool scripted = smoke_test || !screenshot.empty() || !sequence_dir.empty();

    aster::EngineConfig config;
    config.application_name = "Tuzora";
    config.initial_width = argumentInt(argc, argv, "--window-width", 1600);
    config.initial_height = argumentInt(argc, argv, "--window-height", 900);
    config.multisample_samples = argumentInt(argc, argv, "--msaa", 1);
    config.enable_vsync = !no_vsync && !scripted && !frame_report;

    aster::Window window(config);
    aster::RenderDevice renderer;
    renderer.initialize();
    aster::UiCanvas hud;
    hud.initialize();

    aster::TuzoraTuning tuning;
    tuning.seed = seed;
    tuning.debug_tiles = debug_tiles;
    aster::Tuzora game(tuning);
    renderer.prepareScene(game.scene());
    const aster::ControlScheme control_scheme = makeControls();
    aster::ControlState controls;
    aster::FixedTimestep simulation_clock(
        {.step_seconds = 1.0 / 60.0, .max_frame_seconds = 1.0 / 20.0, .max_steps_per_frame = 4u});
    aster::Clock clock;
    const aster::Camera2DConfig camera_config = tuzoraCameraConfig();
    aster::Camera2DState camera_state;
    aster::resetCamera2D(camera_state, game.cameraTarget());
    aster::RendererSettings settings = tuzoraRendererSettings();
    if (!sequence_dir.empty()) {
      std::filesystem::create_directories(sequence_dir);
    }

    int rendered_frames = 0;
    double elapsed = 0.0;
    double frame_seconds_sum = 0.0;
    double draw_call_sum = 0.0;
    double visible_object_sum = 0.0;
    int previous_blocks_broken = 0;
    int previous_blocks_placed = 0;
    bool previous_signal_lit = false;
    bool captured = false;
    while (window.isOpen()) {
      window.pollEvents();
      const double dt = scripted ? 1.0 / 60.0 : clock.tick();
      elapsed += dt;
      controls.update(control_scheme, window.captureControls(control_scheme));
      if (controls.justPressed(kQuit)) {
        window.requestClose();
      }
      const bool ended = game.status().outcome != aster::TuzoraOutcome::Playing;
      if (controls.justPressed(kReset) || (ended && controls.justPressed(kInteract))) {
        game.reset();
        renderer.prepareScene(game.scene());
        aster::resetCamera2D(camera_state, game.cameraTarget());
        simulation_clock.reset();
        previous_blocks_broken = 0;
        previous_blocks_placed = 0;
        previous_signal_lit = false;
      }

      const std::size_t steps = simulation_clock.advance(dt);
      for (std::size_t step = 0; step < steps; ++step) {
        const aster::SimCommand command =
            scripted ? game.scriptedCommand(game.status().tick) : commandFromControls(controls);
        game.updateFixed(command);
        if (game.status().blocks_broken > previous_blocks_broken) {
          aster::addCameraShake2D(camera_state, {0.11f, 0.02f}, 0.30f);
        }
        if (game.status().blocks_placed > previous_blocks_placed) {
          aster::addCameraShake2D(camera_state, {-0.05f, 0.01f}, 0.16f);
        }
        if (game.status().signal_lit && !previous_signal_lit) {
          aster::addCameraShake2D(camera_state, {0.08f, 0.06f}, 0.22f);
        }
        previous_blocks_broken = game.status().blocks_broken;
        previous_blocks_placed = game.status().blocks_placed;
        previous_signal_lit = game.status().signal_lit;
        aster::updateCamera2D(camera_state, game.cameraTarget(), camera_config);
        renderer.prepareScene(game.scene());
      }

      const auto [width, height] = window.framebufferSize();
      const aster::FrameStats stats =
          renderer.render(game.scene(), aster::makeCamera2DOrbit(camera_state, camera_config),
                          settings, width, height, elapsed);
      frame_seconds_sum += stats.frame_seconds;
      draw_call_sum += static_cast<double>(stats.draw_calls);
      visible_object_sum += static_cast<double>(stats.visible_objects);

      const auto [hud_width, hud_height] = window.windowSize();
      drawHud(hud, game.scene(), camera_state, camera_config, pixel_assets, game.hudModel(),
              controls.snapshot(), hud_width, hud_height);

      if (!sequence_dir.empty()) {
        aster::writeFramebufferPpm(framePath(sequence_dir, rendered_frames), width, height);
        if (rendered_frames + 1 >= capture_frames) {
          window.requestClose();
        }
      } else if (!screenshot.empty() && !captured && rendered_frames >= screenshot_frame) {
        writeCapture(screenshot, width, height);
        captured = true;
        window.requestClose();
      }

      window.swapBuffers();
      ++rendered_frames;
      if (smoke_test && rendered_frames >= 18) {
        window.requestClose();
      }
      if (run_frames > 0 && rendered_frames >= run_frames) {
        window.requestClose();
      }
    }
    hud.shutdown();
    if (frame_report && rendered_frames > 0) {
      const double frames = static_cast<double>(rendered_frames);
      std::cout << "Tuzora frame report: frames=" << rendered_frames
                << " mean_frame_ms=" << (frame_seconds_sum / frames) * 1000.0
                << " visible_objects_mean=" << visible_object_sum / frames
                << " draw_calls_mean=" << draw_call_sum / frames << " world_hash=0x" << std::hex
                << game.worldHash() << std::dec << '\n';
    }
    if (smoke_test) {
      std::cout << "Tuzora smoke test passed: frames=" << rendered_frames << " world_hash=0x"
                << std::hex << game.worldHash() << std::dec << '\n';
    }
    return 0;
  } catch (const std::exception &error) {
    std::cerr << "Tuzora error: " << error.what() << '\n';
    return 1;
  }
}
