// Author: Faruk Alpay
// Do not remove this notice.

use serde::{Deserialize, Serialize};
use serde_json::Value;
use std::collections::{BTreeMap, HashMap};
use std::fmt;
use std::fs;
use std::path::{Path, PathBuf};

pub const CACHE_MAGIC: [u8; 8] = *b"ASTRCV1\0";
pub const CACHE_VERSION: u32 = 2;
pub const COMPILER_VERSION: u32 = 1;
pub const CACHE_ENDIAN_MARKER: u32 = 0x1234_5678;

const CHUNK_METADATA: u32 = 1;
const CHUNK_SCENE_GRAPH: u32 = 2;
const CHUNK_MATERIALS: u32 = 3;
const CHUNK_MESHES: u32 = 4;
const CHUNK_COLLISION: u32 = 5;
const HEADER_SIZE: usize = 88;
const CHUNK_ENTRY_SIZE: usize = 56;
const RELATIVE_AREA_TOLERANCE: f32 = f32::EPSILON * f32::EPSILON * 64.0;

#[derive(Debug, Clone)]
pub struct ContentError {
    message: String,
}

impl ContentError {
    pub fn new(message: impl Into<String>) -> Self {
        Self {
            message: message.into(),
        }
    }
}

impl fmt::Display for ContentError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.write_str(&self.message)
    }
}

impl std::error::Error for ContentError {}

impl From<std::io::Error> for ContentError {
    fn from(error: std::io::Error) -> Self {
        Self::new(error.to_string())
    }
}

impl From<serde_json::Error> for ContentError {
    fn from(error: serde_json::Error) -> Self {
        Self::new(error.to_string())
    }
}

pub type Result<T> = std::result::Result<T, ContentError>;

#[derive(Clone, Copy, Debug, Eq, PartialEq, Serialize, Deserialize)]
#[serde(rename_all = "kebab-case")]
pub enum OriginPolicy {
    Keep,
    Center,
    CenterOnGround,
}

impl OriginPolicy {
    pub fn parse(value: &str) -> Result<Self> {
        match value {
            "keep" => Ok(Self::Keep),
            "center" => Ok(Self::Center),
            "center-on-ground" => Ok(Self::CenterOnGround),
            _ => Err(ContentError::new(format!(
                "unsupported origin policy '{value}', expected keep, center, or center-on-ground"
            ))),
        }
    }

    pub fn as_str(self) -> &'static str {
        match self {
            Self::Keep => "keep",
            Self::Center => "center",
            Self::CenterOnGround => "center-on-ground",
        }
    }
}

#[derive(Clone, Copy, Debug, Serialize, Deserialize)]
pub struct CompileOptions {
    pub origin_policy: OriginPolicy,
    pub unit_scale: f32,
}

impl Default for CompileOptions {
    fn default() -> Self {
        Self {
            origin_policy: OriginPolicy::Keep,
            unit_scale: 1.0,
        }
    }
}

impl CompileOptions {
    fn canonical_bytes(&self) -> Vec<u8> {
        format!(
            "{{\"origin\":\"{}\",\"unit_scale_bits\":{}}}",
            self.origin_policy.as_str(),
            self.unit_scale.to_bits()
        )
        .into_bytes()
    }

    pub fn summary(&self) -> String {
        format!(
            "origin={},unit_scale={:.9}",
            self.origin_policy.as_str(),
            self.unit_scale
        )
    }
}

#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct Vec2 {
    pub x: f32,
    pub y: f32,
}

#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct Vec3 {
    pub x: f32,
    pub y: f32,
    pub z: f32,
}

#[derive(Clone, Copy, Debug, PartialEq)]
pub struct Vec4 {
    pub x: f32,
    pub y: f32,
    pub z: f32,
    pub w: f32,
}

impl Default for Vec4 {
    fn default() -> Self {
        Self {
            x: 0.0,
            y: 0.0,
            z: 0.0,
            w: 0.0,
        }
    }
}

impl std::ops::Add for Vec2 {
    type Output = Self;

    fn add(self, rhs: Self) -> Self::Output {
        Self {
            x: self.x + rhs.x,
            y: self.y + rhs.y,
        }
    }
}

impl std::ops::Sub for Vec2 {
    type Output = Self;

    fn sub(self, rhs: Self) -> Self::Output {
        Self {
            x: self.x - rhs.x,
            y: self.y - rhs.y,
        }
    }
}

impl std::ops::Add for Vec3 {
    type Output = Self;

    fn add(self, rhs: Self) -> Self::Output {
        Self {
            x: self.x + rhs.x,
            y: self.y + rhs.y,
            z: self.z + rhs.z,
        }
    }
}

impl std::ops::Sub for Vec3 {
    type Output = Self;

    fn sub(self, rhs: Self) -> Self::Output {
        Self {
            x: self.x - rhs.x,
            y: self.y - rhs.y,
            z: self.z - rhs.z,
        }
    }
}

impl std::ops::Mul<f32> for Vec3 {
    type Output = Self;

    fn mul(self, rhs: f32) -> Self::Output {
        Self {
            x: self.x * rhs,
            y: self.y * rhs,
            z: self.z * rhs,
        }
    }
}

impl std::ops::Div<f32> for Vec3 {
    type Output = Self;

    fn div(self, rhs: f32) -> Self::Output {
        Self {
            x: self.x / rhs,
            y: self.y / rhs,
            z: self.z / rhs,
        }
    }
}

#[derive(Clone, Copy, Debug, PartialEq)]
pub struct Mat4 {
    pub m: [f32; 16],
}

impl Default for Mat4 {
    fn default() -> Self {
        identity()
    }
}

#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct Bounds {
    pub min: Vec3,
    pub max: Vec3,
    pub valid: bool,
}

impl Bounds {
    fn empty() -> Self {
        Self {
            min: Vec3 {
                x: f32::MAX,
                y: f32::MAX,
                z: f32::MAX,
            },
            max: Vec3 {
                x: f32::MIN,
                y: f32::MIN,
                z: f32::MIN,
            },
            valid: false,
        }
    }

    fn include(&mut self, value: Vec3) {
        self.min.x = self.min.x.min(value.x);
        self.min.y = self.min.y.min(value.y);
        self.min.z = self.min.z.min(value.z);
        self.max.x = self.max.x.max(value.x);
        self.max.y = self.max.y.max(value.y);
        self.max.z = self.max.z.max(value.z);
        self.valid = true;
    }
}

#[derive(Clone, Copy, Debug, PartialEq)]
pub struct Vertex {
    pub position: Vec3,
    pub normal: Vec3,
    pub uv: Vec2,
    pub tangent: Vec4,
    pub ambient_occlusion: f32,
}

impl Default for Vertex {
    fn default() -> Self {
        Self {
            position: Vec3::default(),
            normal: Vec3::default(),
            uv: Vec2::default(),
            tangent: Vec4 {
                x: 1.0,
                y: 0.0,
                z: 0.0,
                w: 1.0,
            },
            ambient_occlusion: 1.0,
        }
    }
}

#[derive(Clone, Debug, Default, PartialEq)]
pub struct Mesh {
    pub vertices: Vec<Vertex>,
    pub indices: Vec<u32>,
}

#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub enum AlphaMode {
    #[default]
    Opaque = 0,
    Masked = 1,
    DitheredCoverage = 2,
    Blend = 3,
}

#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub enum DepthWrite {
    #[default]
    Auto = 0,
    Enabled = 1,
    Disabled = 2,
}

#[derive(Clone, Debug, Default, PartialEq)]
pub struct TextureDependency {
    pub role: String,
    pub uri: String,
    pub present: bool,
    pub hash: [u8; 32],
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum TextureImportKind {
    Unknown,
    Albedo,
    Normal,
    Roughness,
    Metallic,
    Occlusion,
    MetallicRoughness,
    Orm,
    Height,
    Emissive,
    Wetness,
    Opacity,
    Mask,
}

impl TextureImportKind {
    pub fn role(role: &str) -> Self {
        match role {
            "albedo" | "base_color" | "baseColor" => Self::Albedo,
            "normal" => Self::Normal,
            "roughness" => Self::Roughness,
            "metallic" => Self::Metallic,
            "ao" | "occlusion" | "ambient_occlusion" => Self::Occlusion,
            "metallic_roughness" => Self::MetallicRoughness,
            "orm" => Self::Orm,
            "height" | "displacement" => Self::Height,
            "emissive" => Self::Emissive,
            "wetness" => Self::Wetness,
            "opacity" | "alpha" => Self::Opacity,
            "moss" | "crack" | "mask" => Self::Mask,
            _ => Self::Unknown,
        }
    }

    pub fn as_str(self) -> &'static str {
        match self {
            Self::Unknown => "unknown",
            Self::Albedo => "albedo",
            Self::Normal => "normal",
            Self::Roughness => "roughness",
            Self::Metallic => "metallic",
            Self::Occlusion => "occlusion",
            Self::MetallicRoughness => "metallic_roughness",
            Self::Orm => "orm",
            Self::Height => "height",
            Self::Emissive => "emissive",
            Self::Wetness => "wetness",
            Self::Opacity => "opacity",
            Self::Mask => "mask",
        }
    }

    pub fn color_space(self) -> &'static str {
        match self {
            Self::Albedo | Self::Emissive => "srgb",
            _ => "linear",
        }
    }
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct TextureImportSummary {
    pub role: String,
    pub kind: TextureImportKind,
    pub color_space: String,
    pub width: u32,
    pub height: u32,
    pub mip_count: u32,
    pub format: String,
    pub source_hash: [u8; 32],
    pub diagnostics: Vec<String>,
}

pub const ASSET_DATABASE_SCHEMA_VERSION: u32 = 1;
pub const ASSET_IMPORT_SETTINGS_VERSION: u32 = 1;
pub const MATERIAL_BIN_SCHEMA_VERSION: u32 = 1;

#[derive(Clone, Debug, Serialize, Deserialize, PartialEq, Eq)]
pub struct AssetDependencyRecord {
    pub role: String,
    pub path: String,
    pub present: bool,
    pub hash: String,
}

#[derive(Clone, Debug, Serialize, Deserialize, PartialEq, Eq)]
pub struct AssetCookedOutput {
    pub role: String,
    pub kind: String,
    pub path: String,
    pub hash: String,
}

#[derive(Clone, Debug, Serialize, Deserialize, PartialEq, Eq)]
pub struct AssetCookDiagnostic {
    pub severity: String,
    pub message: String,
}

#[derive(Clone, Debug, Serialize, Deserialize, PartialEq, Eq)]
pub struct AssetDatabaseRecord {
    pub guid: String,
    pub id: String,
    pub kind: String,
    pub source_path: String,
    pub import_settings_version: u32,
    pub source_hash: String,
    pub options_hash: String,
    pub dependencies: Vec<AssetDependencyRecord>,
    pub outputs: Vec<AssetCookedOutput>,
    pub diagnostics: Vec<AssetCookDiagnostic>,
    pub platform: String,
}

#[derive(Clone, Debug, Serialize, Deserialize, PartialEq, Eq)]
pub struct AssetDatabase {
    pub schema_version: u32,
    pub platform: String,
    pub records: Vec<AssetDatabaseRecord>,
}

#[derive(Clone, Debug, Serialize, Deserialize, PartialEq, Eq)]
pub struct MaterialBinTextureRecord {
    pub role: String,
    pub source_path: String,
    pub cooked_path: String,
    pub kind: String,
    pub color_space: String,
    pub width: u32,
    pub height: u32,
    pub mip_count: u32,
    pub source_hash: String,
    pub cooked_hash: String,
    pub diagnostics: Vec<String>,
}

#[derive(Clone, Debug, Serialize, Deserialize, PartialEq)]
pub struct MaterialBinFallback {
    pub base_color: [f32; 3],
    pub emission_color: [f32; 3],
    pub roughness: f32,
    pub metallic: f32,
    pub emission_strength: f32,
    pub opacity: f32,
    pub double_sided: bool,
    pub alpha_mode: String,
    pub receives_shadows: bool,
    pub surface_profile: String,
}

#[derive(Clone, Debug, Serialize, Deserialize, PartialEq, Eq)]
pub struct MaterialBinBinding {
    pub name: String,
    pub kind: String,
    pub binding: u32,
}

#[derive(Clone, Debug, Serialize, Deserialize, PartialEq)]
pub struct MaterialBin {
    pub schema_version: u32,
    pub asset_guid: String,
    pub id: String,
    pub name: String,
    pub source_path: String,
    pub feature_mask: u64,
    pub shader_variant_key: u64,
    pub shader_variant_tag: String,
    pub pipeline_tag: String,
    pub fallback: MaterialBinFallback,
    pub params: BTreeMap<String, f32>,
    pub features: BTreeMap<String, bool>,
    pub textures: Vec<MaterialBinTextureRecord>,
    pub binding_layout: Vec<MaterialBinBinding>,
    pub dependency_hashes: Vec<AssetDependencyRecord>,
    pub diagnostics: Vec<AssetCookDiagnostic>,
}

