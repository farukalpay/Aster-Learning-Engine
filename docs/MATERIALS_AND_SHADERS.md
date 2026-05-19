# Materials And Shaders

Materials enter through `.astermat` authoring files, future material graph
packages, or C++ `Material` values. `aster_materialc` owns the single-material
contract: parse the material grammar, validate texture roles and color spaces,
cook runtime KTX2 texture outputs through `aster_texturec` behavior, write
`.materialbin` records, emit reports, and create tiny preview frames only for
materials that pass validation. `aster_assetc cook` orchestrates those contracts
for full projects and asset databases.

Strict material cook is the default. The cook still writes the project asset
database and per-asset reports when diagnostics are present, but the process
returns nonzero if any asset has an error. A material with errors stays in the
asset database with diagnostics and dependencies only; it does not emit
renderer-visible `.materialbin`, cooked texture outputs, or previews.

Single-material validation is available through:

```bash
cargo run -p aster_assetc --bin aster_materialc -- inspect --input path/to/material.astermat --asset-root path/to/assets
cargo run -p aster_assetc --bin aster_materialc -- package --input path/to/material.astermat --asset-root path/to/assets --output /tmp/material_package
cargo run -p aster_assetc -- material-inspect --input path/to/material.astermat --asset-root path/to/assets
```

Single-texture validation and package output is available through:

```bash
cargo run -p aster_assetc --bin aster_texturec -- inspect --input path/to/albedo.ktx2 --role albedo
cargo run -p aster_assetc --bin aster_texturec -- package --input path/to/albedo.ktx2 --role albedo --output /tmp/texture_package
```

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
- LitPBR runtime materials require `albedo`, `normal`, and `orm`. Authoring may
  provide an existing `orm` texture or a complete split source set of
  `roughness`, `metallic`, and `ao` that the configured encoder can pack into
  ORM. Incomplete split ORM sources are errors.
- Role color spaces are enforced: `albedo` and `emissive` are sRGB; `normal`,
  `orm`, `height`, `wetness`, `opacity`, and mask-like data are linear/non-color.
  KTX2 headers are checked against the expected runtime format before a material
  is allowed to cook.
- Feature flags for triplanar, normal maps, height/parallax, alpha, fog, shadow,
  decals, and instancing.
- Typed `MaterialGraphNode` operations while preserving raw operation text for
  diagnostics.
- `CompiledMaterialAsset` carries the renderer-visible material graph next to
  the fallback runtime material and shader variant key, keeping `SurfacePattern`
  as legacy/procedural fallback rather than the primary authoring interface.

Cooked material records include texture source hash, cooked hash, source format,
runtime format, dimensions, mip count, byte cost, color-space decision,
encoder/backend, shader variant key, fallback reason, platform compatibility,
dependencies, and diagnostics. Runtime fallback textures remain available for
explicit preview/debug paths. Production cooked materials loaded with
`require_existing_files = true` reject missing or fallback-bound required roles.

Non-KTX2 source image cooking requires `ASTER_TEXTURE_ENCODER` to point at a
real encoder command. The encoder is invoked with `--input`, `--output`,
`--role`, `--color-space`, and `--mips` for direct texture cooks, and with
`--pack-orm`, `--roughness`, `--metallic`, `--ao`, and `--output` for split ORM
packing. If the encoder is unavailable or writes invalid KTX2 output, strict
cook fails instead of writing fake compressed content.

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
