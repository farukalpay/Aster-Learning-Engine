// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/kernel/abi.h"

#include <new>

constexpr uint32_t kEngineMagic = 0x41535452u;

struct AsterEngineHandle__ {
  uint32_t magic = kEngineMagic;
  AsterStatus last_status{sizeof(AsterStatus), ASTER_KERNEL_STRUCT_VERSION_1, ASTER_STATUS_OK,
                          "ok"};
};

namespace {

AsterStatus makeStatus(const AsterStatusCode code, const char *message) {
  return {sizeof(AsterStatus), ASTER_KERNEL_STRUCT_VERSION_1, code, message};
}

bool validEngine(const AsterEngineHandle engine) {
  return engine != nullptr && engine->magic == kEngineMagic;
}

void setLastStatus(const AsterEngineHandle engine, const AsterStatus status) {
  if (validEngine(engine)) {
    engine->last_status = status;
  }
}

AsterStatus destroyOpaqueHandle(void *handle) {
  if (handle == nullptr) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "handle is null");
  }
  return makeStatus(ASTER_STATUS_UNSUPPORTED, "handle type is declared but not constructible yet");
}

} // namespace

extern "C" {

AsterAbiVersion aster_kernel_abi_version(void) {
  return {ASTER_KERNEL_ABI_MAJOR, ASTER_KERNEL_ABI_MINOR, ASTER_KERNEL_ABI_PATCH};
}

AsterStatus aster_kernel_status_ok(void) {
  return makeStatus(ASTER_STATUS_OK, "ok");
}

AsterStatus aster_kernel_status_from_code(const AsterStatusCode code) {
  switch (code) {
  case ASTER_STATUS_OK:
    return makeStatus(ASTER_STATUS_OK, "ok");
  case ASTER_STATUS_INVALID_ARGUMENT:
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "invalid argument");
  case ASTER_STATUS_UNSUPPORTED:
    return makeStatus(ASTER_STATUS_UNSUPPORTED, "unsupported");
  case ASTER_STATUS_OUT_OF_MEMORY:
    return makeStatus(ASTER_STATUS_OUT_OF_MEMORY, "out of memory");
  case ASTER_STATUS_ABI_MISMATCH:
    return makeStatus(ASTER_STATUS_ABI_MISMATCH, "ABI mismatch");
  case ASTER_STATUS_INTERNAL_ERROR:
  default:
    return makeStatus(ASTER_STATUS_INTERNAL_ERROR, "internal error");
  }
}

AsterStatus aster_kernel_engine_create(const AsterEngineDesc *desc,
                                       AsterEngineHandle *out_engine) {
  if (out_engine == nullptr) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "out_engine is null");
  }
  *out_engine = nullptr;
  if (desc == nullptr) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "engine descriptor is null");
  }
  if (desc->size < sizeof(AsterEngineDesc) || desc->version != ASTER_KERNEL_STRUCT_VERSION_1) {
    return makeStatus(ASTER_STATUS_ABI_MISMATCH, "engine descriptor version is not supported");
  }
  if (desc->application_name.data == nullptr && desc->application_name.size != 0u) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "application_name has a size but no data");
  }

  try {
    *out_engine = new AsterEngineHandle__();
  } catch (const std::bad_alloc &) {
    return makeStatus(ASTER_STATUS_OUT_OF_MEMORY, "engine allocation failed");
  } catch (...) {
    return makeStatus(ASTER_STATUS_INTERNAL_ERROR, "engine creation failed");
  }
  setLastStatus(*out_engine, aster_kernel_status_ok());
  return aster_kernel_status_ok();
}

AsterStatus aster_kernel_engine_destroy(const AsterEngineHandle engine) {
  if (!validEngine(engine)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "engine handle is invalid");
  }
  engine->magic = 0u;
  delete engine;
  return aster_kernel_status_ok();
}

AsterStatus aster_kernel_engine_last_status(const AsterEngineHandle engine) {
  if (!validEngine(engine)) {
    return makeStatus(ASTER_STATUS_INVALID_ARGUMENT, "engine handle is invalid");
  }
  return engine->last_status;
}

AsterStatus aster_kernel_window_destroy(const AsterWindowHandle window) {
  return destroyOpaqueHandle(window);
}

AsterStatus aster_kernel_scene_destroy(const AsterSceneHandle scene) {
  return destroyOpaqueHandle(scene);
}

AsterStatus aster_kernel_renderer_destroy(const AsterRendererHandle renderer) {
  return destroyOpaqueHandle(renderer);
}

AsterStatus aster_kernel_mesh_destroy(const AsterMeshHandle mesh) {
  return destroyOpaqueHandle(mesh);
}

AsterStatus aster_kernel_material_destroy(const AsterMaterialHandle material) {
  return destroyOpaqueHandle(material);
}

AsterStatus aster_kernel_physics_world_destroy(const AsterPhysicsWorldHandle physics_world) {
  return destroyOpaqueHandle(physics_world);
}

AsterStatus aster_kernel_system_world_destroy(const AsterSystemWorldHandle system_world) {
  return destroyOpaqueHandle(system_world);
}

AsterStatus aster_kernel_sample_app_destroy(const AsterSampleAppHandle sample_app) {
  return destroyOpaqueHandle(sample_app);
}

} // extern "C"