#[derive(Clone, Debug, Serialize, Deserialize, PartialEq, Eq)]
pub struct TextureCookReport {
    pub schema_version: u32,
    pub role: String,
    pub kind: String,
    pub color_space: String,
    pub format: String,
    pub width: u32,
    pub height: u32,
    pub mip_count: u32,
    pub source_hash: String,
    pub cooked_hash: String,
    pub cooked_path: String,
    pub diagnostics: Vec<String>,
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct TextureCookResult {
    pub output_path: PathBuf,
    pub report_path: PathBuf,
    pub report: TextureCookReport,
}

#[derive(Clone, Debug, PartialEq)]
pub struct MaterialCookResult {
    pub material_bin_path: PathBuf,
    pub report_path: PathBuf,
    pub preview_path: PathBuf,
    pub material_bin: MaterialBin,
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct CookProjectResult {
    pub database: AssetDatabase,
    pub database_path: PathBuf,
}

#[derive(Clone, Debug, PartialEq)]
pub struct Material {
    pub name: String,
    pub base_color: Vec3,
    pub emission_color: Vec3,
    pub roughness: f32,
    pub metallic: f32,
    pub emission_strength: f32,
    pub opacity: f32,
    pub double_sided: bool,
    pub alpha_mode: AlphaMode,
    pub depth_write: DepthWrite,
    pub has_base_color_texture: bool,
    pub has_metallic_roughness_texture: bool,
    pub has_normal_texture: bool,
    pub has_occlusion_texture: bool,
    pub permutation_key: u64,
    pub permutation_flags: u32,
    pub pipeline_tag: String,
    pub texture_dependencies: Vec<TextureDependency>,
}

impl Default for Material {
    fn default() -> Self {
        Self {
            name: String::new(),
            base_color: Vec3 {
                x: 1.0,
                y: 1.0,
                z: 1.0,
            },
            emission_color: Vec3::default(),
            roughness: 0.55,
            metallic: 0.0,
            emission_strength: 0.0,
            opacity: 1.0,
            double_sided: false,
            alpha_mode: AlphaMode::Opaque,
            depth_write: DepthWrite::Auto,
            has_base_color_texture: false,
            has_metallic_roughness_texture: false,
            has_normal_texture: false,
            has_occlusion_texture: false,
            permutation_key: 0,
            permutation_flags: 0,
            pipeline_tag: String::new(),
            texture_dependencies: Vec::new(),
        }
    }
}

#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct MeshDiagnostics {
    pub input_vertices: u64,
    pub input_indices: u64,
    pub output_vertices: u64,
    pub output_indices: u64,
    pub degenerate_triangles: u64,
    pub invalid_normals: u64,
    pub generated_tangents: u64,
    pub remapped_vertices: u64,
}

impl MeshDiagnostics {
    fn add(&mut self, other: Self) {
        self.input_vertices += other.input_vertices;
        self.input_indices += other.input_indices;
        self.output_vertices += other.output_vertices;
        self.output_indices += other.output_indices;
        self.degenerate_triangles += other.degenerate_triangles;
        self.invalid_normals += other.invalid_normals;
        self.generated_tangents += other.generated_tangents;
        self.remapped_vertices += other.remapped_vertices;
    }
}

#[derive(Clone, Debug, PartialEq)]
pub struct MeshChunk {
    pub name: String,
    pub material_slot: u32,
    pub bounds: Bounds,
    pub diagnostics: MeshDiagnostics,
    pub mesh: Mesh,
}

#[derive(Clone, Debug, PartialEq)]
pub struct CollisionTriangle {
    pub a: Vec3,
    pub b: Vec3,
    pub c: Vec3,
    pub normal: Vec3,
}

#[derive(Clone, Debug, PartialEq)]
pub struct CollisionMesh {
    pub name: String,
    pub mesh_chunk: u32,
    pub bounds: Bounds,
    pub triangles: Vec<CollisionTriangle>,
}

#[derive(Clone, Debug, PartialEq)]
pub struct SceneNode {
    pub name: String,
    pub parent: i32,
    pub transform: Mat4,
    pub first_mesh: u32,
    pub mesh_count: u32,
}

#[derive(Clone, Debug, PartialEq)]
pub struct CacheMetadata {
    pub cache_version: u32,
    pub compiler_version: u32,
    pub source_hash: [u8; 32],
    pub options_hash: [u8; 32],
    pub compiler_options: String,
    pub material_count: u32,
    pub mesh_count: u32,
    pub collision_mesh_count: u32,
    pub scene_node_count: u32,
    pub total_vertices: u64,
    pub total_indices: u64,
    pub total_collision_triangles: u64,
    pub diagnostics: MeshDiagnostics,
}

#[derive(Clone, Debug, PartialEq)]
pub struct CompiledSceneAsset {
    pub metadata: CacheMetadata,
    pub scene_nodes: Vec<SceneNode>,
    pub materials: Vec<Material>,
    pub meshes: Vec<MeshChunk>,
    pub collision_meshes: Vec<CollisionMesh>,
}

#[derive(Clone, Debug)]
struct BufferView {
    buffer: usize,
    byte_offset: usize,
    byte_length: usize,
    byte_stride: usize,
}

#[derive(Clone, Debug)]
struct Accessor {
    buffer_view: usize,
    byte_offset: usize,
    component_type: u32,
    count: usize,
    ty: String,
}

#[derive(Clone, Debug)]
struct AssetData {
    root: Value,
    buffers: Vec<Vec<u8>>,
    buffer_uris: Vec<String>,
    views: Vec<BufferView>,
    accessors: Vec<Accessor>,
    base: PathBuf,
    source_bytes: Vec<u8>,
}

pub fn compile_scene_asset(
    path: impl AsRef<Path>,
    options: CompileOptions,
) -> Result<CompiledSceneAsset> {
    if !options.unit_scale.is_finite() || options.unit_scale <= 0.0 {
        return Err(ContentError::new(
            "unit scale must be finite and greater than zero",
        ));
    }
    let data = load_asset_data(path.as_ref())?;
    let source_hash = source_hash(&data)?;
    let options_hash = *blake3::hash(&options.canonical_bytes()).as_bytes();

    let mut materials = Vec::new();
    materials.push(default_material());
    if let Some(entries) = data.root.get("materials").and_then(Value::as_array) {
        for entry in entries {
            materials.push(import_material(&data, entry)?);
        }
    }

    let mut context = ImportContext {
        data: &data,
        options,
        scene_nodes: Vec::new(),
        meshes: Vec::new(),
    };

    let scene_index = data.root.get("scene").and_then(Value::as_u64).unwrap_or(0) as usize;
    let scene = array_at(&data.root, "scenes", scene_index)?;
    let roots = required_array(scene, "nodes")?;
    for root in roots {
        let node_index = root
            .as_u64()
            .ok_or_else(|| ContentError::new("scene root node index is not an integer"))?
            as usize;
        import_node(&mut context, node_index, -1, identity())?;
    }

    if context.meshes.is_empty() {
        return Err(ContentError::new(
            "scene asset contains no renderable mesh primitives",
        ));
    }

    apply_origin_policy(&mut context.meshes, options.origin_policy);
    let collision_meshes = build_collision_meshes(&context.meshes);
    let metadata = build_metadata(
        source_hash,
        options_hash,
        options,
        materials.len(),
        context.scene_nodes.len(),
        &context.meshes,
        &collision_meshes,
    );

    Ok(CompiledSceneAsset {
        metadata,
        scene_nodes: context.scene_nodes,
        materials,
        meshes: context.meshes,
        collision_meshes,
    })
}

pub fn compile_scene_asset_to_cache(
    input: impl AsRef<Path>,
    output: impl AsRef<Path>,
    options: CompileOptions,
) -> Result<CompiledSceneAsset> {
    let asset = compile_scene_asset(input, options)?;
    let bytes = write_cache_bytes(&asset)?;
    fs::write(output, bytes)?;
    Ok(asset)
}

pub fn import_glb_scene(
    input: impl AsRef<Path>,
    options: CompileOptions,
) -> Result<CompiledSceneAsset> {
    compile_scene_asset(input, options)
}

pub fn load_cache(path: impl AsRef<Path>) -> Result<CompiledSceneAsset> {
    read_cache_bytes(&fs::read(path)?)
}

pub fn write_cache_bytes(asset: &CompiledSceneAsset) -> Result<Vec<u8>> {
    let mut chunks = vec![
        (CHUNK_METADATA, write_metadata_chunk(&asset.metadata)?),
        (
            CHUNK_SCENE_GRAPH,
            write_scene_graph_chunk(&asset.scene_nodes)?,
        ),
        (CHUNK_MATERIALS, write_materials_chunk(&asset.materials)?),
        (CHUNK_MESHES, write_meshes_chunk(&asset.meshes)?),
        (
            CHUNK_COLLISION,
            write_collision_chunk(&asset.collision_meshes)?,
        ),
    ];
    chunks.sort_by_key(|(kind, _)| *kind);

    let table_size = chunks.len() * CHUNK_ENTRY_SIZE;
    let mut offset = checked_usize_to_u64(HEADER_SIZE + table_size)?;
    let mut entries = Vec::with_capacity(chunks.len());
    for (kind, bytes) in &chunks {
        let hash = *blake3::hash(bytes).as_bytes();
        entries.push(ChunkEntry {
            kind: *kind,
            offset,
            size: checked_usize_to_u64(bytes.len())?,
            hash,
        });
        offset = offset
            .checked_add(checked_usize_to_u64(bytes.len())?)
            .ok_or_else(|| ContentError::new("cache file is too large"))?;
    }

    let mut out = Vec::with_capacity(offset as usize);
    out.extend_from_slice(&CACHE_MAGIC);
    write_u32(&mut out, CACHE_VERSION);
    write_u32(&mut out, CACHE_ENDIAN_MARKER);
    write_u32(&mut out, COMPILER_VERSION);
    write_u32(&mut out, checked_usize_to_u32(chunks.len())?);
    out.extend_from_slice(&asset.metadata.source_hash);
    out.extend_from_slice(&asset.metadata.options_hash);

    for entry in &entries {
        write_u32(&mut out, entry.kind);
        write_u32(&mut out, 0);
        write_u64(&mut out, entry.offset);
        write_u64(&mut out, entry.size);
        out.extend_from_slice(&entry.hash);
    }
    for (_, bytes) in chunks {
        out.extend_from_slice(&bytes);
    }
    Ok(out)
}

pub fn read_cache_bytes(bytes: &[u8]) -> Result<CompiledSceneAsset> {
    if bytes.len() < HEADER_SIZE {
        return Err(ContentError::new(
            "compiled asset cache is shorter than its header",
        ));
    }
    let mut cursor = 0usize;
    let magic = read_exact(bytes, &mut cursor, CACHE_MAGIC.len())?;
    if magic != CACHE_MAGIC.as_slice() {
        return Err(ContentError::new(
            "compiled asset cache has an invalid magic",
        ));
    }
    let cache_version = read_u32(bytes, &mut cursor)?;
    if cache_version != CACHE_VERSION {
        return Err(ContentError::new(format!(
            "unsupported compiled asset cache version {cache_version}"
        )));
    }
    let endian = read_u32(bytes, &mut cursor)?;
    if endian != CACHE_ENDIAN_MARKER {
        return Err(ContentError::new(
            "compiled asset cache endian marker is invalid",
        ));
    }
    let _compiler_version = read_u32(bytes, &mut cursor)?;
    let chunk_count = read_u32(bytes, &mut cursor)? as usize;
    let _source_hash = read_hash(bytes, &mut cursor)?;
    let _options_hash = read_hash(bytes, &mut cursor)?;
    let table_end = HEADER_SIZE
        .checked_add(
            chunk_count
                .checked_mul(CHUNK_ENTRY_SIZE)
                .ok_or_else(|| ContentError::new("cache chunk table is too large"))?,
        )
        .ok_or_else(|| ContentError::new("cache chunk table is too large"))?;
    if bytes.len() < table_end {
        return Err(ContentError::new(
            "compiled asset cache has a truncated chunk table",
        ));
    }

    let mut entries = HashMap::new();
    for _ in 0..chunk_count {
        let kind = read_u32(bytes, &mut cursor)?;
        let _reserved = read_u32(bytes, &mut cursor)?;
        let offset = read_u64(bytes, &mut cursor)? as usize;
        let size = read_u64(bytes, &mut cursor)? as usize;
        let hash = read_hash(bytes, &mut cursor)?;
        let end = offset
            .checked_add(size)
            .ok_or_else(|| ContentError::new("cache chunk range overflows"))?;
        if offset < table_end || end > bytes.len() {
            return Err(ContentError::new("cache chunk range is outside the file"));
        }
        let chunk = &bytes[offset..end];
        if blake3::hash(chunk).as_bytes() != &hash {
            return Err(ContentError::new("cache chunk hash verification failed"));
        }
        entries.insert(kind, chunk);
    }

    let metadata = read_metadata_chunk(required_chunk(&entries, CHUNK_METADATA)?)?;
    let scene_nodes = read_scene_graph_chunk(required_chunk(&entries, CHUNK_SCENE_GRAPH)?)?;
    let materials = read_materials_chunk(required_chunk(&entries, CHUNK_MATERIALS)?)?;
    let meshes = read_meshes_chunk(required_chunk(&entries, CHUNK_MESHES)?)?;
    let collision_meshes = read_collision_chunk(required_chunk(&entries, CHUNK_COLLISION)?)?;

    if metadata.material_count as usize != materials.len()
        || metadata.mesh_count as usize != meshes.len()
        || metadata.collision_mesh_count as usize != collision_meshes.len()
        || metadata.scene_node_count as usize != scene_nodes.len()
    {
        return Err(ContentError::new(
            "compiled asset cache metadata counts do not match chunk payloads",
        ));
    }

    Ok(CompiledSceneAsset {
        metadata,
        scene_nodes,
        materials,
        meshes,
        collision_meshes,
    })
}

pub fn inspect_cache(path: impl AsRef<Path>) -> Result<String> {
    let asset = load_cache(path)?;
    Ok(format!(
        "Aster cache v{} compiler={} materials={} meshes={} collision_meshes={} nodes={} vertices={} indices={} collision_triangles={} source_hash={} options_hash={} options={}",
        asset.metadata.cache_version,
        asset.metadata.compiler_version,
        asset.metadata.material_count,
        asset.metadata.mesh_count,
        asset.metadata.collision_mesh_count,
        asset.metadata.scene_node_count,
        asset.metadata.total_vertices,
        asset.metadata.total_indices,
        asset.metadata.total_collision_triangles,
        hex_hash(&asset.metadata.source_hash),
        hex_hash(&asset.metadata.options_hash),
        asset.metadata.compiler_options
    ))
}

pub fn hex_hash(hash: &[u8; 32]) -> String {
    let mut out = String::with_capacity(64);
    for byte in hash {
        use std::fmt::Write;
        let _ = write!(&mut out, "{byte:02x}");
    }
    out
}

pub fn texture_mip_count(mut width: u32, mut height: u32) -> u32 {
    width = width.max(1);
    height = height.max(1);
    let mut levels = 1;
    while width > 1 || height > 1 {
        width = (width / 2).max(1);
        height = (height / 2).max(1);
        levels += 1;
    }
    levels
}

pub fn inspect_texture(path: impl AsRef<Path>, role: &str) -> Result<TextureImportSummary> {
    let bytes = fs::read(path)?;
    let (format, width, height, header_mips, mut diagnostics) = inspect_texture_header(&bytes);
    if width == 0 || height == 0 {
        diagnostics.push("texture dimensions could not be decoded from header".to_string());
    }
    let kind = TextureImportKind::role(role);
    Ok(TextureImportSummary {
        role: role.to_string(),
        kind,
        color_space: kind.color_space().to_string(),
        width,
        height,
        mip_count: header_mips.max(if width > 0 && height > 0 {
            texture_mip_count(width, height)
        } else {
            1
        }),
        format,
        source_hash: *blake3::hash(&bytes).as_bytes(),
        diagnostics,
    })
}

pub fn bake_texture_to_ktx2(
    input: impl AsRef<Path>,
    output: impl AsRef<Path>,
    role: &str,
) -> Result<TextureImportSummary> {
    let summary = inspect_texture(&input, role)?;
    let width = summary.width.max(1);
    let height = summary.height.max(1);
    let mip_count = summary.mip_count.max(1);
    let vk_format = if summary.color_space == "srgb" {
        43u32
    } else {
        37u32
    };
    let mut bytes = Vec::new();
    bytes.extend_from_slice(&[0xab, b'K', b'T', b'X', b' ', b'2', b'0', 0xbb]);
    bytes.extend_from_slice(&[b'\r', b'\n', 0x1a, b'\n']);
    bytes.extend_from_slice(&vk_format.to_le_bytes());
    bytes.extend_from_slice(&1u32.to_le_bytes());
    bytes.extend_from_slice(&width.to_le_bytes());
    bytes.extend_from_slice(&height.to_le_bytes());
    bytes.extend_from_slice(&0u32.to_le_bytes());
    bytes.extend_from_slice(&0u32.to_le_bytes());
    bytes.extend_from_slice(&1u32.to_le_bytes());
    bytes.extend_from_slice(&mip_count.to_le_bytes());
    bytes.extend_from_slice(&1u32.to_le_bytes());
    bytes.extend_from_slice(b"ASTER_TEXTURE_BAKE_V1\0");
    bytes.extend_from_slice(summary.kind.as_str().as_bytes());
    bytes.push(0);
    bytes.extend_from_slice(summary.color_space.as_bytes());
    bytes.push(0);
    bytes.extend_from_slice(&summary.source_hash);
    bytes.extend_from_slice(b"ASTER_MIP_PAYLOADS_V1\0");
    let mut mip_width = width;
    let mut mip_height = height;
    for level in 0..mip_count {
        let block_width = (mip_width.max(1) + 3) / 4;
        let block_height = (mip_height.max(1) + 3) / 4;
        let payload_len = (block_width as usize * block_height as usize * 16).max(16);
        bytes.extend_from_slice(&level.to_le_bytes());
        bytes.extend_from_slice(&mip_width.to_le_bytes());
        bytes.extend_from_slice(&mip_height.to_le_bytes());
        bytes.extend_from_slice(&(payload_len as u64).to_le_bytes());
        for byte_index in 0..payload_len {
            let hash_byte =
                summary.source_hash[(byte_index + level as usize) % summary.source_hash.len()];
            let role_bytes = summary.role.as_bytes();
            let role_byte = if role_bytes.is_empty() {
                0
            } else {
                role_bytes[(byte_index + summary.kind.as_str().len()) % role_bytes.len()]
            };
            bytes.push(hash_byte ^ role_byte ^ (level as u8).wrapping_mul(17) ^ (byte_index as u8));
        }
        mip_width = (mip_width / 2).max(1);
        mip_height = (mip_height / 2).max(1);
    }
    if let Some(parent) = output.as_ref().parent() {
        if !parent.as_os_str().is_empty() {
            fs::create_dir_all(parent)?;
        }
    }
    fs::write(output, bytes)?;
    Ok(summary)
}

pub fn read_asset_database(path: impl AsRef<Path>) -> Result<AssetDatabase> {
    Ok(serde_json::from_slice(&fs::read(path)?)?)
}

pub fn cook_project(
    project: impl AsRef<Path>,
    platform: &str,
    output: impl AsRef<Path>,
) -> Result<CookProjectResult> {
    let project = project.as_ref();
    let output = output.as_ref();
    if platform != "desktop" {
        return Err(ContentError::new(format!(
            "unsupported Asset v1 platform '{platform}', expected desktop"
        )));
    }
    fs::create_dir_all(output)?;
    let project_bytes = fs::read(project)?;
    let root: Value = serde_json::from_slice(&project_bytes)?;
    let project_root = project.parent().unwrap_or_else(|| Path::new(""));
    let mut records = Vec::new();
    if let Some(assets) = root.get("assets").and_then(Value::as_array) {
        for asset in assets {
            let id = asset.get("id").and_then(Value::as_str).unwrap_or("");
            let declared_kind = asset.get("kind").and_then(Value::as_str).unwrap_or("");
            let Some(source) = asset.get("path").and_then(Value::as_str) else {
                continue;
            };
            let source_path = project_root.join(source);
            records.push(cook_asset(
                &source_path,
                id,
                declared_kind,
                project_root,
                output,
                platform,
            )?);
        }
    }
    records.sort_by(|lhs, rhs| lhs.guid.cmp(&rhs.guid));
    let database = AssetDatabase {
        schema_version: ASSET_DATABASE_SCHEMA_VERSION,
        platform: platform.to_string(),
        records,
    };
    let database_path = output.join("assetdb.asterdb.json");
    write_json(&database_path, &database)?;
    Ok(CookProjectResult {
        database,
        database_path,
    })
}

pub fn cook_asset(
    source: impl AsRef<Path>,
    id: &str,
    declared_kind: &str,
    project_root: impl AsRef<Path>,
    output_root: impl AsRef<Path>,
    platform: &str,
) -> Result<AssetDatabaseRecord> {
    let source = source.as_ref();
    let project_root = project_root.as_ref();
    let output_root = output_root.as_ref();
    let source_rel = relative_path_string(source, project_root);
    let kind = canonical_asset_kind(source, declared_kind);
    let mut record = base_asset_record(id, &kind, &source_rel, platform);
    if !source.exists() {
        record.diagnostics.push(AssetCookDiagnostic {
            severity: "error".to_string(),
            message: format!("asset source is missing: {}", source.display()),
        });
        return Ok(record);
    }
    let source_bytes = fs::read(source)?;
    record.source_hash = hash_hex_bytes(&source_bytes);
    record.guid = asset_guid(&kind, id, &source_rel);

    match kind.as_str() {
        "scene" => {
            let cache_name = format!("{}.astercache", safe_stem(id, source));
            let cache_path = output_root.join("scenes").join(cache_name);
            if let Some(parent) = cache_path.parent() {
                fs::create_dir_all(parent)?;
            }
            match compile_scene_asset_to_cache(
                source,
                &cache_path,
                CompileOptions {
                    origin_policy: OriginPolicy::Keep,
                    unit_scale: 1.0,
                },
            ) {
                Ok(asset) => {
                    record.source_hash = hex_hash(&asset.metadata.source_hash);
                    record.options_hash = hex_hash(&asset.metadata.options_hash);
                    for material in &asset.materials {
                        for dependency in &material.texture_dependencies {
                            record.dependencies.push(AssetDependencyRecord {
                                role: dependency.role.clone(),
                                path: dependency.uri.clone(),
                                present: dependency.present,
                                hash: hex_hash(&dependency.hash),
                            });
                            if !dependency.present {
                                record.diagnostics.push(AssetCookDiagnostic {
                                    severity: "warning".to_string(),
                                    message: format!(
                                        "scene material '{}' references missing texture '{}'",
                                        material.name, dependency.uri
                                    ),
                                });
                            }
                        }
                    }
                    record.outputs.push(AssetCookedOutput {
                        role: "runtime-cache".to_string(),
                        kind: "astercache".to_string(),
                        path: relative_path_string(&cache_path, output_root),
                        hash: hash_file_hex(&cache_path)?,
                    });
                    let report_path = output_root
                        .join("reports")
                        .join(format!("{}.report.json", safe_stem(id, source)));
                    let report = serde_json::json!({
                        "schema_version": 1,
                        "kind": "scene",
                        "id": id,
                        "source_path": source_rel,
                        "source_hash": record.source_hash,
                        "options_hash": record.options_hash,
                        "materials": asset.metadata.material_count,
                        "meshes": asset.metadata.mesh_count,
                        "collision_meshes": asset.metadata.collision_mesh_count,
                        "vertices": asset.metadata.total_vertices,
                        "indices": asset.metadata.total_indices,
                        "diagnostics": record.diagnostics,
                    });
                    write_json(&report_path, &report)?;
                    record.outputs.push(AssetCookedOutput {
                        role: "report".to_string(),
                        kind: "json".to_string(),
                        path: relative_path_string(&report_path, output_root),
                        hash: hash_file_hex(&report_path)?,
                    });
                }
                Err(error) => record.diagnostics.push(AssetCookDiagnostic {
                    severity: "error".to_string(),
                    message: error.to_string(),
                }),
            }
        }
        "material" if source.extension().and_then(|v| v.to_str()) == Some("astermat") => {
            let cooked = cook_material_asset(source, project_root, output_root, id, platform)?;
            record.dependencies = cooked.material_bin.dependency_hashes.clone();
            record.diagnostics = cooked.material_bin.diagnostics.clone();
            record.options_hash = hash_hex_text(&format!(
                "materialbin:{}:{}",
                MATERIAL_BIN_SCHEMA_VERSION, platform
            ));
            record.outputs.push(AssetCookedOutput {
                role: "materialbin".to_string(),
                kind: "materialbin".to_string(),
                path: relative_path_string(&cooked.material_bin_path, output_root),
                hash: hash_file_hex(&cooked.material_bin_path)?,
            });
            record.outputs.push(AssetCookedOutput {
                role: "report".to_string(),
                kind: "json".to_string(),
                path: relative_path_string(&cooked.report_path, output_root),
                hash: hash_file_hex(&cooked.report_path)?,
            });
            record.outputs.push(AssetCookedOutput {
                role: "preview".to_string(),
                kind: "ppm".to_string(),
                path: relative_path_string(&cooked.preview_path, output_root),
                hash: hash_file_hex(&cooked.preview_path)?,
            });
            for texture in &cooked.material_bin.textures {
                record.outputs.push(AssetCookedOutput {
                    role: format!("texture:{}", texture.role),
                    kind: "ktx2".to_string(),
                    path: texture.cooked_path.clone(),
                    hash: texture.cooked_hash.clone(),
                });
            }
        }
        "texture" => {
            let role = source
                .file_stem()
                .and_then(|value| value.to_str())
                .and_then(|stem| stem.rsplit_once('_').map(|(_, role)| role))
                .unwrap_or("unknown");
            let cooked = cook_texture_asset(source, output_root, role)?;
            record.options_hash = hash_hex_text("texture:ktx2-basis:v1");
            record.outputs.push(AssetCookedOutput {
                role: "texture".to_string(),
                kind: "ktx2".to_string(),
                path: relative_path_string(&cooked.output_path, output_root),
                hash: cooked.report.cooked_hash.clone(),
            });
            record.outputs.push(AssetCookedOutput {
                role: "report".to_string(),
                kind: "json".to_string(),
                path: relative_path_string(&cooked.report_path, output_root),
                hash: hash_file_hex(&cooked.report_path)?,
            });
            record.dependencies.push(AssetDependencyRecord {
                role: role.to_string(),
                path: source_rel,
                present: true,
                hash: cooked.report.source_hash,
            });
        }
        _ => record.diagnostics.push(AssetCookDiagnostic {
            severity: "warning".to_string(),
            message: format!(
                "Asset v1 cook does not transform kind '{}' from '{}'",
                declared_kind,
                source.display()
            ),
        }),
    }
    Ok(record)
}

pub fn cook_texture_asset(
    input: impl AsRef<Path>,
    output_root: impl AsRef<Path>,
    role: &str,
) -> Result<TextureCookResult> {
    let input = input.as_ref();
    let output_root = output_root.as_ref();
    let output_path = output_root.join("textures").join(format!(
        "{}.ktx2",
        input
            .file_stem()
            .and_then(|v| v.to_str())
            .unwrap_or("texture")
    ));
    cook_texture_asset_as(input, output_root, role, &output_path)
}

pub fn cook_material_asset(
    input: impl AsRef<Path>,
    project_root: impl AsRef<Path>,
    output_root: impl AsRef<Path>,
    fallback_id: &str,
    platform: &str,
) -> Result<MaterialCookResult> {
    let input = input.as_ref();
    let project_root = project_root.as_ref();
    let output_root = output_root.as_ref();
    let source = fs::read_to_string(input)?;
    let parsed = parse_astermat_source(&source, fallback_id);
    let id = if parsed.id.is_empty() {
        fallback_id.to_string()
    } else {
        parsed.id.clone()
    };
    let asset_guid = asset_guid("material", &id, &relative_path_string(input, project_root));
    let material_stem = safe_identifier(&id);
    let mut texture_records = Vec::new();
    let mut dependencies = Vec::new();
    let mut diagnostics = parsed.diagnostics.clone();

    for (role, uri) in &parsed.textures {
        let source_path = if Path::new(uri).is_absolute() {
            PathBuf::from(uri)
        } else {
            input.parent().unwrap_or_else(|| Path::new("")).join(uri)
        };
        if !source_path.exists() {
            diagnostics.push(AssetCookDiagnostic {
                severity: "error".to_string(),
                message: format!(
                    "material texture '{}' is missing: {}",
                    role,
                    source_path.display()
                ),
            });
            dependencies.push(AssetDependencyRecord {
                role: role.clone(),
                path: relative_path_string(&source_path, project_root),
                present: false,
                hash: String::new(),
            });
            continue;
        }
        let cooked_name = format!("{}_{}.ktx2", material_stem, safe_identifier(role));
        let cooked_path = output_root.join("textures").join(cooked_name);
        let cooked = cook_texture_asset_as(&source_path, output_root, role, &cooked_path)?;
        dependencies.push(AssetDependencyRecord {
            role: role.clone(),
            path: relative_path_string(&source_path, project_root),
            present: true,
            hash: cooked.report.source_hash.clone(),
        });
        texture_records.push(MaterialBinTextureRecord {
            role: role.clone(),
            source_path: relative_path_string(&source_path, project_root),
            cooked_path: relative_path_string(&cooked.output_path, output_root),
            kind: cooked.report.kind.clone(),
            color_space: cooked.report.color_space.clone(),
            width: cooked.report.width,
            height: cooked.report.height,
            mip_count: cooked.report.mip_count,
            source_hash: cooked.report.source_hash.clone(),
            cooked_hash: cooked.report.cooked_hash.clone(),
            diagnostics: cooked.report.diagnostics.clone(),
        });
    }

    let mut binding_layout = vec![MaterialBinBinding {
        name: "MaterialParameters".to_string(),
        kind: "uniform-buffer".to_string(),
        binding: 0,
    }];
    for (index, texture) in texture_records.iter().enumerate() {
        binding_layout.push(MaterialBinBinding {
            name: texture.role.clone(),
            kind: "texture".to_string(),
            binding: (index + 1) as u32,
        });
    }
    if !texture_records.is_empty() {
        binding_layout.push(MaterialBinBinding {
            name: "MaterialSampler".to_string(),
            kind: "sampler".to_string(),
            binding: binding_layout.len() as u32,
        });
    }

    let feature_mask = material_feature_mask(&parsed);
    let shader_variant_key = material_variant_key(&source, &texture_records);
    let shader_variant_tag = material_variant_tag(&parsed, !texture_records.is_empty());
    let material_bin = MaterialBin {
        schema_version: MATERIAL_BIN_SCHEMA_VERSION,
        asset_guid,
        id: id.clone(),
        name: parsed.name.clone(),
        source_path: relative_path_string(input, project_root),
        feature_mask,
        shader_variant_key,
        shader_variant_tag: shader_variant_tag.clone(),
        pipeline_tag: material_bin_pipeline_tag(&parsed, !texture_records.is_empty()),
        fallback: MaterialBinFallback {
            base_color: [
                param_or(&parsed, "base_color_r", 1.0),
                param_or(&parsed, "base_color_g", 1.0),
                param_or(&parsed, "base_color_b", 1.0),
            ],
            emission_color: [
                param_or(&parsed, "emission_color_r", 0.0),
                param_or(&parsed, "emission_color_g", 0.0),
                param_or(&parsed, "emission_color_b", 0.0),
            ],
            roughness: param_or(&parsed, "roughness", 0.55),
            metallic: param_or(&parsed, "metallic", 0.0),
            emission_strength: param_or(&parsed, "emission_strength", 0.0),
            opacity: param_or(&parsed, "opacity", 1.0),
            double_sided: parsed
                .features
                .get("double_sided")
                .copied()
                .unwrap_or(parsed.cull_mode.eq_ignore_ascii_case("None")),
            alpha_mode: parsed.blend_mode.clone(),
            receives_shadows: parsed.receives_shadows,
            surface_profile: parsed.surface_profile.clone(),
        },
        params: parsed.params.clone(),
        features: parsed.features.clone(),
        textures: texture_records,
        binding_layout,
        dependency_hashes: dependencies,
        diagnostics,
    };

    let material_bin_path = output_root
        .join("materials")
        .join(format!("{}.materialbin", material_stem));
    write_json(&material_bin_path, &material_bin)?;
    let report_path = output_root
        .join("reports")
        .join(format!("{}.report.json", material_stem));
    write_json(&report_path, &material_bin)?;
    let preview_path = output_root
        .join("previews")
        .join(format!("{}.preview.ppm", material_stem));
    write_material_preview(&preview_path, &material_bin)?;
    let _ = platform;
    Ok(MaterialCookResult {
        material_bin_path,
        report_path,
        preview_path,
        material_bin,
    })
}

pub fn report_asset_database(database: &AssetDatabase) -> String {
    let error_count = database
        .records
        .iter()
        .flat_map(|record| record.diagnostics.iter())
        .filter(|diagnostic| diagnostic.severity == "error")
        .count();
    let warning_count = database
        .records
        .iter()
        .flat_map(|record| record.diagnostics.iter())
        .filter(|diagnostic| diagnostic.severity == "warning")
        .count();
    let output_count: usize = database
        .records
        .iter()
        .map(|record| record.outputs.len())
        .sum();
    format!(
        "Aster asset database v{} platform={} assets={} outputs={} errors={} warnings={}",
        database.schema_version,
        database.platform,
        database.records.len(),
        output_count,
        error_count,
        warning_count
    )
}

fn cook_texture_asset_as(
    input: &Path,
    output_root: &Path,
    role: &str,
    output_path: &Path,
) -> Result<TextureCookResult> {
    let summary = bake_texture_to_ktx2(input, output_path, role)?;
    let cooked_hash = hash_file_hex(output_path)?;
    let report_path = output_root.join("reports").join(format!(
        "{}.{}.report.json",
        input
            .file_stem()
            .and_then(|v| v.to_str())
            .unwrap_or("texture"),
        safe_identifier(role)
    ));
    let report = TextureCookReport {
        schema_version: 1,
        role: role.to_string(),
        kind: summary.kind.as_str().to_string(),
        color_space: summary.color_space,
        format: summary.format,
        width: summary.width,
        height: summary.height,
        mip_count: summary.mip_count,
        source_hash: hex_hash(&summary.source_hash),
        cooked_hash,
        cooked_path: relative_path_string(output_path, output_root),
        diagnostics: summary.diagnostics,
    };
    write_json(&report_path, &report)?;
    Ok(TextureCookResult {
        output_path: output_path.to_path_buf(),
        report_path,
        report,
    })
}

#[derive(Clone, Debug, Default)]
struct ParsedMaterialSource {
    id: String,
    name: String,
    shading_model: String,
    blend_mode: String,
    cull_mode: String,
    surface_profile: String,
    receives_shadows: bool,
    textures: BTreeMap<String, String>,
    params: BTreeMap<String, f32>,
    features: BTreeMap<String, bool>,
    diagnostics: Vec<AssetCookDiagnostic>,
}

fn parse_astermat_source(source: &str, fallback_id: &str) -> ParsedMaterialSource {
    let mut parsed = ParsedMaterialSource {
        id: fallback_id.to_string(),
        name: fallback_id.to_string(),
        shading_model: "LitPBR".to_string(),
        blend_mode: "Opaque".to_string(),
        cull_mode: "Back".to_string(),
        surface_profile: "Auto".to_string(),
        receives_shadows: true,
        ..ParsedMaterialSource::default()
    };
    let mut block = "";
    for raw_line in source.lines() {
        let line = raw_line.split("//").next().unwrap_or("").trim();
        if line.is_empty() {
            continue;
        }
        if let Some(rest) = line.strip_prefix("material ") {
            parsed.id = rest
                .split_whitespace()
                .next()
                .unwrap_or(fallback_id)
                .trim_matches('{')
                .to_string();
            parsed.name = parsed.id.clone();
            continue;
        }
        match line {
            "textures {" => {
                block = "textures";
                continue;
            }
            "params {" => {
                block = "params";
                continue;
            }
            "features {" => {
                block = "features";
                continue;
            }
            "layers {" => {
                block = "layers";
                continue;
            }
            "}" => {
                block = "";
                continue;
            }
            _ => {}
        }
        if block == "textures" {
            if let Some((role, value)) = split_key_value(line) {
                parsed
                    .textures
                    .insert(role.to_string(), unquote(value).to_string());
            }
            continue;
        }
        if block == "params" {
            if let Some((key, value)) = split_key_value(line) {
                if let Ok(value) = value.parse::<f32>() {
                    parsed.params.insert(key.to_string(), value);
                } else {
                    parsed.diagnostics.push(AssetCookDiagnostic {
                        severity: "warning".to_string(),
                        message: format!("ignored non-numeric material parameter '{key}'"),
                    });
                }
            }
            continue;
        }
        if block == "features" {
            if let Some((key, value)) = split_key_value(line) {
                parsed.features.insert(key.to_string(), value == "true");
            }
            continue;
        }
        if block.is_empty() {
            if let Some((key, value)) = split_key_value(line) {
                let value = unquote(value);
                match key {
                    "name" => parsed.name = value.to_string(),
                    "shading_model" => parsed.shading_model = value.to_string(),
                    "blend_mode" => parsed.blend_mode = value.to_string(),
                    "cull_mode" => parsed.cull_mode = value.to_string(),
                    "surface_profile" => parsed.surface_profile = value.to_string(),
                    "receives_shadows" => parsed.receives_shadows = value != "false",
                    _ => {}
                }
            }
        }
    }
    parsed
}

fn split_key_value(line: &str) -> Option<(&str, &str)> {
    let (key, value) = line.split_once(':')?;
    Some((key.trim(), value.trim().trim_end_matches(',')))
}

fn unquote(value: &str) -> &str {
    value.trim().trim_matches('"')
}

fn param_or(parsed: &ParsedMaterialSource, name: &str, fallback: f32) -> f32 {
    parsed.params.get(name).copied().unwrap_or(fallback)
}

fn material_feature_mask(parsed: &ParsedMaterialSource) -> u64 {
    let mut mask = 0u64;
    if !parsed.textures.is_empty() {
        mask |= 1 << 0;
    }
    if parsed.textures.contains_key("normal") || parsed.features.get("normal_map") == Some(&true) {
        mask |= 1 << 1;
    }
    if parsed.textures.contains_key("orm") {
        mask |= 1 << 2;
    }
    if parsed.textures.contains_key("height") {
        mask |= 1 << 3;
    }
    if parsed.features.get("parallax") == Some(&true) {
        mask |= 1 << 4;
    }
    if parsed.features.get("triplanar") == Some(&true) {
        mask |= 1 << 5;
    }
    if parsed.blend_mode != "Opaque" {
        mask |= 1 << 6;
    }
    mask
}

fn material_variant_key(source: &str, textures: &[MaterialBinTextureRecord]) -> u64 {
    let mut hasher = blake3::Hasher::new();
    hasher.update(b"aster.materialbin.variant.v1\0");
    hasher.update(source.as_bytes());
    for texture in textures {
        hasher.update(texture.role.as_bytes());
        hasher.update(texture.source_hash.as_bytes());
        hasher.update(texture.cooked_hash.as_bytes());
    }
    let hash = hasher.finalize();
    let bytes = hash.as_bytes();
    u64::from_le_bytes([
        bytes[0], bytes[1], bytes[2], bytes[3], bytes[4], bytes[5], bytes[6], bytes[7],
    ])
}

fn material_variant_tag(parsed: &ParsedMaterialSource, textured: bool) -> String {
    let mut tag = format!("{}.{}", parsed.shading_model, parsed.blend_mode);
    if textured {
        tag.push_str(".textured");
    }
    for (feature, enabled) in &parsed.features {
        if *enabled {
            tag.push('.');
            tag.push_str(&feature.replace('_', "-"));
        }
    }
    tag
}

fn material_bin_pipeline_tag(parsed: &ParsedMaterialSource, textured: bool) -> String {
    let mut tag = if parsed.blend_mode == "Blend" {
        "transparent.depth-read".to_string()
    } else {
        "opaque.depth-write".to_string()
    };
    if parsed.cull_mode == "None" {
        tag.push_str(".double-sided");
    } else {
        tag.push_str(".culled");
    }
    if textured {
        tag.push_str(".textured");
    }
    tag.push_str(".shader-variant");
    tag
}

fn base_asset_record(
    id: &str,
    kind: &str,
    source_path: &str,
    platform: &str,
) -> AssetDatabaseRecord {
    AssetDatabaseRecord {
        guid: asset_guid(kind, id, source_path),
        id: id.to_string(),
        kind: kind.to_string(),
        source_path: source_path.to_string(),
        import_settings_version: ASSET_IMPORT_SETTINGS_VERSION,
        source_hash: String::new(),
        options_hash: hash_hex_text(&format!("{kind}:{}", ASSET_IMPORT_SETTINGS_VERSION)),
        dependencies: Vec::new(),
        outputs: Vec::new(),
        diagnostics: Vec::new(),
        platform: platform.to_string(),
    }
}

fn canonical_asset_kind(path: &Path, declared_kind: &str) -> String {
    let extension = path
        .extension()
        .and_then(|value| value.to_str())
        .unwrap_or("")
        .to_ascii_lowercase();
    match extension.as_str() {
        "scene" | "gltf" | "glb" => "scene".to_string(),
        "astermat" => "material".to_string(),
        "png" | "ktx2" | "tga" | "jpg" | "jpeg" => "texture".to_string(),
        _ if !declared_kind.is_empty() => declared_kind.to_string(),
        _ => "unknown".to_string(),
    }
}

fn asset_guid(kind: &str, id: &str, source_path: &str) -> String {
    let mut hasher = blake3::Hasher::new();
    hasher.update(b"aster.asset.guid.v1\0");
    hasher.update(kind.as_bytes());
    hasher.update(b"\0");
    hasher.update(id.as_bytes());
    hasher.update(b"\0");
    hasher.update(source_path.replace('\\', "/").as_bytes());
    hex_hash(hasher.finalize().as_bytes())
}

fn hash_file_hex(path: &Path) -> Result<String> {
    Ok(hash_hex_bytes(&fs::read(path)?))
}

fn hash_hex_bytes(bytes: &[u8]) -> String {
    hex_hash(blake3::hash(bytes).as_bytes())
}

fn hash_hex_text(text: &str) -> String {
    hash_hex_bytes(text.as_bytes())
}

fn relative_path_string(path: &Path, root: &Path) -> String {
    path.strip_prefix(root)
        .unwrap_or(path)
        .to_string_lossy()
        .replace('\\', "/")
}

fn safe_stem(id: &str, source: &Path) -> String {
    if !id.is_empty() {
        safe_identifier(id)
    } else {
        safe_identifier(
            source
                .file_stem()
                .and_then(|value| value.to_str())
                .unwrap_or("asset"),
        )
    }
}

fn safe_identifier(value: &str) -> String {
    let mut out = String::with_capacity(value.len());
    for c in value.chars() {
        if c.is_ascii_alphanumeric() {
            out.push(c.to_ascii_lowercase());
        } else if c == '_' || c == '-' || c == '.' {
            out.push(c);
        } else {
            out.push('_');
        }
    }
    if out.is_empty() {
        "asset".to_string()
    } else {
        out
    }
}

fn write_json<T: Serialize + ?Sized>(path: &Path, value: &T) -> Result<()> {
    if let Some(parent) = path.parent() {
        if !parent.as_os_str().is_empty() {
            fs::create_dir_all(parent)?;
        }
    }
    let bytes = serde_json::to_vec_pretty(value)?;
    fs::write(path, bytes)?;
    Ok(())
}

fn write_material_preview(path: &Path, material: &MaterialBin) -> Result<()> {
    if let Some(parent) = path.parent() {
        if !parent.as_os_str().is_empty() {
            fs::create_dir_all(parent)?;
        }
    }
    let hash = blake3::hash(material.id.as_bytes());
    let bytes = hash.as_bytes();
    let r = (material.fallback.base_color[0].clamp(0.0, 1.0) * 255.0) as u32;
    let g = (material.fallback.base_color[1].clamp(0.0, 1.0) * 255.0) as u32;
    let b = (material.fallback.base_color[2].clamp(0.0, 1.0) * 255.0) as u32;
    let mut ppm = String::from("P3\n8 8\n255\n");
    for y in 0..8u32 {
        for x in 0..8u32 {
            let checker = if ((x / 2) + (y / 2)) % 2 == 0 { 18 } else { 0 };
            let noise = bytes[((x + y * 8) as usize) % bytes.len()] as u32 / 16;
            ppm.push_str(&format!(
                "{} {} {} ",
                (r + checker + noise).min(255),
                (g + checker + noise / 2).min(255),
                (b + checker).min(255)
            ));
        }
        ppm.push('\n');
    }
    fs::write(path, ppm)?;
    Ok(())
}

fn inspect_texture_header(bytes: &[u8]) -> (String, u32, u32, u32, Vec<String>) {
    if bytes.len() >= 24 && bytes.starts_with(&[0x89, b'P', b'N', b'G', b'\r', b'\n', 0x1a, b'\n'])
    {
        return (
            "png".to_string(),
            u32::from_be_bytes([bytes[16], bytes[17], bytes[18], bytes[19]]),
            u32::from_be_bytes([bytes[20], bytes[21], bytes[22], bytes[23]]),
            1,
            Vec::new(),
        );
    }
    if bytes.len() >= 68
        && bytes.starts_with(&[
            0xab, b'K', b'T', b'X', b' ', b'2', b'0', 0xbb, b'\r', b'\n', 0x1a, b'\n',
        ])
    {
        return (
            "ktx2".to_string(),
            u32::from_le_bytes([bytes[20], bytes[21], bytes[22], bytes[23]]),
            u32::from_le_bytes([bytes[24], bytes[25], bytes[26], bytes[27]]),
            u32::from_le_bytes([bytes[40], bytes[41], bytes[42], bytes[43]]).max(1),
            Vec::new(),
        );
    }
    if bytes.len() >= 18 {
        let image_type = bytes[2];
        if image_type == 2 || image_type == 3 || image_type == 10 || image_type == 11 {
            return (
                "tga".to_string(),
                u16::from_le_bytes([bytes[12], bytes[13]]) as u32,
                u16::from_le_bytes([bytes[14], bytes[15]]) as u32,
                1,
                Vec::new(),
            );
        }
    }
    if bytes.len() >= 4 && bytes.starts_with(&[0xff, 0xd8]) {
        if let Some((width, height)) = inspect_jpeg_size(bytes) {
            return ("jpeg".to_string(), width, height, 1, Vec::new());
        }
        return (
            "jpeg".to_string(),
            0,
            0,
            1,
            vec!["jpeg header was present but SOF dimensions were not found".to_string()],
        );
    }
    if bytes.len() >= 4 && bytes.starts_with(&[0x76, 0x2f, 0x31, 0x01]) {
        return (
            "exr".to_string(),
            0,
            0,
            1,
            vec!["EXR payload detected; full dataWindow decode is deferred".to_string()],
        );
    }
    (
        "unknown".to_string(),
        0,
        0,
        1,
        vec!["unsupported texture header".to_string()],
    )
}

fn inspect_jpeg_size(bytes: &[u8]) -> Option<(u32, u32)> {
    let mut cursor = 2usize;
    while cursor + 9 < bytes.len() {
        if bytes[cursor] != 0xff {
            cursor += 1;
            continue;
        }
        while cursor < bytes.len() && bytes[cursor] == 0xff {
            cursor += 1;
        }
        if cursor >= bytes.len() {
            return None;
        }
        let marker = bytes[cursor];
        cursor += 1;
        if marker == 0xd9 || marker == 0xda {
            return None;
        }
        if cursor + 2 > bytes.len() {
            return None;
        }
        let length = u16::from_be_bytes([bytes[cursor], bytes[cursor + 1]]) as usize;
        if length < 2 || cursor + length > bytes.len() {
            return None;
        }
        let is_sof = matches!(
            marker,
            0xc0 | 0xc1
                | 0xc2
                | 0xc3
                | 0xc5
                | 0xc6
                | 0xc7
                | 0xc9
                | 0xca
                | 0xcb
                | 0xcd
                | 0xce
                | 0xcf
        );
        if is_sof && length >= 7 {
            let height = u16::from_be_bytes([bytes[cursor + 3], bytes[cursor + 4]]) as u32;
            let width = u16::from_be_bytes([bytes[cursor + 5], bytes[cursor + 6]]) as u32;
            return Some((width, height));
        }
        cursor += length;
    }
    None
}

fn default_material() -> Material {
    let mut material = Material {
        name: "Default".to_string(),
        ..Material::default()
    };
    compile_material_for_rendering(&mut material);
    material
}

const MATERIAL_FLAG_TEXTURED: u32 = 1 << 0;
const MATERIAL_FLAG_PROCEDURAL: u32 = 1 << 1;
const MATERIAL_FLAG_TRANSPARENT: u32 = 1 << 2;
const MATERIAL_FLAG_DOUBLE_SIDED: u32 = 1 << 3;
const MATERIAL_FLAG_DEPTH_WRITE: u32 = 1 << 4;
const MATERIAL_FLAG_CONTACT_SHADOW: u32 = 1 << 5;

fn material_has_texture_dependencies(material: &Material) -> bool {
    material.has_base_color_texture
        || material.has_metallic_roughness_texture
        || material.has_normal_texture
        || material.has_occlusion_texture
        || material
            .texture_dependencies
            .iter()
            .any(|dependency| !dependency.role.is_empty())
}

fn material_is_transparent(material: &Material) -> bool {
    material.alpha_mode == AlphaMode::Blend || material.opacity < 0.999
}

fn material_writes_depth(material: &Material) -> bool {
    match material.depth_write {
        DepthWrite::Enabled => true,
        DepthWrite::Disabled => false,
        DepthWrite::Auto => !material_is_transparent(material),
    }
}

fn material_permutation_flags(material: &Material) -> u32 {
    let mut flags = 0u32;
    if material_has_texture_dependencies(material) {
        flags |= MATERIAL_FLAG_TEXTURED;
    }
    if material_is_transparent(material) {
        flags |= MATERIAL_FLAG_TRANSPARENT;
    }
    if material.double_sided {
        flags |= MATERIAL_FLAG_DOUBLE_SIDED;
    }
    if material_writes_depth(material) {
        flags |= MATERIAL_FLAG_DEPTH_WRITE;
    }
    flags
}

fn append_key(hash: &mut u64, bytes: &[u8]) {
    for byte in bytes {
        *hash ^= u64::from(*byte);
        *hash = hash.wrapping_mul(1_099_511_628_211);
    }
}

fn append_u32_key(hash: &mut u64, value: u32) {
    append_key(hash, &value.to_le_bytes());
}

fn append_f32_key(hash: &mut u64, value: f32) {
    append_key(hash, &value.to_le_bytes());
}

fn material_pipeline_tag(material: &Material, flags: u32) -> String {
    let mut tag = String::new();
    tag.push_str(if material_is_transparent(material) {
        "transparent"
    } else {
        "opaque"
    });
    tag.push_str(if material_writes_depth(material) {
        ".depth-write"
    } else {
        ".depth-read"
    });
    tag.push_str(if material.double_sided {
        ".double-sided"
    } else {
        ".culled"
    });
    if flags & MATERIAL_FLAG_TEXTURED != 0 {
        tag.push_str(".textured");
    }
    if flags & MATERIAL_FLAG_PROCEDURAL != 0 {
        tag.push_str(".procedural");
    }
    if flags & MATERIAL_FLAG_CONTACT_SHADOW != 0 {
        tag.push_str(".contact-shadow");
    }
    tag
}

fn compile_material_for_rendering(material: &mut Material) {
    let flags = material_permutation_flags(material);
    let mut hash = 1_469_598_103_934_665_603u64;
    append_u32_key(&mut hash, flags);
    append_u32_key(&mut hash, 0); // MaterialRenderRole::Surface
    append_u32_key(&mut hash, material.alpha_mode as u32);
    append_u32_key(&mut hash, material.depth_write as u32);
    append_u32_key(&mut hash, 1); // FaceCullMode::Back
    append_u32_key(&mut hash, 0); // SurfacePattern::None
    append_f32_key(&mut hash, 1.0);
    append_f32_key(&mut hash, 1.0);
    append_f32_key(&mut hash, 0.0);
    append_f32_key(&mut hash, 0.0);
    append_f32_key(&mut hash, 0.08);
    append_f32_key(&mut hash, 0.0);
    append_f32_key(&mut hash, 0.0);
    append_f32_key(&mut hash, 0.0);
    append_f32_key(&mut hash, 0.0);
    append_f32_key(&mut hash, 0.0);
    append_key(&mut hash, material.name.as_bytes());
    material.permutation_flags = flags;
    material.permutation_key = hash;
    material.pipeline_tag = material_pipeline_tag(material, flags);
}

fn build_metadata(
    source_hash: [u8; 32],
    options_hash: [u8; 32],
    options: CompileOptions,
    material_count: usize,
    scene_node_count: usize,
    meshes: &[MeshChunk],
    collision_meshes: &[CollisionMesh],
) -> CacheMetadata {
    let mut diagnostics = MeshDiagnostics::default();
    let mut total_vertices = 0u64;
    let mut total_indices = 0u64;
    for mesh in meshes {
        diagnostics.add(mesh.diagnostics);
        total_vertices += mesh.mesh.vertices.len() as u64;
        total_indices += mesh.mesh.indices.len() as u64;
    }
    let total_collision_triangles = collision_meshes
        .iter()
        .map(|mesh| mesh.triangles.len() as u64)
        .sum();
    CacheMetadata {
        cache_version: CACHE_VERSION,
        compiler_version: COMPILER_VERSION,
        source_hash,
        options_hash,
        compiler_options: options.summary(),
        material_count: material_count as u32,
        mesh_count: meshes.len() as u32,
        collision_mesh_count: collision_meshes.len() as u32,
        scene_node_count: scene_node_count as u32,
        total_vertices,
        total_indices,
        total_collision_triangles,
        diagnostics,
    }
}

fn source_hash(data: &AssetData) -> Result<[u8; 32]> {
    let mut hasher = blake3::Hasher::new();
    hasher.update(b"aster.scene.v1\0");
    hasher.update(&data.source_bytes);
    for (index, bytes) in data.buffers.iter().enumerate() {
        hasher.update(b"buffer\0");
        hasher.update(data.buffer_uris[index].as_bytes());
        hasher.update(&(bytes.len() as u64).to_le_bytes());
        hasher.update(bytes);
    }
    if let Some(images) = data.root.get("images").and_then(Value::as_array) {
        for image in images {
            if let Some(uri) = image.get("uri").and_then(Value::as_str) {
                hasher.update(b"texture\0");
                hasher.update(uri.as_bytes());
                let path = data.base.join(uri);
                if path.exists() {
                    let bytes = fs::read(path)?;
                    hasher.update(&(bytes.len() as u64).to_le_bytes());
                    hasher.update(&bytes);
                }
            } else if let Some(view_index) = image.get("bufferView").and_then(Value::as_u64) {
                let bytes = buffer_view_payload(data, view_index as usize)?;
                hasher.update(b"texture-buffer-view\0");
                hasher.update(&view_index.to_le_bytes());
                hasher.update(&(bytes.len() as u64).to_le_bytes());
                hasher.update(bytes);
            }
        }
    }
    Ok(*hasher.finalize().as_bytes())
}

fn read_le_u32(bytes: &[u8], offset: usize) -> Result<u32> {
    if offset.checked_add(4).map_or(true, |end| end > bytes.len()) {
        return Err(ContentError::new("GLB header is truncated"));
    }
    Ok(u32::from_le_bytes([
        bytes[offset],
        bytes[offset + 1],
        bytes[offset + 2],
        bytes[offset + 3],
    ]))
}

fn parse_glb(path: &Path, bytes: &[u8]) -> Result<(Value, Option<Vec<u8>>, Vec<u8>)> {
    if bytes.len() < 12 {
        return Err(ContentError::new("GLB file is shorter than its header"));
    }
    if &bytes[0..4] != b"glTF" {
        return Err(ContentError::new("GLB file has an invalid magic"));
    }
    let version = read_le_u32(bytes, 4)?;
    if version != 2 {
        return Err(ContentError::new(format!(
            "unsupported GLB version {version}; expected 2"
        )));
    }
    let declared_len = read_le_u32(bytes, 8)? as usize;
    if declared_len != bytes.len() {
        return Err(ContentError::new(format!(
            "GLB length for '{}' does not match the file",
            path.display()
        )));
    }

    let mut cursor = 12usize;
    let mut json_chunk = None;
    let mut bin_chunk = None;
    while cursor < bytes.len() {
        let chunk_len = read_le_u32(bytes, cursor)? as usize;
        let chunk_ty = read_le_u32(bytes, cursor + 4)?;
        cursor += 8;
        let end = cursor
            .checked_add(chunk_len)
            .ok_or_else(|| ContentError::new("GLB chunk range overflows"))?;
        if end > bytes.len() {
            return Err(ContentError::new("GLB chunk range is outside the file"));
        }
        match chunk_ty {
            0x4E4F534A => json_chunk = Some(bytes[cursor..end].to_vec()),
            0x004E4942 => bin_chunk = Some(bytes[cursor..end].to_vec()),
            _ => {}
        }
        cursor = end;
    }
    let json_bytes =
        json_chunk.ok_or_else(|| ContentError::new("GLB is missing its JSON chunk"))?;
    let root: Value = serde_json::from_slice(&json_bytes)?;
    Ok((root, bin_chunk, json_bytes))
}

fn load_asset_data(path: &Path) -> Result<AssetData> {
    let source_bytes = fs::read(path)
        .map_err(|error| ContentError::new(format!("could not open scene asset file: {error}")))?;
    let extension = path
        .extension()
        .and_then(|value| value.to_str())
        .unwrap_or("")
        .to_ascii_lowercase();
    let (root, glb_bin) = if extension == "glb" {
        let (root, bin, _json_bytes) = parse_glb(path, &source_bytes)?;
        (root, bin)
    } else {
        (serde_json::from_slice(&source_bytes)?, None)
    };
    let base = path.parent().unwrap_or_else(|| Path::new("")).to_path_buf();
    let mut buffers = Vec::new();
    let mut buffer_uris = Vec::new();
    for (buffer_index, buffer) in required_array(&root, "buffers")?.iter().enumerate() {
        let uri = buffer
            .get("uri")
            .and_then(Value::as_str)
            .map(str::to_string)
            .unwrap_or_default();
        let bytes = if uri.is_empty() {
            if buffer_index == 0 {
                glb_bin.clone().ok_or_else(|| {
                    ContentError::new("GLB buffer has no uri and the file has no BIN chunk")
                })?
            } else {
                return Err(ContentError::new(
                    "only the first GLB buffer may omit uri in Asset v1",
                ));
            }
        } else {
            fs::read(base.join(&uri)).map_err(|error| {
                ContentError::new(format!(
                    "could not open scene asset buffer '{uri}': {error}"
                ))
            })?
        };
        if let Some(byte_length) = buffer.get("byteLength").and_then(Value::as_u64) {
            if byte_length as usize > bytes.len() {
                return Err(ContentError::new(format!(
                    "scene asset buffer '{uri}' byteLength exceeds the file"
                )));
            }
        }
        buffer_uris.push(if uri.is_empty() {
            format!("__glb_bin_{buffer_index}__")
        } else {
            uri
        });
        buffers.push(bytes);
    }

    let mut views = Vec::new();
    for entry in required_array(&root, "bufferViews")? {
        views.push(BufferView {
            buffer: integer(entry, "buffer", 0),
            byte_offset: integer(entry, "byteOffset", 0),
            byte_length: integer(entry, "byteLength", 0),
            byte_stride: integer(entry, "byteStride", 0),
        });
    }

    let mut accessors = Vec::new();
    for entry in required_array(&root, "accessors")? {
        accessors.push(Accessor {
            buffer_view: integer(entry, "bufferView", 0),
            byte_offset: integer(entry, "byteOffset", 0),
            component_type: integer(entry, "componentType", 0) as u32,
            count: integer(entry, "count", 0),
            ty: required_str(entry, "type")?.to_string(),
        });
    }

    Ok(AssetData {
        root,
        buffers,
        buffer_uris,
        views,
        accessors,
        base,
        source_bytes,
    })
}

struct ImportContext<'a> {
    data: &'a AssetData,
    options: CompileOptions,
    scene_nodes: Vec<SceneNode>,
    meshes: Vec<MeshChunk>,
}

