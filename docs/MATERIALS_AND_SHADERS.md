# Materials And Shaders

Materials enter through `.astermat` authoring files or C++ `Material` values.
The parser validates texture roles, parameters, feature flags, and material
layers, then produces a fallback runtime material and a shader variant key.

Current material contracts:

- Lit PBR, emissive/unlit intent, opaque/masked/blend modes, cull modes, decal
  and shadow participation.
- Runtime color inputs are semantic: `MaterialDesc::base_color` is `LinearRgb`
  and `MaterialDesc::emission_color` is `EmissionColor`. Importers that read
  artist-facing sRGB values convert them with `srgbToLinear` before building
  renderer materials.
- Texture roles for albedo, normal, ORM, roughness, metallic, AO, height,
  emissive, wetness, and opacity. Runtime binding uses a fixed role table so
  software, Metal, and D3D12 can report the same material binding trace.
- Feature flags for triplanar, normal maps, height/parallax, alpha, fog, shadow,
  decals, and instancing.
- Typed `MaterialGraphNode` operations while preserving raw operation text for
  diagnostics.
- `CompiledMaterialAsset` carries the renderer-visible material graph next to
  the fallback runtime material and shader variant key, keeping `SurfacePattern`
  as legacy/procedural fallback rather than the primary authoring interface.

Shader library modules live in `shaders/lib`. The v1 utility set includes BRDF,
PBR, tonemap, fog, triplanar, normal mapping, clearcoat, wetness, parallax,
detail normals, alpha helpers, and material debug outputs.

Frame forensics expose material binding status per visible object and role:
valid/fallback/bound, dimensions, mip count, and descriptor layout hash. Debug
view names exist for base color, normal, roughness, metallic, AO, emissive, UV,
mip level, overdraw, light clusters, shadow mask, fog, and reflection probes.
Software frame captures now carry RGBA payloads plus content hashes for the
final frame and reference shadow/fog/probe resources. Mesh visibility traces and
object cluster membership traces sit next to the material binding trace so a
debugger can connect material, visibility, and lighting decisions.
