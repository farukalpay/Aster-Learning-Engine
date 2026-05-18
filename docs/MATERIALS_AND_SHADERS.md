# Materials And Shaders

Materials enter through `.astermat` authoring files or C++ `Material` values.
The parser validates texture roles, parameters, feature flags, and material
layers, then produces a fallback runtime material and a shader variant key.

Current material contracts:

- Lit PBR, emissive/unlit intent, opaque/masked/blend modes, cull modes, decal
  and shadow participation.
- Texture roles for albedo, normal, ORM, height, emissive, and wetness.
- Feature flags for triplanar, normal maps, height/parallax, alpha, fog, shadow,
  decals, and instancing.
- Typed `MaterialGraphNode` operations while preserving raw operation text for
  diagnostics.

Shader library modules live in `shaders/lib`. The v1 utility set includes BRDF,
PBR, tonemap, fog, triplanar, normal mapping, clearcoat, wetness, parallax,
detail normals, alpha helpers, and material debug outputs.
