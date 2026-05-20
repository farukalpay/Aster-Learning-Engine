# Materials And Shaders

Materials enter through `.astergraph` procedural asset graphs, legacy `.astermat`
authoring files, or C++ `Material` values. `.astergraph` is the canonical V1
authoring kernel for graph-authored runtime procedural materials: it records a
stable graph GUID, node IDs, dependency edges, procedural material IR, mesh and
collision descriptors, shader and pipeline keys, quality score, and per-node
diagnostics. `.astermat` remains supported as a legacy/import path.

`aster_materialc` owns the single-material `.astermat` contract: parse the
material grammar, validate texture roles and color spaces, cook runtime KTX2
texture outputs through `aster_texturec` behavior, write `.materialbin` records,
emit reports, and create tiny preview frames only for materials that pass
validation. `aster_assetc cook` orchestrates `.astergraph`, material, texture,
scene, and project contracts for full projects and asset databases.

Strict material cook is the default. The cook still writes the project asset
database and per-asset reports when diagnostics are present, but the process
returns nonzero if any asset has an error. A material with errors stays in the
asset database with diagnostics and dependencies only; it does not emit
renderer-visible `.materialbin`, cooked texture outputs, or previews.

Single-material validation is available through:

```bash
cargo run -p aster_assetc --bin aster_materialc -- inspect --input path/to/material.astermat --asset-root path/to/assets
cargo run -p aster_assetc --bin aster_materialc -- package --input path/to/material.astermat --asset-root path/to/assets --output /tmp/material_package
cargo run -p aster_assetc --bin aster_assetc -- material-inspect --input path/to/material.astermat --asset-root path/to/assets
```

Single-graph validation and package output is available through:

```bash
cargo run -p aster_assetc --bin aster_assetc -- graph-inspect --input path/to/material.astergraph
cargo run -p aster_assetc --bin aster_assetc -- graph-package --input path/to/material.astergraph --output /tmp/asset_graph_package
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
- Typed `.astergraph` nodes for mesh primitive/descriptors, boolean or carve
  intent, bevel/fracture intent, UV policy, tangent validation, material
  assignment, procedural material generators, collision proxy, LOD, probe helper,
  prefab variant, cook/export, and diagnostics. Complex mesh operators are V1
  descriptors plus diagnostics; Material Lab procedural material execution is
  the renderer acceptance path.
- Runtime procedural material nodes include noise, cellular, slope, curvature,
  cavity, edge wear, wetness flow, rust spread, moss growth, decal layering, ORM
  pack/unpack intent, normal, and height. They feed shared runtime/reference
  metadata rather than requiring mandatory baked textures in V1.

## Surface Fidelity Gate

Feature presence is not treated as visual quality by itself. `RenderQualityProfile`
now carries a surface-fidelity policy that asks whether a LitPBR material can
survive the conditions that players actually see: energy-conserving BSDF intent,
environment/reflection response, nonzero area-light radius, shadow filtering,
tangent-space policy for normal maps, physical texel-density metadata,
height-normal coupling, roughness-height coupling, macro/micro frequency
breakup, temporal stability budget for height or micro-detail, and
artist-facing preview rig metadata.

`evaluateMaterialQuality` reports those checks under the `surface-fidelity`
category. Material Lab separates them from generic cook issues so an artist sees
the image-facing failure mode directly: missing tangent-space policy, missing
motion-aliasing budget, missing preview environment, or a profile that disables
IBL/shadow filtering. The report still keeps texture role, color-space, mip,
compression, descriptor, and provenance data, but those details are support
evidence rather than the definition of quality.

Applying a production or cinematic `RenderQualityProfile` also gives preview and
runtime light rigs a minimum source radius, surface occlusion/contact-hardening
settings, lens/composition defaults, and physical texel-density floors, so
area-light response and scale reading are not only labels in the material
report. Texture sampling keeps full mip-chain checks and runtime anisotropy
policy in the texture/runtime path; normal-mapped materials must declare how
their tangent basis is authored or generated before the audit can be considered
quiet.

## Hero Material Suite

`.astergraph` and `.astermat` content should converge on one artist-visible
material truth: the material is accepted only when the preview, runtime binding
trace, shader variant, fallback reason, mip behavior, and backend diff agree
with the intended surface. The canonical suite is:

- Rusted pipe: `showcases/pipe_lab/rusted_pipe.astergraph`.
- Wet rock: `showcases/material_lab/wet_rock.astermat` and
  `showcases/material_lab/procedural_wet_rock.astergraph`.
- Cave wall or moss: `showcases/material_lab/procedural_cave_moss.astergraph`.
- Wet decal and soot layering:
  `showcases/material_lab/procedural_wet_decal_soot.astergraph`.
- Biological surface: `showcases/primate_lab/cercopithecidae.astergraph`.

Each material in that suite must carry albedo, normal, ORM, height, wetness,
roughness-response evidence, physical texel density, height/normal/roughness
coupling, macro/micro frequency breakup, and cavity/edge response proof; a named
debug view; the selected shader variant; a fallback or no-fallback reason;
mip-chain and anisotropy behavior; and a software/Metal/D3D12 backend-diff
record. A material that only validates roles and color spaces but cannot explain
the visible response is not production ready.

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
source path, texture kind, color space, fallback/degrade reason,
valid/fallback/bound, dimensions, mip count, and descriptor layout hash. Asset
frame traces connect the render object back to source asset, source node, source
mesh, material slot, mesh import diagnostics, texture roles, backend
degradations, source graph GUID, graph node ID, shader variant key, pipeline
cache key, procedural capability status, residency/fallback reason, and native
or reference backend status. A bad frame should therefore say whether the
failure came from a missing UV channel, generated or missing tangent basis,
incomplete mip chain, wrong color space, fallback texture, unknown texture role,
backend sampling degrade, unsupported procedural node, or procedural reference
path. Debug view names exist for base color, normal, roughness, metallic, AO,
emissive, UV, mip level, overdraw, light clusters, surface attributes, surface
occlusion, shadow mask, fog, and reflection probes. Software frame captures now
carry RGBA payloads plus content hashes for the final frame and reference
surface-attribute/surface-occlusion/shadow/fog/probe resources. Mesh
visibility traces and object cluster membership traces sit next to the material
binding and asset traces so a debugger can connect material, visibility,
lighting, and asset-production decisions.