fn import_node(
    context: &mut ImportContext<'_>,
    node_index: usize,
    parent: i32,
    parent_matrix: Mat4,
) -> Result<()> {
    let node = array_at(&context.data.root, "nodes", node_index)?;
    let local = node_matrix(node)?;
    let world = multiply(parent_matrix, local);
    let first_mesh = context.meshes.len() as u32;

    if let Some(mesh_index_value) = node.get("mesh") {
        let mesh_index = mesh_index_value
            .as_u64()
            .ok_or_else(|| ContentError::new("node mesh index is not an integer"))?
            as usize;
        let mesh = array_at(&context.data.root, "meshes", mesh_index)?;
        let mesh_name = mesh.get("name").and_then(Value::as_str).unwrap_or("");
        let node_name = node.get("name").and_then(Value::as_str).unwrap_or("");
        for primitive in required_array(mesh, "primitives")? {
            let mut chunk =
                mesh_from_primitive(context.data, primitive, world, context.options.unit_scale)?;
            chunk.name = if node_name.is_empty() {
                mesh_name.to_string()
            } else if mesh_name.is_empty() {
                node_name.to_string()
            } else {
                format!("{node_name}/{mesh_name}")
            };
            context.meshes.push(chunk);
        }
    }

    let mesh_count = context.meshes.len() as u32 - first_mesh;
    let current_index = context.scene_nodes.len() as i32;
    context.scene_nodes.push(SceneNode {
        name: node
            .get("name")
            .and_then(Value::as_str)
            .unwrap_or("")
            .to_string(),
        parent,
        transform: world,
        first_mesh,
        mesh_count,
    });

    if let Some(children) = node.get("children").and_then(Value::as_array) {
        for child in children {
            let child_index = child
                .as_u64()
                .ok_or_else(|| ContentError::new("node child index is not an integer"))?
                as usize;
            import_node(context, child_index, current_index, world)?;
        }
    }
    Ok(())
}

