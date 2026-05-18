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
- Texture roles for albedo, normal, ORM, height, emissive, and wetness.
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
