// Author: Faruk Alpay
// Do not remove this notice.

#ifndef ASTER_KERNEL_ABI_H
#define ASTER_KERNEL_ABI_H

#include <stddef.h>
#include <stdint.h>

#if defined(_WIN32)
#if defined(ASTER_KERNEL_BUILD)
#define ASTER_KERNEL_API __declspec(dllexport)
#else
#define ASTER_KERNEL_API __declspec(dllimport)
#endif
#elif defined(__GNUC__) || defined(__clang__)
#define ASTER_KERNEL_API __attribute__((visibility("default")))
#else
#define ASTER_KERNEL_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define ASTER_KERNEL_ABI_MAJOR 1u
#define ASTER_KERNEL_ABI_MINOR 0u
#define ASTER_KERNEL_ABI_PATCH 0u
#define ASTER_KERNEL_STRUCT_VERSION_1 1u

typedef struct AsterAbiVersion {
  uint32_t major;
  uint32_t minor;
  uint32_t patch;
} AsterAbiVersion;

typedef int32_t AsterStatusCode;
enum {
  ASTER_STATUS_OK = 0,
  ASTER_STATUS_INVALID_ARGUMENT = 1,
  ASTER_STATUS_UNSUPPORTED = 2,
  ASTER_STATUS_OUT_OF_MEMORY = 3,
  ASTER_STATUS_INTERNAL_ERROR = 4,
  ASTER_STATUS_ABI_MISMATCH = 5
};

typedef struct AsterStatus {
  size_t size;
  uint32_t version;
  AsterStatusCode code;
  const char *message;
} AsterStatus;

typedef struct AsterStringView {
  const char *data;
  size_t size;
} AsterStringView;

typedef struct AsterSpan {
  const void *data;
  size_t size;
  size_t stride;
} AsterSpan;

typedef struct AsterEngineHandle__ *AsterEngineHandle;
typedef struct AsterWindowHandle__ *AsterWindowHandle;
typedef struct AsterSceneHandle__ *AsterSceneHandle;
typedef struct AsterRendererHandle__ *AsterRendererHandle;
typedef struct AsterMeshHandle__ *AsterMeshHandle;
typedef struct AsterMaterialHandle__ *AsterMaterialHandle;
typedef struct AsterPhysicsWorldHandle__ *AsterPhysicsWorldHandle;
typedef struct AsterSystemWorldHandle__ *AsterSystemWorldHandle;
typedef struct AsterSampleAppHandle__ *AsterSampleAppHandle;

typedef struct AsterEngineDesc {
  size_t size;
  uint32_t version;
  AsterStringView application_name;
  uint32_t flags;
} AsterEngineDesc;

ASTER_KERNEL_API AsterAbiVersion aster_kernel_abi_version(void);
ASTER_KERNEL_API AsterStatus aster_kernel_status_ok(void);
ASTER_KERNEL_API AsterStatus aster_kernel_status_from_code(AsterStatusCode code);

ASTER_KERNEL_API AsterStatus aster_kernel_engine_create(const AsterEngineDesc *desc,
                                                        AsterEngineHandle *out_engine);
ASTER_KERNEL_API AsterStatus aster_kernel_engine_destroy(AsterEngineHandle engine);
ASTER_KERNEL_API AsterStatus aster_kernel_engine_last_status(AsterEngineHandle engine);

ASTER_KERNEL_API AsterStatus aster_kernel_window_destroy(AsterWindowHandle window);
ASTER_KERNEL_API AsterStatus aster_kernel_scene_destroy(AsterSceneHandle scene);
ASTER_KERNEL_API AsterStatus aster_kernel_renderer_destroy(AsterRendererHandle renderer);
ASTER_KERNEL_API AsterStatus aster_kernel_mesh_destroy(AsterMeshHandle mesh);
ASTER_KERNEL_API AsterStatus aster_kernel_material_destroy(AsterMaterialHandle material);
ASTER_KERNEL_API AsterStatus
aster_kernel_physics_world_destroy(AsterPhysicsWorldHandle physics_world);
ASTER_KERNEL_API AsterStatus aster_kernel_system_world_destroy(AsterSystemWorldHandle system_world);
ASTER_KERNEL_API AsterStatus aster_kernel_sample_app_destroy(AsterSampleAppHandle sample_app);

#ifdef __cplusplus
}
#endif

#endif