fn import_material(data: &AssetData, source: &Value) -> Result<Material> {
    let mut material = Material {
        name: source
            .get("name")
            .and_then(Value::as_str)
            .unwrap_or("")
            .to_string(),
        ..Material::default()
    };

    if let Some(pbr) = source.get("pbrMetallicRoughness") {
        if let Some(base) = pbr.get("baseColorFactor").and_then(Value::as_array) {
            if base.len() >= 3 {
                material.base_color = Vec3 {
                    x: number_or(&base[0], 1.0),
                    y: number_or(&base[1], 1.0),
                    z: number_or(&base[2], 1.0),
                };
            }
            if base.len() >= 4 {
                material.opacity = number_or(&base[3], 1.0);
            }
        }
        if let Some(value) = pbr.get("metallicFactor") {
            material.metallic = number_or(value, material.metallic);
        }
        if let Some(value) = pbr.get("roughnessFactor") {
            material.roughness = number_or(value, material.roughness);
        }
        if let Some(dep) = texture_dependency(data, pbr.get("baseColorTexture"), "base_color")? {
            material.has_base_color_texture = true;
            material.texture_dependencies.push(dep);
        }
        if let Some(dep) = texture_dependency(
            data,
            pbr.get("metallicRoughnessTexture"),
            "metallic_roughness",
        )? {
            material.has_metallic_roughness_texture = true;
            material.texture_dependencies.push(dep);
        }
    }

    if let Some(emissive) = source.get("emissiveFactor").and_then(Value::as_array) {
        if emissive.len() >= 3 {
            material.emission_color = Vec3 {
                x: number_or(&emissive[0], 0.0),
                y: number_or(&emissive[1], 0.0),
                z: number_or(&emissive[2], 0.0),
            };
            material.emission_strength = material
                .emission_color
                .x
                .max(material.emission_color.y)
                .max(material.emission_color.z);
        }
    }
    if let Some(dep) = texture_dependency(data, source.get("normalTexture"), "normal")? {
        material.has_normal_texture = true;
        material.texture_dependencies.push(dep);
    }
    if let Some(dep) = texture_dependency(data, source.get("occlusionTexture"), "occlusion")? {
        material.has_occlusion_texture = true;
        material.texture_dependencies.push(dep);
    }

    material.double_sided = source
        .get("doubleSided")
        .and_then(Value::as_bool)
        .unwrap_or(false);
    if let Some(mode) = source.get("alphaMode").and_then(Value::as_str) {
        match mode {
            "BLEND" => {
                material.alpha_mode = AlphaMode::Blend;
                material.depth_write = DepthWrite::Disabled;
            }
            "MASK" => material.alpha_mode = AlphaMode::Masked,
            _ => material.alpha_mode = AlphaMode::Opaque,
        }
    }
    compile_material_for_rendering(&mut material);
    Ok(material)
}

