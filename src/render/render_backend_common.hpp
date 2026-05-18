// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/render/render_device.hpp"
#include "native_render_backend.hpp"

namespace aster::render_backend {

struct LocalBounds {
  Vec3 min{};
  Vec3 max{};
};

[[nodiscard]] bool containsViewerCullVolume(const ViewerCullVolume &volume, Vec3 point);
[[nodiscard]] FaceCullMode objectCullMode(const RenderObject &object, Vec3 camera_position,
                                          bool pipeline_back_face_culling);
[[nodiscard]] bool isContactShadowUtility(const RenderObject &object);
[[nodiscard]] LocalBounds primitiveLocalBounds(MeshPrimitive primitive);
[[nodiscard]] LocalBounds customMeshLocalBounds(const CpuMesh &mesh);
[[nodiscard]] LocalBounds objectLocalBounds(const RenderObject &object);
[[nodiscard]] bool canCastContactShadow(const RenderObject &object,
                                        const GroundingSettings &grounding);
[[nodiscard]] RenderObject contactShadowObjectFor(const RenderObject &object,
                                                  const GroundingSettings &grounding);
[[nodiscard]] const CpuMesh *meshForObject(const RenderObject &object,
                                           const PreparedRenderMeshes &meshes);

} // namespace aster::render_backend