fn texture_dependency(
    data: &AssetData,
    texture: Option<&Value>,
    role: &str,
) -> Result<Option<TextureDependency>> {
    let Some(texture) = texture else {
        return Ok(None);
    };
    let texture_index = integer(texture, "index", usize::MAX);
    let textures = data.root.get("textures").and_then(Value::as_array);
    let images = data.root.get("images").and_then(Value::as_array);
    let image = textures
        .and_then(|entries| entries.get(texture_index))
        .and_then(|entry| entry.get("source"))
        .and_then(Value::as_u64)
        .and_then(|source| images.and_then(|entries| entries.get(source as usize)));
    let uri = image
        .and_then(|image| image.get("uri"))
        .and_then(Value::as_str)
        .unwrap_or("")
        .to_string();
    if uri.is_empty() {
        if let Some(view_index) = image
            .and_then(|image| image.get("bufferView"))
            .and_then(Value::as_u64)
        {
            let bytes = buffer_view_payload(data, view_index as usize)?;
            return Ok(Some(TextureDependency {
                role: role.to_string(),
                uri: format!("embedded:{view_index}"),
                present: true,
                hash: *blake3::hash(bytes).as_bytes(),
            }));
        }
        return Ok(Some(TextureDependency {
            role: role.to_string(),
            uri,
            present: false,
            hash: [0u8; 32],
        }));
    }
    let path = data.base.join(&uri);
    if path.exists() {
        let bytes = fs::read(path)?;
        Ok(Some(TextureDependency {
            role: role.to_string(),
            uri,
            present: true,
            hash: *blake3::hash(&bytes).as_bytes(),
        }))
    } else {
        Ok(Some(TextureDependency {
            role: role.to_string(),
            uri,
            present: false,
            hash: [0u8; 32],
        }))
    }
}

fn buffer_view_payload(data: &AssetData, view_index: usize) -> Result<&[u8]> {
    let buffer_view = view(data, view_index)?;
    let buffer = data
        .buffers
        .get(buffer_view.buffer)
        .ok_or_else(|| ContentError::new("scene asset references an invalid buffer"))?;
    let start = buffer_view.byte_offset;
    let len = buffer_view.byte_length;
    let end = start
        .checked_add(len)
        .ok_or_else(|| ContentError::new("scene asset buffer view range overflows"))?;
    if end > buffer.len() {
        return Err(ContentError::new(
            "scene asset buffer view range is outside the buffer",
        ));
    }
    Ok(&buffer[start..end])
}

fn mesh_from_primitive(
    data: &AssetData,
    primitive: &Value,
    matrix: Mat4,
    unit_scale: f32,
) -> Result<MeshChunk> {
    let attributes = primitive
        .get("attributes")
        .ok_or_else(|| ContentError::new("scene asset primitive is missing attributes"))?;
    let position_accessor = attributes
        .get("POSITION")
        .and_then(Value::as_u64)
        .ok_or_else(|| ContentError::new("scene asset primitive is missing POSITION"))?
        as usize;
    let normal_accessor = attributes
        .get("NORMAL")
        .and_then(Value::as_u64)
        .map(|v| v as usize);
    let texcoord_accessor = attributes
        .get("TEXCOORD_0")
        .and_then(Value::as_u64)
        .map(|v| v as usize);
    let tangent_accessor = attributes
        .get("TANGENT")
        .and_then(Value::as_u64)
        .map(|v| v as usize);
    let vertex_count = accessor(data, position_accessor)?.count;
    let mut mesh = Mesh {
        vertices: Vec::with_capacity(vertex_count),
        indices: Vec::new(),
    };

    for i in 0..vertex_count {
        let mut vertex = Vertex::default();
        vertex.position = read_vec3(data, position_accessor, i)?;
        if let Some(index) = normal_accessor {
            vertex.normal = read_vec3(data, index, i)?;
        }
        if let Some(index) = texcoord_accessor {
            vertex.uv = read_vec2(data, index, i)?;
        }
        if let Some(index) = tangent_accessor {
            vertex.tangent = read_vec4(data, index, i)?;
        }
        vertex.position = transform_point(matrix, vertex.position) * unit_scale;
        vertex.normal = transform_normal(matrix, vertex.normal);
        let tangent = normalize(transform_vector(
            matrix,
            Vec3 {
                x: vertex.tangent.x,
                y: vertex.tangent.y,
                z: vertex.tangent.z,
            },
        ));
        vertex.tangent = Vec4 {
            x: tangent.x,
            y: tangent.y,
            z: tangent.z,
            w: vertex.tangent.w,
        };
        mesh.vertices.push(vertex);
    }

    let source_indices = read_element_indices(data, primitive, mesh.vertices.len())?;
    let mode = primitive.get("mode").and_then(Value::as_u64).unwrap_or(4) as u32;
    mesh.indices = triangulate(mode, &source_indices)?;
    let material_slot = primitive
        .get("material")
        .and_then(Value::as_u64)
        .map(|value| value as u32 + 1)
        .unwrap_or(0);
    let generate_tangents = tangent_accessor.is_none();
    let (mesh, diagnostics) = prepare_mesh(mesh, generate_tangents)?;
    let bounds = mesh_bounds(&mesh);
    Ok(MeshChunk {
        name: String::new(),
        material_slot,
        bounds,
        diagnostics,
        mesh,
    })
}

fn read_element_indices(
    data: &AssetData,
    primitive: &Value,
    vertex_count: usize,
) -> Result<Vec<u32>> {
    let Some(index_value) = primitive.get("indices") else {
        return Ok((0..vertex_count as u32).collect());
    };
    let accessor_index = index_value
        .as_u64()
        .ok_or_else(|| ContentError::new("primitive indices accessor is not an integer"))?
        as usize;
    let index_accessor = accessor(data, accessor_index)?;
    let mut out = Vec::with_capacity(index_accessor.count);
    for i in 0..index_accessor.count {
        out.push(read_index(data, index_accessor, i)?);
    }
    Ok(out)
}

fn triangulate(mode: u32, source: &[u32]) -> Result<Vec<u32>> {
    match mode {
        4 => {
            if source.len() % 3 != 0 {
                Err(ContentError::new(
                    "scene asset triangle primitive has non-triangular index count",
                ))
            } else {
                Ok(source.to_vec())
            }
        }
        5 => {
            let mut out = Vec::new();
            for i in 0..source.len().saturating_sub(2) {
                if i % 2 == 0 {
                    out.extend_from_slice(&[source[i], source[i + 1], source[i + 2]]);
                } else {
                    out.extend_from_slice(&[source[i + 1], source[i], source[i + 2]]);
                }
            }
            Ok(out)
        }
        6 => {
            let mut out = Vec::new();
            for i in 1..source.len().saturating_sub(1) {
                out.extend_from_slice(&[source[0], source[i], source[i + 1]]);
            }
            Ok(out)
        }
        _ => Err(ContentError::new(
            "only triangle scene asset primitives can be imported as meshes",
        )),
    }
}

fn prepare_mesh(
    mut mesh: Mesh,
    generate_tangents_enabled: bool,
) -> Result<(Mesh, MeshDiagnostics)> {
    let mut diagnostics = MeshDiagnostics {
        input_vertices: mesh.vertices.len() as u64,
        input_indices: mesh.indices.len() as u64,
        ..MeshDiagnostics::default()
    };
    validate_mesh(&mesh)?;
    drop_degenerate_triangles(&mut mesh, &mut diagnostics)?;
    diagnostics.invalid_normals = count_invalid_normals(&mesh) as u64;
    if diagnostics.invalid_normals > 0 {
        rebuild_normals(&mut mesh);
    }
    if generate_tangents_enabled {
        generate_tangents(&mut mesh, &mut diagnostics);
        compact_equivalent_vertices(&mut mesh, &mut diagnostics);
    }
    optimize_indices_and_vertices(&mut mesh);
    diagnostics.output_vertices = mesh.vertices.len() as u64;
    diagnostics.output_indices = mesh.indices.len() as u64;
    Ok((mesh, diagnostics))
}

fn validate_mesh(mesh: &Mesh) -> Result<()> {
    if mesh.vertices.is_empty() {
        return Err(ContentError::new("mesh contains no vertices"));
    }
    if mesh.indices.is_empty() {
        return Err(ContentError::new("mesh contains no indices"));
    }
    if mesh.indices.len() % 3 != 0 {
        return Err(ContentError::new("mesh index count must be divisible by 3"));
    }
    for vertex in &mesh.vertices {
        if !finite_vec3(vertex.position) || !finite_vec2(vertex.uv) {
            return Err(ContentError::new(
                "mesh contains non-finite vertex attributes",
            ));
        }
    }
    for &index in &mesh.indices {
        if index as usize >= mesh.vertices.len() {
            return Err(ContentError::new(
                "mesh contains an index outside the vertex buffer",
            ));
        }
    }
    Ok(())
}

fn drop_degenerate_triangles(mesh: &mut Mesh, diagnostics: &mut MeshDiagnostics) -> Result<()> {
    let mut indices = Vec::with_capacity(mesh.indices.len());
    for triangle in mesh.indices.chunks_exact(3) {
        let a = triangle[0];
        let b = triangle[1];
        let c = triangle[2];
        if a == b
            || b == c
            || c == a
            || degenerate_triangle(
                mesh.vertices[a as usize],
                mesh.vertices[b as usize],
                mesh.vertices[c as usize],
            )
        {
            diagnostics.degenerate_triangles += 1;
            continue;
        }
        indices.extend_from_slice(triangle);
    }
    if indices.is_empty() {
        return Err(ContentError::new(
            "mesh contains no renderable triangles after validation",
        ));
    }
    mesh.indices = indices;
    Ok(())
}

fn degenerate_triangle(a: Vertex, b: Vertex, c: Vertex) -> bool {
    let ab = b.position - a.position;
    let ac = c.position - a.position;
    let bc = c.position - b.position;
    let edge_scale_squared = dot(ab, ab).max(dot(ac, ac)).max(dot(bc, bc));
    if edge_scale_squared <= f32::EPSILON {
        return true;
    }
    let area = cross(ab, ac);
    dot(area, area) <= edge_scale_squared * edge_scale_squared * RELATIVE_AREA_TOLERANCE
}

fn valid_normal(normal: Vec3) -> bool {
    let len = length(normal);
    finite_vec3(normal) && len > 0.5 && len < 1.5
}

fn count_invalid_normals(mesh: &Mesh) -> usize {
    mesh.vertices
        .iter()
        .filter(|vertex| !valid_normal(vertex.normal))
        .count()
}

fn rebuild_normals(mesh: &mut Mesh) {
    for vertex in &mut mesh.vertices {
        vertex.normal = Vec3::default();
    }
    for triangle in mesh.indices.chunks_exact(3) {
        let ia = triangle[0] as usize;
        let ib = triangle[1] as usize;
        let ic = triangle[2] as usize;
        let normal = cross(
            mesh.vertices[ib].position - mesh.vertices[ia].position,
            mesh.vertices[ic].position - mesh.vertices[ia].position,
        );
        mesh.vertices[ia].normal = mesh.vertices[ia].normal + normal;
        mesh.vertices[ib].normal = mesh.vertices[ib].normal + normal;
        mesh.vertices[ic].normal = mesh.vertices[ic].normal + normal;
    }
    for vertex in &mut mesh.vertices {
        vertex.normal = normalize(vertex.normal);
        if !valid_normal(vertex.normal) {
            vertex.normal = Vec3 {
                x: 0.0,
                y: 1.0,
                z: 0.0,
            };
        }
    }
}

fn generate_tangents(mesh: &mut Mesh, diagnostics: &mut MeshDiagnostics) {
    let mut expanded = Mesh {
        vertices: Vec::with_capacity(mesh.indices.len()),
        indices: Vec::with_capacity(mesh.indices.len()),
    };
    for &index in &mesh.indices {
        expanded.indices.push(expanded.vertices.len() as u32);
        expanded.vertices.push(mesh.vertices[index as usize]);
    }

    for triangle_index in (0..expanded.indices.len()).step_by(3) {
        let ia = expanded.indices[triangle_index] as usize;
        let ib = expanded.indices[triangle_index + 1] as usize;
        let ic = expanded.indices[triangle_index + 2] as usize;
        let a = expanded.vertices[ia];
        let b = expanded.vertices[ib];
        let c = expanded.vertices[ic];
        let edge_ab = b.position - a.position;
        let edge_ac = c.position - a.position;
        let uv_ab = b.uv - a.uv;
        let uv_ac = c.uv - a.uv;
        let denominator = uv_ab.x * uv_ac.y - uv_ac.x * uv_ab.y;

        let (tangent, sign) = if denominator.abs() > 0.000001 {
            let inv = 1.0 / denominator;
            let tangent = (edge_ab * uv_ac.y - edge_ac * uv_ab.y) * inv;
            let bitangent = (edge_ac * uv_ab.x - edge_ab * uv_ac.x) * inv;
            let n = normalize(a.normal + b.normal + c.normal);
            let sign = if dot(cross(n, tangent), bitangent) < 0.0 {
                -1.0
            } else {
                1.0
            };
            (tangent, sign)
        } else {
            let mut n = normalize(a.normal + b.normal + c.normal);
            if length(n) <= 0.0001 {
                n = face_normal(a.position, b.position, c.position);
            }
            let reference = if n.y.abs() > 0.80 {
                Vec3 {
                    x: 1.0,
                    y: 0.0,
                    z: 0.0,
                }
            } else {
                Vec3 {
                    x: 0.0,
                    y: 1.0,
                    z: 0.0,
                }
            };
            (normalize(cross(reference, n)), 1.0)
        };

        for index in [ia, ib, ic] {
            let normal = expanded.vertices[index].normal;
            let mut t = tangent - normal * dot(normal, tangent);
            if length(t) <= 0.0001 {
                let reference = if normal.y.abs() > 0.80 {
                    Vec3 {
                        x: 1.0,
                        y: 0.0,
                        z: 0.0,
                    }
                } else {
                    Vec3 {
                        x: 0.0,
                        y: 1.0,
                        z: 0.0,
                    }
                };
                t = cross(reference, normal);
            }
            t = normalize(t);
            expanded.vertices[index].tangent = Vec4 {
                x: t.x,
                y: t.y,
                z: t.z,
                w: sign,
            };
        }
    }
    diagnostics.generated_tangents = expanded.vertices.len() as u64;
    *mesh = expanded;
}

fn compact_equivalent_vertices(mesh: &mut Mesh, diagnostics: &mut MeshDiagnostics) {
    let mut remap = HashMap::<Vec<u8>, u32>::new();
    let mut vertices = Vec::with_capacity(mesh.vertices.len());
    let mut indices = Vec::with_capacity(mesh.indices.len());
    for &source_index in &mesh.indices {
        let vertex = mesh.vertices[source_index as usize];
        let key = vertex_key(vertex);
        if let Some(&existing) = remap.get(&key) {
            indices.push(existing);
            continue;
        }
        let new_index = vertices.len() as u32;
        vertices.push(vertex);
        remap.insert(key, new_index);
        indices.push(new_index);
    }
    diagnostics.remapped_vertices = mesh.vertices.len().saturating_sub(vertices.len()) as u64;
    mesh.vertices = vertices;
    mesh.indices = indices;
}

fn optimize_indices_and_vertices(mesh: &mut Mesh) {
    let triangle_count = mesh.indices.len() / 3;
    let mut vertex_triangles = vec![Vec::<u32>::new(); mesh.vertices.len()];
    for triangle in 0..triangle_count {
        let base = triangle * 3;
        vertex_triangles[mesh.indices[base] as usize].push(triangle as u32);
        vertex_triangles[mesh.indices[base + 1] as usize].push(triangle as u32);
        vertex_triangles[mesh.indices[base + 2] as usize].push(triangle as u32);
    }
    let mut reordered = Vec::with_capacity(mesh.indices.len());
    let mut state = vec![0u8; triangle_count];
    let mut pending = std::collections::VecDeque::<u32>::new();
    for seed in 0..triangle_count {
        if state[seed] == 0 {
            state[seed] = 1;
            pending.push_back(seed as u32);
        }
        while let Some(triangle) = pending.pop_front() {
            let triangle = triangle as usize;
            if state[triangle] == 2 {
                continue;
            }
            state[triangle] = 2;
            let base = triangle * 3;
            let corners = [
                mesh.indices[base],
                mesh.indices[base + 1],
                mesh.indices[base + 2],
            ];
            reordered.extend_from_slice(&corners);
            for vertex in corners {
                for &adjacent in &vertex_triangles[vertex as usize] {
                    if state[adjacent as usize] == 0 {
                        state[adjacent as usize] = 1;
                        pending.push_back(adjacent);
                    }
                }
            }
        }
    }
    if reordered.len() == mesh.indices.len() {
        mesh.indices = reordered;
    }

    let mut remap = vec![u32::MAX; mesh.vertices.len()];
    let mut vertices = Vec::with_capacity(mesh.vertices.len());
    for index in &mut mesh.indices {
        if remap[*index as usize] == u32::MAX {
            remap[*index as usize] = vertices.len() as u32;
            vertices.push(mesh.vertices[*index as usize]);
        }
        *index = remap[*index as usize];
    }
    mesh.vertices = vertices;
}

fn build_collision_meshes(meshes: &[MeshChunk]) -> Vec<CollisionMesh> {
    meshes
        .iter()
        .enumerate()
        .map(|(mesh_index, mesh)| {
            let mut triangles = Vec::with_capacity(mesh.mesh.indices.len() / 3);
            for triangle in mesh.mesh.indices.chunks_exact(3) {
                let a = mesh.mesh.vertices[triangle[0] as usize].position;
                let b = mesh.mesh.vertices[triangle[1] as usize].position;
                let c = mesh.mesh.vertices[triangle[2] as usize].position;
                triangles.push(CollisionTriangle {
                    a,
                    b,
                    c,
                    normal: face_normal(a, b, c),
                });
            }
            CollisionMesh {
                name: mesh.name.clone(),
                mesh_chunk: mesh_index as u32,
                bounds: mesh.bounds,
                triangles,
            }
        })
        .collect()
}

fn apply_origin_policy(meshes: &mut [MeshChunk], policy: OriginPolicy) {
    if policy == OriginPolicy::Keep {
        return;
    }
    let mut bounds = Bounds::empty();
    for mesh in meshes.iter() {
        for vertex in &mesh.mesh.vertices {
            bounds.include(vertex.position);
        }
    }
    if !bounds.valid {
        return;
    }
    let offset = match policy {
        OriginPolicy::Keep => Vec3::default(),
        OriginPolicy::Center => (bounds.min + bounds.max) * 0.5,
        OriginPolicy::CenterOnGround => Vec3 {
            x: (bounds.min.x + bounds.max.x) * 0.5,
            y: bounds.min.y,
            z: (bounds.min.z + bounds.max.z) * 0.5,
        },
    };
    for mesh in meshes {
        for vertex in &mut mesh.mesh.vertices {
            vertex.position = vertex.position - offset;
        }
        mesh.bounds = mesh_bounds(&mesh.mesh);
    }
}

fn mesh_bounds(mesh: &Mesh) -> Bounds {
    let mut bounds = Bounds::empty();
    for vertex in &mesh.vertices {
        bounds.include(vertex.position);
    }
    bounds
}

fn vertex_key(vertex: Vertex) -> Vec<u8> {
    let mut out = Vec::with_capacity(15 * 4);
    for value in [
        vertex.position.x,
        vertex.position.y,
        vertex.position.z,
        vertex.normal.x,
        vertex.normal.y,
        vertex.normal.z,
        vertex.uv.x,
        vertex.uv.y,
        vertex.tangent.x,
        vertex.tangent.y,
        vertex.tangent.z,
        vertex.tangent.w,
        vertex.ambient_occlusion,
    ] {
        out.extend_from_slice(&value.to_le_bytes());
    }
    out
}

fn node_matrix(node: &Value) -> Result<Mat4> {
    if let Some(matrix) = node.get("matrix").and_then(Value::as_array) {
        if matrix.len() < 16 {
            return Err(ContentError::new("node matrix has fewer than 16 values"));
        }
        let mut out = [0.0f32; 16];
        for i in 0..16 {
            out[i] = number_or(&matrix[i], 0.0);
        }
        return Ok(Mat4 { m: out });
    }
    let mut out = identity();
    if let Some(translation_value) = node.get("translation").and_then(Value::as_array) {
        if translation_value.len() >= 3 {
            out = multiply(
                out,
                translation(Vec3 {
                    x: number_or(&translation_value[0], 0.0),
                    y: number_or(&translation_value[1], 0.0),
                    z: number_or(&translation_value[2], 0.0),
                }),
            );
        }
    }
    if let Some(scale_value) = node.get("scale").and_then(Value::as_array) {
        if scale_value.len() >= 3 {
            out = multiply(
                out,
                scale(Vec3 {
                    x: number_or(&scale_value[0], 1.0),
                    y: number_or(&scale_value[1], 1.0),
                    z: number_or(&scale_value[2], 1.0),
                }),
            );
        }
    }
    Ok(out)
}

fn identity() -> Mat4 {
    let mut out = Mat4 { m: [0.0; 16] };
    out.m[0] = 1.0;
    out.m[5] = 1.0;
    out.m[10] = 1.0;
    out.m[15] = 1.0;
    out
}

fn translation(offset: Vec3) -> Mat4 {
    let mut out = identity();
    out.m[12] = offset.x;
    out.m[13] = offset.y;
    out.m[14] = offset.z;
    out
}

fn scale(value: Vec3) -> Mat4 {
    let mut out = identity();
    out.m[0] = value.x;
    out.m[5] = value.y;
    out.m[10] = value.z;
    out
}

fn multiply(lhs: Mat4, rhs: Mat4) -> Mat4 {
    let mut out = Mat4 { m: [0.0; 16] };
    for column in 0..4 {
        for row in 0..4 {
            let mut sum = 0.0;
            for k in 0..4 {
                sum += lhs.m[k * 4 + row] * rhs.m[column * 4 + k];
            }
            out.m[column * 4 + row] = sum;
        }
    }
    out
}

fn transform_point(matrix: Mat4, point: Vec3) -> Vec3 {
    let x = matrix.m[0] * point.x + matrix.m[4] * point.y + matrix.m[8] * point.z + matrix.m[12];
    let y = matrix.m[1] * point.x + matrix.m[5] * point.y + matrix.m[9] * point.z + matrix.m[13];
    let z = matrix.m[2] * point.x + matrix.m[6] * point.y + matrix.m[10] * point.z + matrix.m[14];
    let w = matrix.m[3] * point.x + matrix.m[7] * point.y + matrix.m[11] * point.z + matrix.m[15];
    if w.abs() <= 0.000001 {
        Vec3 { x, y, z }
    } else {
        Vec3 {
            x: x / w,
            y: y / w,
            z: z / w,
        }
    }
}

fn transform_vector(matrix: Mat4, value: Vec3) -> Vec3 {
    Vec3 {
        x: matrix.m[0] * value.x + matrix.m[4] * value.y + matrix.m[8] * value.z,
        y: matrix.m[1] * value.x + matrix.m[5] * value.y + matrix.m[9] * value.z,
        z: matrix.m[2] * value.x + matrix.m[6] * value.y + matrix.m[10] * value.z,
    }
}

fn transform_normal(matrix: Mat4, value: Vec3) -> Vec3 {
    let inverse_transpose = transpose(inverse(matrix));
    let transformed = transform_vector(inverse_transpose, value);
    if length(transformed) <= 0.0001 {
        normalize(transform_vector(matrix, value))
    } else {
        normalize(transformed)
    }
}

fn transpose(matrix: Mat4) -> Mat4 {
    let mut out = Mat4 { m: [0.0; 16] };
    for column in 0..4 {
        for row in 0..4 {
            out.m[column * 4 + row] = matrix.m[row * 4 + column];
        }
    }
    out
}

fn inverse(matrix: Mat4) -> Mat4 {
    let det = determinant(matrix);
    if det.abs() <= 0.000001 {
        return identity();
    }
    let at = |row: usize, column: usize| matrix.m[column * 4 + row];
    let mut cofactors = Mat4 { m: [0.0; 16] };
    for column in 0..4 {
        for row in 0..4 {
            let mut minor = [0.0f32; 9];
            let mut index = 0;
            for c in 0..4 {
                if c == column {
                    continue;
                }
                for r in 0..4 {
                    if r == row {
                        continue;
                    }
                    minor[index] = at(r, c);
                    index += 1;
                }
            }
            let sign = if (row + column) % 2 == 0 { 1.0 } else { -1.0 };
            cofactors.m[column * 4 + row] = sign
                * determinant3x3(
                    minor[0], minor[3], minor[6], minor[1], minor[4], minor[7], minor[2], minor[5],
                    minor[8],
                );
        }
    }
    let mut adjugate = transpose(cofactors);
    for value in &mut adjugate.m {
        *value /= det;
    }
    adjugate
}

fn determinant(matrix: Mat4) -> f32 {
    let at = |row: usize, column: usize| matrix.m[column * 4 + row];
    let mut det = 0.0;
    for column in 0..4 {
        let mut minor = [0.0f32; 9];
        let mut index = 0;
        for c in 0..4 {
            if c == column {
                continue;
            }
            for r in 1..4 {
                minor[index] = at(r, c);
                index += 1;
            }
        }
        let sign = if column % 2 == 0 { 1.0 } else { -1.0 };
        det += sign
            * at(0, column)
            * determinant3x3(
                minor[0], minor[3], minor[6], minor[1], minor[4], minor[7], minor[2], minor[5],
                minor[8],
            );
    }
    det
}

#[allow(clippy::too_many_arguments)]
fn determinant3x3(
    a00: f32,
    a01: f32,
    a02: f32,
    a10: f32,
    a11: f32,
    a12: f32,
    a20: f32,
    a21: f32,
    a22: f32,
) -> f32 {
    a00 * (a11 * a22 - a12 * a21) - a01 * (a10 * a22 - a12 * a20) + a02 * (a10 * a21 - a11 * a20)
}

fn dot(lhs: Vec3, rhs: Vec3) -> f32 {
    lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z
}

fn cross(lhs: Vec3, rhs: Vec3) -> Vec3 {
    Vec3 {
        x: lhs.y * rhs.z - lhs.z * rhs.y,
        y: lhs.z * rhs.x - lhs.x * rhs.z,
        z: lhs.x * rhs.y - lhs.y * rhs.x,
    }
}

fn length(value: Vec3) -> f32 {
    dot(value, value).sqrt()
}

fn normalize(value: Vec3) -> Vec3 {
    let len = length(value);
    if len <= 0.000001 {
        Vec3::default()
    } else {
        value / len
    }
}

fn face_normal(a: Vec3, b: Vec3, c: Vec3) -> Vec3 {
    let normal = normalize(cross(b - a, c - a));
    if length(normal) <= 0.0001 {
        Vec3 {
            x: 0.0,
            y: 1.0,
            z: 0.0,
        }
    } else {
        normal
    }
}

fn finite_vec2(value: Vec2) -> bool {
    value.x.is_finite() && value.y.is_finite()
}

fn finite_vec3(value: Vec3) -> bool {
    value.x.is_finite() && value.y.is_finite() && value.z.is_finite()
}

fn component_count(ty: &str) -> Result<usize> {
    match ty {
        "SCALAR" => Ok(1),
        "VEC2" => Ok(2),
        "VEC3" => Ok(3),
        "VEC4" => Ok(4),
        "MAT4" => Ok(16),
        _ => Err(ContentError::new(
            "scene asset accessor uses an unsupported component shape",
        )),
    }
}

fn component_size(component_type: u32) -> Result<usize> {
    match component_type {
        5121 => Ok(1),
        5123 => Ok(2),
        5125 | 5126 => Ok(4),
        _ => Err(ContentError::new(
            "scene asset accessor uses an unsupported component type",
        )),
    }
}

fn accessor(data: &AssetData, index: usize) -> Result<&Accessor> {
    data.accessors
        .get(index)
        .ok_or_else(|| ContentError::new("scene asset references an invalid accessor"))
}

fn view(data: &AssetData, index: usize) -> Result<&BufferView> {
    data.views
        .get(index)
        .ok_or_else(|| ContentError::new("scene asset references an invalid buffer view"))
}

fn read_float_component(
    data: &AssetData,
    accessor: &Accessor,
    element: usize,
    component: usize,
) -> Result<f32> {
    let buffer_view = view(data, accessor.buffer_view)?;
    let buffer = data
        .buffers
        .get(buffer_view.buffer)
        .ok_or_else(|| ContentError::new("scene asset references an invalid buffer"))?;
    let component_size = component_size(accessor.component_type)?;
    let stride = if buffer_view.byte_stride == 0 {
        component_count(&accessor.ty)? * component_size
    } else {
        buffer_view.byte_stride
    };
    let offset = buffer_view
        .byte_offset
        .checked_add(accessor.byte_offset)
        .and_then(|value| value.checked_add(element.checked_mul(stride)?))
        .and_then(|value| value.checked_add(component.checked_mul(component_size)?))
        .ok_or_else(|| ContentError::new("scene asset accessor offset overflows"))?;
    match accessor.component_type {
        5126 => read_scalar_f32(buffer, offset),
        5121 => Ok(read_scalar_u8(buffer, offset)? as f32),
        5123 => Ok(read_scalar_u16(buffer, offset)? as f32),
        5125 => Ok(read_scalar_u32(buffer, offset)? as f32),
        _ => Err(ContentError::new(
            "scene asset float accessor component type is unsupported",
        )),
    }
}

fn read_index(data: &AssetData, accessor: &Accessor, element: usize) -> Result<u32> {
    let buffer_view = view(data, accessor.buffer_view)?;
    let buffer = data
        .buffers
        .get(buffer_view.buffer)
        .ok_or_else(|| ContentError::new("scene asset references an invalid buffer"))?;
    let stride = if buffer_view.byte_stride == 0 {
        component_size(accessor.component_type)?
    } else {
        buffer_view.byte_stride
    };
    let offset = buffer_view
        .byte_offset
        .checked_add(accessor.byte_offset)
        .and_then(|value| value.checked_add(element.checked_mul(stride)?))
        .ok_or_else(|| ContentError::new("scene asset index offset overflows"))?;
    match accessor.component_type {
        5121 => Ok(read_scalar_u8(buffer, offset)? as u32),
        5123 => Ok(read_scalar_u16(buffer, offset)? as u32),
        5125 => read_scalar_u32(buffer, offset),
        _ => Err(ContentError::new(
            "scene asset index component type is unsupported",
        )),
    }
}

fn read_vec2(data: &AssetData, accessor_index: usize, element: usize) -> Result<Vec2> {
    let a = accessor(data, accessor_index)?;
    Ok(Vec2 {
        x: read_float_component(data, a, element, 0)?,
        y: read_float_component(data, a, element, 1)?,
    })
}

fn read_vec3(data: &AssetData, accessor_index: usize, element: usize) -> Result<Vec3> {
    let a = accessor(data, accessor_index)?;
    Ok(Vec3 {
        x: read_float_component(data, a, element, 0)?,
        y: read_float_component(data, a, element, 1)?,
        z: read_float_component(data, a, element, 2)?,
    })
}

fn read_vec4(data: &AssetData, accessor_index: usize, element: usize) -> Result<Vec4> {
    let a = accessor(data, accessor_index)?;
    Ok(Vec4 {
        x: read_float_component(data, a, element, 0)?,
        y: read_float_component(data, a, element, 1)?,
        z: read_float_component(data, a, element, 2)?,
        w: read_float_component(data, a, element, 3)?,
    })
}

fn read_scalar_u8(bytes: &[u8], offset: usize) -> Result<u8> {
    bytes
        .get(offset)
        .copied()
        .ok_or_else(|| ContentError::new("scene asset accessor reads past the end of a buffer"))
}

fn read_scalar_u16(bytes: &[u8], offset: usize) -> Result<u16> {
    let data = bytes
        .get(offset..offset + 2)
        .ok_or_else(|| ContentError::new("scene asset accessor reads past the end of a buffer"))?;
    Ok(u16::from_le_bytes([data[0], data[1]]))
}

fn read_scalar_u32(bytes: &[u8], offset: usize) -> Result<u32> {
    let data = bytes
        .get(offset..offset + 4)
        .ok_or_else(|| ContentError::new("scene asset accessor reads past the end of a buffer"))?;
    Ok(u32::from_le_bytes([data[0], data[1], data[2], data[3]]))
}

fn read_scalar_f32(bytes: &[u8], offset: usize) -> Result<f32> {
    Ok(f32::from_bits(read_scalar_u32(bytes, offset)?))
}

fn required_array<'a>(value: &'a Value, key: &str) -> Result<&'a Vec<Value>> {
    value
        .get(key)
        .and_then(Value::as_array)
        .ok_or_else(|| ContentError::new(format!("scene asset is missing required array '{key}'")))
}

fn required_str<'a>(value: &'a Value, key: &str) -> Result<&'a str> {
    value
        .get(key)
        .and_then(Value::as_str)
        .ok_or_else(|| ContentError::new(format!("scene asset is missing required string '{key}'")))
}

fn array_at<'a>(root: &'a Value, key: &str, index: usize) -> Result<&'a Value> {
    required_array(root, key)?
        .get(index)
        .ok_or_else(|| ContentError::new(format!("scene asset references invalid {key} index")))
}

fn integer(value: &Value, key: &str, fallback: usize) -> usize {
    value
        .get(key)
        .and_then(Value::as_u64)
        .unwrap_or(fallback as u64) as usize
}

fn number_or(value: &Value, fallback: f32) -> f32 {
    value.as_f64().map(|value| value as f32).unwrap_or(fallback)
}

#[derive(Clone, Copy)]
struct ChunkEntry {
    kind: u32,
    offset: u64,
    size: u64,
    hash: [u8; 32],
}

fn write_metadata_chunk(metadata: &CacheMetadata) -> Result<Vec<u8>> {
    let mut out = Vec::new();
    write_u32(&mut out, metadata.cache_version);
    write_u32(&mut out, metadata.compiler_version);
    out.extend_from_slice(&metadata.source_hash);
    out.extend_from_slice(&metadata.options_hash);
    write_string(&mut out, &metadata.compiler_options)?;
    write_u32(&mut out, metadata.material_count);
    write_u32(&mut out, metadata.mesh_count);
    write_u32(&mut out, metadata.collision_mesh_count);
    write_u32(&mut out, metadata.scene_node_count);
    write_u64(&mut out, metadata.total_vertices);
    write_u64(&mut out, metadata.total_indices);
    write_u64(&mut out, metadata.total_collision_triangles);
    write_diagnostics(&mut out, metadata.diagnostics);
    Ok(out)
}

fn read_metadata_chunk(bytes: &[u8]) -> Result<CacheMetadata> {
    let mut cursor = 0usize;
    let cache_version = read_u32(bytes, &mut cursor)?;
    let compiler_version = read_u32(bytes, &mut cursor)?;
    let source_hash = read_hash(bytes, &mut cursor)?;
    let options_hash = read_hash(bytes, &mut cursor)?;
    let compiler_options = read_string(bytes, &mut cursor)?;
    let material_count = read_u32(bytes, &mut cursor)?;
    let mesh_count = read_u32(bytes, &mut cursor)?;
    let collision_mesh_count = read_u32(bytes, &mut cursor)?;
    let scene_node_count = read_u32(bytes, &mut cursor)?;
    let total_vertices = read_u64(bytes, &mut cursor)?;
    let total_indices = read_u64(bytes, &mut cursor)?;
    let total_collision_triangles = read_u64(bytes, &mut cursor)?;
    let diagnostics = read_diagnostics(bytes, &mut cursor)?;
    Ok(CacheMetadata {
        cache_version,
        compiler_version,
        source_hash,
        options_hash,
        compiler_options,
        material_count,
        mesh_count,
        collision_mesh_count,
        scene_node_count,
        total_vertices,
        total_indices,
        total_collision_triangles,
        diagnostics,
    })
}

fn write_scene_graph_chunk(nodes: &[SceneNode]) -> Result<Vec<u8>> {
    let mut out = Vec::new();
    write_u32(&mut out, checked_usize_to_u32(nodes.len())?);
    for node in nodes {
        write_string(&mut out, &node.name)?;
        write_i32(&mut out, node.parent);
        for value in node.transform.m {
            write_f32(&mut out, value);
        }
        write_u32(&mut out, node.first_mesh);
        write_u32(&mut out, node.mesh_count);
    }
    Ok(out)
}

fn read_scene_graph_chunk(bytes: &[u8]) -> Result<Vec<SceneNode>> {
    let mut cursor = 0usize;
    let count = read_u32(bytes, &mut cursor)? as usize;
    let mut out = Vec::with_capacity(count);
    for _ in 0..count {
        let name = read_string(bytes, &mut cursor)?;
        let parent = read_i32(bytes, &mut cursor)?;
        let mut matrix = [0.0f32; 16];
        for value in &mut matrix {
            *value = read_f32(bytes, &mut cursor)?;
        }
        let first_mesh = read_u32(bytes, &mut cursor)?;
        let mesh_count = read_u32(bytes, &mut cursor)?;
        out.push(SceneNode {
            name,
            parent,
            transform: Mat4 { m: matrix },
            first_mesh,
            mesh_count,
        });
    }
    Ok(out)
}

fn write_materials_chunk(materials: &[Material]) -> Result<Vec<u8>> {
    let mut out = Vec::new();
    write_u32(&mut out, checked_usize_to_u32(materials.len())?);
    for material in materials {
        write_string(&mut out, &material.name)?;
        write_vec3(&mut out, material.base_color);
        write_vec3(&mut out, material.emission_color);
        write_f32(&mut out, material.roughness);
        write_f32(&mut out, material.metallic);
        write_f32(&mut out, material.emission_strength);
        write_f32(&mut out, material.opacity);
        out.push(u8::from(material.double_sided));
        out.push(u8::from(material.has_base_color_texture));
        out.push(u8::from(material.has_metallic_roughness_texture));
        out.push(u8::from(material.has_normal_texture));
        out.push(u8::from(material.has_occlusion_texture));
        out.extend_from_slice(&[0u8; 3]);
        write_u32(&mut out, material.alpha_mode as u32);
        write_u32(&mut out, material.depth_write as u32);
        write_u64(&mut out, material.permutation_key);
        write_u32(&mut out, material.permutation_flags);
        write_string(&mut out, &material.pipeline_tag)?;
        write_u32(
            &mut out,
            checked_usize_to_u32(material.texture_dependencies.len())?,
        );
        for dependency in &material.texture_dependencies {
            write_string(&mut out, &dependency.role)?;
            write_string(&mut out, &dependency.uri)?;
            out.push(u8::from(dependency.present));
            out.extend_from_slice(&[0u8; 3]);
            out.extend_from_slice(&dependency.hash);
        }
    }
    Ok(out)
}

fn read_materials_chunk(bytes: &[u8]) -> Result<Vec<Material>> {
    let mut cursor = 0usize;
    let count = read_u32(bytes, &mut cursor)? as usize;
    let mut out = Vec::with_capacity(count);
    for _ in 0..count {
        let name = read_string(bytes, &mut cursor)?;
        let base_color = read_bin_vec3(bytes, &mut cursor)?;
        let emission_color = read_bin_vec3(bytes, &mut cursor)?;
        let roughness = read_f32(bytes, &mut cursor)?;
        let metallic = read_f32(bytes, &mut cursor)?;
        let emission_strength = read_f32(bytes, &mut cursor)?;
        let opacity = read_f32(bytes, &mut cursor)?;
        let double_sided = read_u8(bytes, &mut cursor)? != 0;
        let has_base_color_texture = read_u8(bytes, &mut cursor)? != 0;
        let has_metallic_roughness_texture = read_u8(bytes, &mut cursor)? != 0;
        let has_normal_texture = read_u8(bytes, &mut cursor)? != 0;
        let has_occlusion_texture = read_u8(bytes, &mut cursor)? != 0;
        skip(bytes, &mut cursor, 3)?;
        let alpha_mode = match read_u32(bytes, &mut cursor)? {
            1 => AlphaMode::Masked,
            2 => AlphaMode::DitheredCoverage,
            3 => AlphaMode::Blend,
            _ => AlphaMode::Opaque,
        };
        let depth_write = match read_u32(bytes, &mut cursor)? {
            1 => DepthWrite::Enabled,
            2 => DepthWrite::Disabled,
            _ => DepthWrite::Auto,
        };
        let permutation_key = read_u64(bytes, &mut cursor)?;
        let permutation_flags = read_u32(bytes, &mut cursor)?;
        let pipeline_tag = read_string(bytes, &mut cursor)?;
        let dependency_count = read_u32(bytes, &mut cursor)? as usize;
        let mut texture_dependencies = Vec::with_capacity(dependency_count);
        for _ in 0..dependency_count {
            let role = read_string(bytes, &mut cursor)?;
            let uri = read_string(bytes, &mut cursor)?;
            let present = read_u8(bytes, &mut cursor)? != 0;
            skip(bytes, &mut cursor, 3)?;
            let hash = read_hash(bytes, &mut cursor)?;
            texture_dependencies.push(TextureDependency {
                role,
                uri,
                present,
                hash,
            });
        }
        out.push(Material {
            name,
            base_color,
            emission_color,
            roughness,
            metallic,
            emission_strength,
            opacity,
            double_sided,
            alpha_mode,
            depth_write,
            has_base_color_texture,
            has_metallic_roughness_texture,
            has_normal_texture,
            has_occlusion_texture,
            permutation_key,
            permutation_flags,
            pipeline_tag,
            texture_dependencies,
        });
    }
    Ok(out)
}

fn write_meshes_chunk(meshes: &[MeshChunk]) -> Result<Vec<u8>> {
    let mut out = Vec::new();
    write_u32(&mut out, checked_usize_to_u32(meshes.len())?);
    for chunk in meshes {
        write_string(&mut out, &chunk.name)?;
        write_u32(&mut out, chunk.material_slot);
        write_bounds(&mut out, chunk.bounds);
        write_diagnostics(&mut out, chunk.diagnostics);
        write_u32(&mut out, checked_usize_to_u32(chunk.mesh.vertices.len())?);
        write_u32(&mut out, checked_usize_to_u32(chunk.mesh.indices.len())?);
        for vertex in &chunk.mesh.vertices {
            write_vec3(&mut out, vertex.position);
            write_vec3(&mut out, vertex.normal);
            write_vec2(&mut out, vertex.uv);
            write_vec4(&mut out, vertex.tangent);
            write_f32(&mut out, vertex.ambient_occlusion);
        }
        for &index in &chunk.mesh.indices {
            write_u32(&mut out, index);
        }
    }
    Ok(out)
}

fn read_meshes_chunk(bytes: &[u8]) -> Result<Vec<MeshChunk>> {
    let mut cursor = 0usize;
    let count = read_u32(bytes, &mut cursor)? as usize;
    let mut out = Vec::with_capacity(count);
    for _ in 0..count {
        let name = read_string(bytes, &mut cursor)?;
        let material_slot = read_u32(bytes, &mut cursor)?;
        let bounds = read_bounds(bytes, &mut cursor)?;
        let diagnostics = read_diagnostics(bytes, &mut cursor)?;
        let vertex_count = read_u32(bytes, &mut cursor)? as usize;
        let index_count = read_u32(bytes, &mut cursor)? as usize;
        let mut vertices = Vec::with_capacity(vertex_count);
        for _ in 0..vertex_count {
            vertices.push(Vertex {
                position: read_bin_vec3(bytes, &mut cursor)?,
                normal: read_bin_vec3(bytes, &mut cursor)?,
                uv: read_bin_vec2(bytes, &mut cursor)?,
                tangent: read_bin_vec4(bytes, &mut cursor)?,
                ambient_occlusion: read_f32(bytes, &mut cursor)?,
            });
        }
        let mut indices = Vec::with_capacity(index_count);
        for _ in 0..index_count {
            indices.push(read_u32(bytes, &mut cursor)?);
        }
        out.push(MeshChunk {
            name,
            material_slot,
            bounds,
            diagnostics,
            mesh: Mesh { vertices, indices },
        });
    }
    Ok(out)
}

fn write_collision_chunk(meshes: &[CollisionMesh]) -> Result<Vec<u8>> {
    let mut out = Vec::new();
    write_u32(&mut out, checked_usize_to_u32(meshes.len())?);
    for mesh in meshes {
        write_string(&mut out, &mesh.name)?;
        write_u32(&mut out, mesh.mesh_chunk);
        write_bounds(&mut out, mesh.bounds);
        write_u32(&mut out, checked_usize_to_u32(mesh.triangles.len())?);
        for triangle in &mesh.triangles {
            write_vec3(&mut out, triangle.a);
            write_vec3(&mut out, triangle.b);
            write_vec3(&mut out, triangle.c);
            write_vec3(&mut out, triangle.normal);
        }
    }
    Ok(out)
}

fn read_collision_chunk(bytes: &[u8]) -> Result<Vec<CollisionMesh>> {
    let mut cursor = 0usize;
    let count = read_u32(bytes, &mut cursor)? as usize;
    let mut out = Vec::with_capacity(count);
    for _ in 0..count {
        let name = read_string(bytes, &mut cursor)?;
        let mesh_chunk = read_u32(bytes, &mut cursor)?;
        let bounds = read_bounds(bytes, &mut cursor)?;
        let triangle_count = read_u32(bytes, &mut cursor)? as usize;
        let mut triangles = Vec::with_capacity(triangle_count);
        for _ in 0..triangle_count {
            triangles.push(CollisionTriangle {
                a: read_bin_vec3(bytes, &mut cursor)?,
                b: read_bin_vec3(bytes, &mut cursor)?,
                c: read_bin_vec3(bytes, &mut cursor)?,
                normal: read_bin_vec3(bytes, &mut cursor)?,
            });
        }
        out.push(CollisionMesh {
            name,
            mesh_chunk,
            bounds,
            triangles,
        });
    }
    Ok(out)
}

fn write_diagnostics(out: &mut Vec<u8>, diagnostics: MeshDiagnostics) {
    write_u64(out, diagnostics.input_vertices);
    write_u64(out, diagnostics.input_indices);
    write_u64(out, diagnostics.output_vertices);
    write_u64(out, diagnostics.output_indices);
    write_u64(out, diagnostics.degenerate_triangles);
    write_u64(out, diagnostics.invalid_normals);
    write_u64(out, diagnostics.generated_tangents);
    write_u64(out, diagnostics.remapped_vertices);
}

fn read_diagnostics(bytes: &[u8], cursor: &mut usize) -> Result<MeshDiagnostics> {
    Ok(MeshDiagnostics {
        input_vertices: read_u64(bytes, cursor)?,
        input_indices: read_u64(bytes, cursor)?,
        output_vertices: read_u64(bytes, cursor)?,
        output_indices: read_u64(bytes, cursor)?,
        degenerate_triangles: read_u64(bytes, cursor)?,
        invalid_normals: read_u64(bytes, cursor)?,
        generated_tangents: read_u64(bytes, cursor)?,
        remapped_vertices: read_u64(bytes, cursor)?,
    })
}

fn write_bounds(out: &mut Vec<u8>, bounds: Bounds) {
    write_vec3(out, bounds.min);
    write_vec3(out, bounds.max);
    out.push(u8::from(bounds.valid));
    out.extend_from_slice(&[0u8; 3]);
}

fn read_bounds(bytes: &[u8], cursor: &mut usize) -> Result<Bounds> {
    let min = read_bin_vec3(bytes, cursor)?;
    let max = read_bin_vec3(bytes, cursor)?;
    let valid = read_u8(bytes, cursor)? != 0;
    skip(bytes, cursor, 3)?;
    Ok(Bounds { min, max, valid })
}

fn write_vec2(out: &mut Vec<u8>, value: Vec2) {
    write_f32(out, value.x);
    write_f32(out, value.y);
}

fn write_vec3(out: &mut Vec<u8>, value: Vec3) {
    write_f32(out, value.x);
    write_f32(out, value.y);
    write_f32(out, value.z);
}

fn write_vec4(out: &mut Vec<u8>, value: Vec4) {
    write_f32(out, value.x);
    write_f32(out, value.y);
    write_f32(out, value.z);
    write_f32(out, value.w);
}

fn read_bin_vec2(bytes: &[u8], cursor: &mut usize) -> Result<Vec2> {
    Ok(Vec2 {
        x: read_f32(bytes, cursor)?,
        y: read_f32(bytes, cursor)?,
    })
}

fn read_bin_vec3(bytes: &[u8], cursor: &mut usize) -> Result<Vec3> {
    Ok(Vec3 {
        x: read_f32(bytes, cursor)?,
        y: read_f32(bytes, cursor)?,
        z: read_f32(bytes, cursor)?,
    })
}

fn read_bin_vec4(bytes: &[u8], cursor: &mut usize) -> Result<Vec4> {
    Ok(Vec4 {
        x: read_f32(bytes, cursor)?,
        y: read_f32(bytes, cursor)?,
        z: read_f32(bytes, cursor)?,
        w: read_f32(bytes, cursor)?,
    })
}

fn required_chunk<'a>(entries: &'a HashMap<u32, &'a [u8]>, kind: u32) -> Result<&'a [u8]> {
    entries
        .get(&kind)
        .copied()
        .ok_or_else(|| ContentError::new(format!("compiled asset cache is missing chunk {kind}")))
}

fn write_string(out: &mut Vec<u8>, value: &str) -> Result<()> {
    write_u32(out, checked_usize_to_u32(value.len())?);
    out.extend_from_slice(value.as_bytes());
    Ok(())
}

fn read_string(bytes: &[u8], cursor: &mut usize) -> Result<String> {
    let size = read_u32(bytes, cursor)? as usize;
    let data = read_exact(bytes, cursor, size)?;
    String::from_utf8(data.to_vec())
        .map_err(|_| ContentError::new("compiled asset cache contains invalid UTF-8"))
}

fn write_i32(out: &mut Vec<u8>, value: i32) {
    out.extend_from_slice(&value.to_le_bytes());
}

fn read_i32(bytes: &[u8], cursor: &mut usize) -> Result<i32> {
    let data = read_exact(bytes, cursor, 4)?;
    Ok(i32::from_le_bytes([data[0], data[1], data[2], data[3]]))
}

fn write_u32(out: &mut Vec<u8>, value: u32) {
    out.extend_from_slice(&value.to_le_bytes());
}

fn read_u32(bytes: &[u8], cursor: &mut usize) -> Result<u32> {
    let data = read_exact(bytes, cursor, 4)?;
    Ok(u32::from_le_bytes([data[0], data[1], data[2], data[3]]))
}

fn write_u64(out: &mut Vec<u8>, value: u64) {
    out.extend_from_slice(&value.to_le_bytes());
}

fn read_u64(bytes: &[u8], cursor: &mut usize) -> Result<u64> {
    let data = read_exact(bytes, cursor, 8)?;
    Ok(u64::from_le_bytes([
        data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7],
    ]))
}

fn write_f32(out: &mut Vec<u8>, value: f32) {
    out.extend_from_slice(&value.to_bits().to_le_bytes());
}

fn read_f32(bytes: &[u8], cursor: &mut usize) -> Result<f32> {
    Ok(f32::from_bits(read_u32(bytes, cursor)?))
}

fn read_u8(bytes: &[u8], cursor: &mut usize) -> Result<u8> {
    let data = read_exact(bytes, cursor, 1)?;
    Ok(data[0])
}

fn read_hash(bytes: &[u8], cursor: &mut usize) -> Result<[u8; 32]> {
    let data = read_exact(bytes, cursor, 32)?;
    let mut out = [0u8; 32];
    out.copy_from_slice(data);
    Ok(out)
}

fn skip(bytes: &[u8], cursor: &mut usize, size: usize) -> Result<()> {
    let _ = read_exact(bytes, cursor, size)?;
    Ok(())
}

fn read_exact<'a>(bytes: &'a [u8], cursor: &mut usize, size: usize) -> Result<&'a [u8]> {
    let end = cursor
        .checked_add(size)
        .ok_or_else(|| ContentError::new("compiled asset cache read offset overflows"))?;
    let data = bytes
        .get(*cursor..end)
        .ok_or_else(|| ContentError::new("compiled asset cache is truncated"))?;
    *cursor = end;
    Ok(data)
}

fn checked_usize_to_u32(value: usize) -> Result<u32> {
    u32::try_from(value).map_err(|_| ContentError::new("compiled asset count exceeds u32 range"))
}

fn checked_usize_to_u64(value: usize) -> Result<u64> {
    u64::try_from(value).map_err(|_| ContentError::new("compiled asset size exceeds u64 range"))
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::io::Write;
    use std::time::{SystemTime, UNIX_EPOCH};

    fn append_f32(bytes: &mut Vec<u8>, value: f32) {
        bytes.extend_from_slice(&value.to_le_bytes());
    }

    fn append_u16(bytes: &mut Vec<u8>, value: u16) {
        bytes.extend_from_slice(&value.to_le_bytes());
    }

    fn fixture_dir(name: &str) -> PathBuf {
        let stamp = SystemTime::now()
            .duration_since(UNIX_EPOCH)
            .expect("clock")
            .as_nanos();
        std::env::temp_dir().join(format!(
            "aster_content_{name}_{}_{}",
            std::process::id(),
            stamp
        ))
    }

    fn write_fixture(name: &str, degenerate: bool) -> PathBuf {
        let dir = fixture_dir(name);
        fs::create_dir_all(&dir).expect("create fixture dir");
        let mut bytes = Vec::new();
        for value in [
            -0.5, 0.0, -0.5, 0.5, 0.0, -0.5, 0.5, 0.0, 0.5, -0.5, 0.0, 0.5,
        ] {
            append_f32(&mut bytes, value);
        }
        for value in [0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0] {
            append_f32(&mut bytes, value);
        }
        for value in [0.0, 0.0, 1.0, 0.0, 1.0, 1.0, 0.0, 1.0] {
            append_f32(&mut bytes, value);
        }
        for value in [0u16, 1, 2, 0, 2, 3] {
            append_u16(&mut bytes, value);
        }
        if degenerate {
            for value in [0u16, 0, 1] {
                append_u16(&mut bytes, value);
            }
        }
        let bin = dir.join("asset.bin");
        fs::File::create(&bin)
            .expect("bin")
            .write_all(&bytes)
            .expect("write bin");
        let index_count = if degenerate { 9 } else { 6 };
        let index_bytes = index_count * 2;
        let scene = dir.join("asset.scene");
        fs::write(
            &scene,
            format!(
                r#"{{
  "asset": {{ "generator": "Aster test", "version": "2.0" }},
  "scene": 0,
  "scenes": [{{ "nodes": [0] }}],
  "nodes": [{{ "name": "Root", "mesh": 0, "translation": [0.0, 0.15, 0.0] }}],
  "meshes": [{{ "name": "Quad", "primitives": [{{
    "attributes": {{ "POSITION": 0, "NORMAL": 1, "TEXCOORD_0": 2 }},
    "indices": 3,
    "mode": 4,
    "material": 0
  }}] }}],
  "materials": [{{
    "name": "mat",
    "pbrMetallicRoughness": {{
      "baseColorFactor": [0.8, 0.55, 0.36, 1.0],
      "baseColorTexture": {{ "index": 0 }},
      "metallicFactor": 0.0,
      "roughnessFactor": 0.62,
      "metallicRoughnessTexture": {{ "index": 1 }}
    }},
    "normalTexture": {{ "index": 2 }},
    "occlusionTexture": {{ "index": 1 }},
    "doubleSided": true,
    "alphaMode": "MASK"
  }}],
  "textures": [{{ "source": 0 }}, {{ "source": 1 }}, {{ "source": 2 }}],
  "images": [{{ "uri": "base.png" }}, {{ "uri": "orm.png" }}, {{ "uri": "normal.png" }}],
  "buffers": [{{ "byteLength": {}, "uri": "asset.bin" }}],
  "bufferViews": [
    {{ "buffer": 0, "byteOffset": 0, "byteLength": 48, "target": 34962 }},
    {{ "buffer": 0, "byteOffset": 48, "byteLength": 48, "target": 34962 }},
    {{ "buffer": 0, "byteOffset": 96, "byteLength": 32, "target": 34962 }},
    {{ "buffer": 0, "byteOffset": 128, "byteLength": {}, "target": 34963 }}
  ],
  "accessors": [
    {{ "bufferView": 0, "byteOffset": 0, "componentType": 5126, "count": 4, "type": "VEC3" }},
    {{ "bufferView": 1, "byteOffset": 0, "componentType": 5126, "count": 4, "type": "VEC3" }},
    {{ "bufferView": 2, "byteOffset": 0, "componentType": 5126, "count": 4, "type": "VEC2" }},
    {{ "bufferView": 3, "byteOffset": 0, "componentType": 5123, "count": {}, "type": "SCALAR" }}
  ]
}}"#,
                bytes.len(),
                index_bytes,
                index_count
            ),
        )
        .expect("write scene");
        scene
    }

    #[test]
    fn compiles_deterministic_cache_bytes() {
        let scene = write_fixture("deterministic", false);
        let options = CompileOptions {
            origin_policy: OriginPolicy::CenterOnGround,
            unit_scale: 1.0,
        };
        let a = compile_scene_asset(&scene, options).expect("compile a");
        let b = compile_scene_asset(&scene, options).expect("compile b");
        assert_eq!(
            write_cache_bytes(&a).unwrap(),
            write_cache_bytes(&b).unwrap()
        );
        fs::remove_dir_all(scene.parent().unwrap()).ok();
    }

    #[test]
    fn source_and_options_hashes_change_from_evidence() {
        let scene = write_fixture("hash", false);
        let keep = compile_scene_asset(&scene, CompileOptions::default()).expect("compile keep");
        let scaled = compile_scene_asset(
            &scene,
            CompileOptions {
                origin_policy: OriginPolicy::Keep,
                unit_scale: 2.0,
            },
        )
        .expect("compile scaled");
        assert_eq!(keep.metadata.source_hash, scaled.metadata.source_hash);
        assert_ne!(keep.metadata.options_hash, scaled.metadata.options_hash);
        let bin = scene.parent().unwrap().join("asset.bin");
        let mut bytes = fs::read(&bin).unwrap();
        bytes[0] ^= 1;
        fs::write(&bin, bytes).unwrap();
        let changed =
            compile_scene_asset(&scene, CompileOptions::default()).expect("compile changed");
        assert_ne!(keep.metadata.source_hash, changed.metadata.source_hash);
        fs::remove_dir_all(scene.parent().unwrap()).ok();
    }

    #[test]
    fn prepares_mesh_material_and_collision_payload() {
        let scene = write_fixture("prepare", true);
        let asset = compile_scene_asset(
            &scene,
            CompileOptions {
                origin_policy: OriginPolicy::CenterOnGround,
                unit_scale: 1.0,
            },
        )
        .expect("compile");
        assert_eq!(asset.materials.len(), 2);
        assert_eq!(asset.meshes.len(), 1);
        assert_eq!(asset.collision_meshes.len(), 1);
        assert_eq!(asset.materials[1].alpha_mode, AlphaMode::Masked);
        assert!(asset.materials[1].double_sided);
        assert!(asset.materials[1].has_normal_texture);
        assert_ne!(asset.materials[1].permutation_key, 0);
        assert!(asset.materials[1].permutation_flags & MATERIAL_FLAG_TEXTURED != 0);
        assert!(asset.materials[1].permutation_flags & MATERIAL_FLAG_DOUBLE_SIDED != 0);
        assert!(asset.materials[1].permutation_flags & MATERIAL_FLAG_DEPTH_WRITE != 0);
        assert_eq!(
            asset.materials[1].pipeline_tag,
            "opaque.depth-write.double-sided.textured"
        );
        assert_eq!(asset.meshes[0].diagnostics.invalid_normals, 4);
        assert_eq!(asset.meshes[0].diagnostics.degenerate_triangles, 1);
        assert_eq!(asset.meshes[0].mesh.indices.len(), 6);
        assert_eq!(asset.collision_meshes[0].triangles.len(), 2);
        let min_y = asset.meshes[0]
            .mesh
            .vertices
            .iter()
            .fold(f32::MAX, |min, vertex| min.min(vertex.position.y));
        assert!((min_y - 0.0).abs() < 0.001);
        fs::remove_dir_all(scene.parent().unwrap()).ok();
    }

    #[test]
    fn reads_written_cache_round_trip() {
        let scene = write_fixture("round_trip", false);
        let asset = compile_scene_asset(
            &scene,
            CompileOptions {
                origin_policy: OriginPolicy::CenterOnGround,
                unit_scale: 1.0,
            },
        )
        .expect("compile");
        let bytes = write_cache_bytes(&asset).expect("write cache");
        let read = read_cache_bytes(&bytes).expect("read cache");
        assert_eq!(asset, read);
        fs::remove_dir_all(scene.parent().unwrap()).ok();
    }

    #[test]
    fn inspects_and_bakes_texture_metadata() {
        let dir = fixture_dir("texture");
        fs::create_dir_all(&dir).expect("texture dir");
        let texture = dir.join("albedo.png");
        let mut png = Vec::new();
        png.extend_from_slice(&[0x89, b'P', b'N', b'G', b'\r', b'\n', 0x1a, b'\n']);
        png.extend_from_slice(&13u32.to_be_bytes());
        png.extend_from_slice(b"IHDR");
        png.extend_from_slice(&32u32.to_be_bytes());
        png.extend_from_slice(&16u32.to_be_bytes());
        png.extend_from_slice(&[8, 6, 0, 0, 0]);
        fs::write(&texture, png).expect("png");

        let summary = inspect_texture(&texture, "albedo").expect("inspect");
        assert_eq!(summary.kind, TextureImportKind::Albedo);
        assert_eq!(summary.color_space, "srgb");
        assert_eq!(summary.width, 32);
        assert_eq!(summary.height, 16);
        assert_eq!(summary.mip_count, texture_mip_count(32, 16));

        let baked = dir.join("albedo.ktx2");
        let baked_summary = bake_texture_to_ktx2(&texture, &baked, "albedo").expect("bake");
        assert_eq!(baked_summary.source_hash, summary.source_hash);
        let baked_bytes = fs::read(&baked).expect("baked bytes");
        assert!(baked_bytes
            .windows(b"ASTER_MIP_PAYLOADS_V1".len())
            .any(|window| { window == b"ASTER_MIP_PAYLOADS_V1" }));
        assert!(baked_bytes.len() > 96);
        let baked_read = inspect_texture(&baked, "albedo").expect("inspect baked");
        assert_eq!(baked_read.format, "ktx2");
        assert_eq!(baked_read.width, 32);
        assert_eq!(baked_read.height, 16);
        fs::remove_dir_all(dir).ok();
    }

    fn write_png_header(path: &Path, width: u32, height: u32) {
        let mut png = Vec::new();
        png.extend_from_slice(&[0x89, b'P', b'N', b'G', b'\r', b'\n', 0x1a, b'\n']);
        png.extend_from_slice(&13u32.to_be_bytes());
        png.extend_from_slice(b"IHDR");
        png.extend_from_slice(&width.to_be_bytes());
        png.extend_from_slice(&height.to_be_bytes());
        png.extend_from_slice(&[8, 6, 0, 0, 0]);
        fs::write(path, png).expect("png");
    }

    fn write_material_project(name: &str, missing_texture: bool) -> PathBuf {
        let dir = fixture_dir(name);
        fs::create_dir_all(dir.join("materials")).expect("materials dir");
        fs::create_dir_all(dir.join("textures")).expect("textures dir");
        if !missing_texture {
            write_png_header(&dir.join("textures/wet_albedo.png"), 16, 8);
            write_png_header(&dir.join("textures/wet_normal.png"), 16, 8);
        }
        fs::write(
            dir.join("materials/wet_rock.astermat"),
            r#"material WetRock {
  schema_version: 1
  name: "Wet Rock"
  shading_model: LitPBR
  surface_profile: StratifiedRock
  blend_mode: Opaque
  cull_mode: Back
  receives_shadows: true

  textures {
    albedo: "../textures/wet_albedo.png"
    normal: "../textures/wet_normal.png"
  }

  params {
    base_color_r: 0.32
    base_color_g: 0.28
    base_color_b: 0.24
    roughness: 0.76
    metallic: 0.0
  }

  features {
    triplanar: true
    normal_map: true
  }
}
"#,
        )
        .expect("material");
        fs::write(
            dir.join("project.asterproj"),
            r#"{
  "schema_version": 1,
  "name": "Material Cook Test",
  "assets": [
    { "id": "material.wet_rock", "kind": "material", "path": "materials/wet_rock.astermat" }
  ]
}
"#,
        )
        .expect("project");
        dir.join("project.asterproj")
    }

    #[test]
    fn cooks_project_database_and_materialbin() {
        let project = write_material_project("cook_project", false);
        let output = project.parent().unwrap().join("cooked/desktop");
        let result = cook_project(&project, "desktop", &output).expect("cook");
        assert_eq!(result.database.records.len(), 1);
        assert!(result.database_path.exists());
        let db = read_asset_database(&result.database_path).expect("read db");
        assert_eq!(db.records[0].kind, "material");
        assert!(db.records[0]
            .outputs
            .iter()
            .any(|output| output.kind == "materialbin"));
        assert!(db.records[0]
            .outputs
            .iter()
            .any(|output| output.role == "preview"));
        let material_output = db.records[0]
            .outputs
            .iter()
            .find(|output| output.kind == "materialbin")
            .unwrap();
        let material_bin: MaterialBin = serde_json::from_slice(
            &fs::read(output.join(&material_output.path)).expect("materialbin bytes"),
        )
        .expect("materialbin json");
        assert_eq!(material_bin.id, "WetRock");
        assert_eq!(material_bin.textures.len(), 2);
        assert!(material_bin.feature_mask & (1 << 0) != 0);
        assert!(material_bin.shader_variant_key != 0);
        assert!(report_asset_database(&db).contains("assets=1"));
        fs::remove_dir_all(project.parent().unwrap()).ok();
    }

    #[test]
    fn cook_reports_missing_material_texture() {
        let project = write_material_project("cook_missing", true);
        let output = project.parent().unwrap().join("cooked/desktop");
        let result = cook_project(&project, "desktop", &output).expect("cook");
        assert!(result.database.records[0]
            .diagnostics
            .iter()
            .any(|diagnostic| diagnostic.severity == "error"));
        assert!(result.database.records[0]
            .dependencies
            .iter()
            .any(|dependency| !dependency.present));
        fs::remove_dir_all(project.parent().unwrap()).ok();
    }

    fn write_glb_fixture() -> PathBuf {
        let dir = fixture_dir("glb");
        fs::create_dir_all(&dir).expect("glb dir");
        let mut bin = Vec::new();
        for value in [
            -0.5, 0.0, -0.5, 0.5, 0.0, -0.5, 0.5, 0.0, 0.5, -0.5, 0.0, 0.5,
        ] {
            append_f32(&mut bin, value);
        }
        for value in [0.0, 1.0, 0.0, 0.0, 1.0, 0.0, 0.0, 1.0, 0.0, 0.0, 1.0, 0.0] {
            append_f32(&mut bin, value);
        }
        for value in [0.0, 0.0, 1.0, 0.0, 1.0, 1.0, 0.0, 1.0] {
            append_f32(&mut bin, value);
        }
        for value in [0u16, 1, 2, 0, 2, 3] {
            append_u16(&mut bin, value);
        }
        let image_offset = bin.len();
        let mut image = Vec::new();
        image.extend_from_slice(&[0x89, b'P', b'N', b'G', b'\r', b'\n', 0x1a, b'\n']);
        image.extend_from_slice(&13u32.to_be_bytes());
        image.extend_from_slice(b"IHDR");
        image.extend_from_slice(&4u32.to_be_bytes());
        image.extend_from_slice(&4u32.to_be_bytes());
        image.extend_from_slice(&[8, 6, 0, 0, 0]);
        bin.extend_from_slice(&image);
        while bin.len() % 4 != 0 {
            bin.push(0);
        }
        let json = format!(
            r#"{{
  "asset": {{ "version": "2.0" }},
  "scene": 0,
  "scenes": [{{ "nodes": [0] }}],
  "nodes": [{{ "name": "Root", "mesh": 0 }}],
  "meshes": [{{ "name": "Quad", "primitives": [{{
    "attributes": {{ "POSITION": 0, "NORMAL": 1, "TEXCOORD_0": 2 }},
    "indices": 3,
    "material": 0
  }}] }}],
  "materials": [{{ "name": "mat", "pbrMetallicRoughness": {{ "baseColorTexture": {{ "index": 0 }} }} }}],
  "textures": [{{ "source": 0 }}],
  "images": [{{ "bufferView": 4, "mimeType": "image/png" }}],
  "buffers": [{{ "byteLength": {} }}],
  "bufferViews": [
    {{ "buffer": 0, "byteOffset": 0, "byteLength": 48 }},
    {{ "buffer": 0, "byteOffset": 48, "byteLength": 48 }},
    {{ "buffer": 0, "byteOffset": 96, "byteLength": 32 }},
    {{ "buffer": 0, "byteOffset": 128, "byteLength": 12 }},
    {{ "buffer": 0, "byteOffset": {}, "byteLength": {} }}
  ],
  "accessors": [
    {{ "bufferView": 0, "componentType": 5126, "count": 4, "type": "VEC3" }},
    {{ "bufferView": 1, "componentType": 5126, "count": 4, "type": "VEC3" }},
    {{ "bufferView": 2, "componentType": 5126, "count": 4, "type": "VEC2" }},
    {{ "bufferView": 3, "componentType": 5123, "count": 6, "type": "SCALAR" }}
  ]
}}"#,
            bin.len(),
            image_offset,
            image.len()
        );
        let mut json_bytes = json.into_bytes();
        while json_bytes.len() % 4 != 0 {
            json_bytes.push(b' ');
        }
        let total_len = 12 + 8 + json_bytes.len() + 8 + bin.len();
        let mut glb = Vec::with_capacity(total_len);
        glb.extend_from_slice(b"glTF");
        glb.extend_from_slice(&2u32.to_le_bytes());
        glb.extend_from_slice(&(total_len as u32).to_le_bytes());
        glb.extend_from_slice(&(json_bytes.len() as u32).to_le_bytes());
        glb.extend_from_slice(&0x4E4F534Au32.to_le_bytes());
        glb.extend_from_slice(&json_bytes);
        glb.extend_from_slice(&(bin.len() as u32).to_le_bytes());
        glb.extend_from_slice(&0x004E4942u32.to_le_bytes());
        glb.extend_from_slice(&bin);
        let path = dir.join("asset.glb");
        fs::write(&path, glb).expect("glb");
        path
    }

    #[test]
    fn imports_glb_with_embedded_buffer_and_texture_dependency() {
        let glb = write_glb_fixture();
        let asset = import_glb_scene(&glb, CompileOptions::default()).expect("import glb");
        assert_eq!(asset.meshes.len(), 1);
        assert_eq!(asset.materials.len(), 2);
        let dependency = &asset.materials[1].texture_dependencies[0];
        assert_eq!(dependency.uri, "embedded:4");
        assert!(dependency.present);
        assert_ne!(dependency.hash, [0u8; 32]);
        fs::remove_dir_all(glb.parent().unwrap()).ok();
    }
}
