// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#![recursion_limit = "256"]

use serde::{Deserialize, Serialize};
use serde_json::Value;
use std::collections::{BTreeMap, HashMap};
use std::fmt;
use std::fs;
use std::path::{Path, PathBuf};
use std::process::Command;

pub const CACHE_MAGIC: [u8; 8] = *b"ASTRCV1\0";
pub const CACHE_VERSION: u32 = 3;
pub const COMPILER_VERSION: u32 = 1;
pub const CACHE_ENDIAN_MARKER: u32 = 0x1234_5678;

const CHUNK_METADATA: u32 = 1;
const CHUNK_SCENE_GRAPH: u32 = 2;
const CHUNK_MATERIALS: u32 = 3;
const CHUNK_MESHES: u32 = 4;
const CHUNK_COLLISION: u32 = 5;
const CHUNK_GEOMETRY_AUDIT: u32 = 6;
const CHUNK_MESHLETS: u32 = 7;
const CHUNK_LODS: u32 = 8;
const CHUNK_SKELETONS: u32 = 9;
const CHUNK_ANIMATIONS: u32 = 10;
const CHUNK_MORPHS: u32 = 11;
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

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct TextureRoleContract {
    pub role: &'static str,
    pub color_space: &'static str,
    pub required_for_lit_pbr: bool,
    pub runtime_format: &'static str,
    pub compression: &'static str,
}

pub const TEXTURE_ROLE_CONTRACTS: &[TextureRoleContract] = &[
    TextureRoleContract {
        role: "albedo",
        color_space: "srgb",
        required_for_lit_pbr: true,
        runtime_format: "ktx2",
        compression: "ktx2-basis",
    },
    TextureRoleContract {
        role: "normal",
        color_space: "linear",
        required_for_lit_pbr: true,
        runtime_format: "ktx2",
        compression: "ktx2-basis",
    },
    TextureRoleContract {
        role: "orm",
        color_space: "linear",
        required_for_lit_pbr: true,
        runtime_format: "ktx2",
        compression: "ktx2-basis",
    },
    TextureRoleContract {
        role: "roughness",
        color_space: "linear",
        required_for_lit_pbr: false,
        runtime_format: "ktx2",
        compression: "ktx2-basis",
    },
    TextureRoleContract {
        role: "metallic",
        color_space: "linear",
        required_for_lit_pbr: false,
        runtime_format: "ktx2",
        compression: "ktx2-basis",
    },
    TextureRoleContract {
        role: "ao",
        color_space: "linear",
        required_for_lit_pbr: false,
        runtime_format: "ktx2",
        compression: "ktx2-basis",
    },
    TextureRoleContract {
        role: "height",
        color_space: "linear",
        required_for_lit_pbr: false,
        runtime_format: "ktx2",
        compression: "ktx2-basis",
    },
    TextureRoleContract {
        role: "emissive",
        color_space: "srgb",
        required_for_lit_pbr: false,
        runtime_format: "ktx2",
        compression: "ktx2-basis",
    },
    TextureRoleContract {
        role: "wetness",
        color_space: "linear",
        required_for_lit_pbr: false,
        runtime_format: "ktx2",
        compression: "ktx2-basis",
    },
    TextureRoleContract {
        role: "opacity",
        color_space: "linear",
        required_for_lit_pbr: false,
        runtime_format: "ktx2",
        compression: "ktx2-basis",
    },
];

pub fn texture_role_contract(role: &str) -> Option<TextureRoleContract> {
    let canonical = canonical_material_texture_role(role)?;
    TEXTURE_ROLE_CONTRACTS
        .iter()
        .copied()
        .find(|contract| contract.role == canonical)
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

pub const ASSET_DATABASE_SCHEMA_VERSION: u32 = 2;
pub const ASSET_IMPORT_SETTINGS_VERSION: u32 = 2;
pub const ASSET_MANIFEST_SCHEMA_VERSION: u32 = 1;
pub const MATERIAL_BIN_SCHEMA_VERSION: u32 = 2;
pub const ASSET_GRAPH_BIN_SCHEMA_VERSION: u32 = 1;

#[derive(Clone, Debug, Default, Serialize, Deserialize, PartialEq, Eq)]
pub struct AssetSourceLocation {
    #[serde(default, skip_serializing_if = "String::is_empty")]
    pub source_path: String,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub line: Option<usize>,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub column: Option<usize>,
}

#[derive(Clone, Debug, Default, Serialize, Deserialize, PartialEq, Eq)]
pub struct AssetSourceRecord {
    pub path: String,
    #[serde(default, skip_serializing_if = "String::is_empty")]
    pub hash: String,
}

#[derive(Clone, Debug, Serialize, Deserialize, PartialEq, Eq)]
pub struct AssetImportPresetRecord {
    pub name: String,
    pub origin_policy: String,
    pub unit_scale: String,
    pub texture_role_policy: String,
    pub material_slot_policy: String,
    pub collision_policy: String,
    pub lod_policy: String,
    pub meshlet_policy: String,
    pub skeleton_policy: String,
    pub animation_policy: String,
    pub morph_policy: String,
}

impl Default for AssetImportPresetRecord {
    fn default() -> Self {
        Self {
            name: "default".to_string(),
            origin_policy: OriginPolicy::Keep.as_str().to_string(),
            unit_scale: "1.000000000".to_string(),
            texture_role_policy: "strict-litpbr-v2".to_string(),
            material_slot_policy: "preserve".to_string(),
            collision_policy: "static-triangle-mesh".to_string(),
            lod_policy: "audit-only".to_string(),
            meshlet_policy: "deterministic-64-126-audit".to_string(),
            skeleton_policy: "import-and-validate".to_string(),
            animation_policy: "import-clips".to_string(),
            morph_policy: "preserve-and-validate".to_string(),
        }
    }
}

#[derive(Clone, Debug, Serialize, Deserialize, PartialEq, Eq)]
pub struct AssetPlatformProfileRecord {
    pub name: String,
    pub runtime_texture_format: String,
    pub compression: String,
    pub target: String,
}

impl Default for AssetPlatformProfileRecord {
    fn default() -> Self {
        Self {
            name: "desktop".to_string(),
            runtime_texture_format: "ktx2".to_string(),
            compression: "ktx2-basis".to_string(),
            target: "desktop".to_string(),
        }
    }
}

#[derive(Clone, Debug, Default, Serialize, Deserialize, PartialEq, Eq)]
pub struct AssetDependencyEdge {
    pub from: String,
    pub to: String,
    pub role: String,
    pub present: bool,
    #[serde(default, skip_serializing_if = "String::is_empty")]
    pub hash: String,
}

#[derive(Clone, Debug, Default, Serialize, Deserialize, PartialEq, Eq)]
pub struct AssetDerivedHashes {
    #[serde(default, skip_serializing_if = "String::is_empty")]
    pub source_hash: String,
    #[serde(default, skip_serializing_if = "String::is_empty")]
    pub options_hash: String,
    #[serde(default, skip_serializing_if = "String::is_empty")]
    pub dependency_hash: String,
    #[serde(default, skip_serializing_if = "String::is_empty")]
    pub artifact_hash: String,
    #[serde(default, skip_serializing_if = "String::is_empty")]
    pub material_hash: String,
    #[serde(default, skip_serializing_if = "String::is_empty")]
    pub shader_variant_key: String,
    #[serde(default, skip_serializing_if = "String::is_empty")]
    pub pipeline_cache_key: String,
    #[serde(default, skip_serializing_if = "String::is_empty")]
    pub vertex_input_contract: String,
    #[serde(default, skip_serializing_if = "String::is_empty")]
    pub frame_plan_fingerprint: String,
}

#[derive(Clone, Debug, Default, Serialize, Deserialize, PartialEq, Eq)]
pub struct WorldReadyAssetReport {
    pub accepted: bool,
    pub report_hash: String,
    #[serde(default)]
    pub readiness_signals: Vec<String>,
    #[serde(default)]
    pub diagnostics: Vec<String>,
}

#[derive(Clone, Debug, Default, Serialize, Deserialize, PartialEq, Eq)]
pub struct AssetFateReport {
    pub asset_id: String,
    pub asset_guid: String,
    pub kind: String,
    pub source_path: String,
    pub production_ready: bool,
    pub dependency_count: usize,
    pub output_count: usize,
    pub diagnostic_count: usize,
    #[serde(default)]
    pub derived_hashes: AssetDerivedHashes,
    #[serde(default)]
    pub chain: Vec<String>,
    #[serde(default)]
    pub render_contract: Vec<String>,
    #[serde(default)]
    pub artifact_provenance: Vec<AssetArtifactRecord>,
    #[serde(default)]
    pub world_ready: WorldReadyAssetReport,
}

#[derive(Clone, Debug, Default, Serialize, Deserialize, PartialEq, Eq)]
pub struct AssetGraphNode {
    pub guid: String,
    pub id: String,
    pub kind: String,
    pub source_path: String,
    pub production_ready: bool,
}

#[derive(Clone, Debug, Default, Serialize, Deserialize, PartialEq, Eq)]
pub struct AssetGraphEdge {
    pub from: String,
    pub to: String,
    pub role: String,
    pub present: bool,
    #[serde(default, skip_serializing_if = "String::is_empty")]
    pub hash: String,
}

#[derive(Clone, Debug, Default, Serialize, Deserialize, PartialEq, Eq)]
pub struct AssetGraph {
    pub schema_version: u32,
    pub project_fingerprint: String,
    pub nodes: Vec<AssetGraphNode>,
    pub edges: Vec<AssetGraphEdge>,
}

#[derive(Clone, Debug, Default, Serialize, Deserialize, PartialEq, Eq)]
pub struct AssetArtifactRecord {
    pub role: String,
    pub kind: String,
    pub path: String,
    pub hash: String,
    #[serde(default)]
    pub reused: bool,
}

#[derive(Clone, Debug, Serialize, Deserialize, PartialEq, Eq)]
pub struct AssetToolVersionRecord {
    pub name: String,
    pub version: String,
}

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
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub source_path: Option<String>,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub line: Option<usize>,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub column: Option<usize>,
    #[serde(default, skip_serializing_if = "Vec::is_empty")]
    pub source_locations: Vec<AssetSourceLocation>,
}

#[derive(Clone, Debug, Serialize, Deserialize, PartialEq, Eq)]
pub struct AssetDatabaseRecord {
    pub guid: String,
    pub id: String,
    pub kind: String,
    #[serde(default)]
    pub source: AssetSourceRecord,
    pub source_path: String,
    #[serde(default)]
    pub import_preset: AssetImportPresetRecord,
    #[serde(default)]
    pub platform_profile: AssetPlatformProfileRecord,
    pub import_settings_version: u32,
    pub source_hash: String,
    pub options_hash: String,
    #[serde(default)]
    pub dependency_edges: Vec<AssetDependencyEdge>,
    pub dependencies: Vec<AssetDependencyRecord>,
    #[serde(default)]
    pub artifacts: Vec<AssetArtifactRecord>,
    pub outputs: Vec<AssetCookedOutput>,
    pub diagnostics: Vec<AssetCookDiagnostic>,
    #[serde(default)]
    pub tool_versions: Vec<AssetToolVersionRecord>,
    #[serde(default)]
    pub derived_hashes: AssetDerivedHashes,
    #[serde(default)]
    pub fate_report: AssetFateReport,
    pub platform: String,
}

#[derive(Clone, Debug, Serialize, Deserialize, PartialEq, Eq)]
pub struct AssetDatabase {
    pub schema_version: u32,
    pub platform: String,
    #[serde(default)]
    pub artifact_manifest: String,
    #[serde(default)]
    pub tool_versions: Vec<AssetToolVersionRecord>,
    #[serde(default)]
    pub asset_graph: AssetGraph,
    #[serde(default)]
    pub fate_reports: Vec<AssetFateReport>,
    pub records: Vec<AssetDatabaseRecord>,
}

#[derive(Clone, Debug, Default, Serialize, Deserialize, PartialEq, Eq)]
pub struct AssetCatalogRecord {
    pub id: String,
    pub path: String,
    pub simple_name: String,
    #[serde(default)]
    pub tags: Vec<String>,
    #[serde(default)]
    pub metadata: BTreeMap<String, String>,
    #[serde(default)]
    pub deleted: bool,
}

#[derive(Clone, Debug, Serialize, Deserialize, PartialEq, Eq)]
pub struct AssetCatalogStore {
    pub schema_version: u32,
    pub catalogs: Vec<AssetCatalogRecord>,
}

#[derive(Clone, Debug, Serialize, Deserialize, PartialEq, Eq)]
pub struct MaterialBinTextureRecord {
    pub role: String,
    pub source_path: String,
    pub cooked_path: String,
    pub kind: String,
    pub color_space: String,
    pub source_format: String,
    pub runtime_format: String,
    pub width: u32,
    pub height: u32,
    pub mip_count: u32,
    pub byte_cost: u64,
    pub encoder: String,
    pub fallback_reason: String,
    pub platform_compatibility: String,
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
    #[serde(default)]
    pub provenance: BTreeMap<String, String>,
    #[serde(default)]
    pub authoring: BTreeMap<String, String>,
    #[serde(default)]
    pub preview: BTreeMap<String, String>,
    #[serde(default)]
    pub quality_profile: BTreeMap<String, String>,
    pub textures: Vec<MaterialBinTextureRecord>,
    pub binding_layout: Vec<MaterialBinBinding>,
    pub dependency_hashes: Vec<AssetDependencyRecord>,
    #[serde(default)]
    pub derived_hashes: AssetDerivedHashes,
    pub diagnostics: Vec<AssetCookDiagnostic>,
}

#[derive(Clone, Debug, Serialize, Deserialize, PartialEq, Eq)]
pub struct TextureCookReport {
    pub schema_version: u32,
    pub role: String,
    pub kind: String,
    pub color_space: String,
    pub format: String,
    pub source_format: String,
    pub runtime_format: String,
    pub width: u32,
    pub height: u32,
    pub mip_count: u32,
    pub byte_cost: u64,
    pub encoder: String,
    pub fallback_reason: String,
    pub platform_compatibility: String,
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
    pub material_bin_path: Option<PathBuf>,
    pub report_path: PathBuf,
    pub preview_path: Option<PathBuf>,
    pub material_bin: MaterialBin,
    pub emitted_runtime_outputs: bool,
}

#[derive(Clone, Debug, Serialize, Deserialize, PartialEq, Eq)]
pub struct ProceduralGraphNode {
    pub id: String,
    pub kind: String,
    #[serde(default, skip_serializing_if = "String::is_empty")]
    pub role: String,
    #[serde(default, skip_serializing_if = "String::is_empty")]
    pub label: String,
    #[serde(default, skip_serializing_if = "BTreeMap::is_empty")]
    pub params: BTreeMap<String, String>,
    pub capability_status: String,
}

#[derive(Clone, Debug, Serialize, Deserialize, PartialEq, Eq)]
pub struct ProceduralGraphEdge {
    pub from: String,
    pub to: String,
    pub role: String,
}

#[derive(Clone, Debug, Default, Serialize, Deserialize, PartialEq, Eq)]
pub struct ProceduralGraphSocket {
    pub node_id: String,
    pub name: String,
    #[serde(rename = "type")]
    pub socket_type: String,
    pub direction: String,
    pub role: String,
    pub default_value: String,
}

#[derive(Clone, Debug, Default, Serialize, Deserialize, PartialEq, Eq)]
pub struct ProceduralGraphZone {
    pub id: String,
    pub kind: String,
    pub input_node: String,
    pub output_node: String,
    pub items: Vec<String>,
}

#[derive(Clone, Debug, Default, Serialize, Deserialize, PartialEq, Eq)]
pub struct ProceduralGraphBundleItem {
    pub bundle_id: String,
    pub name: String,
    #[serde(rename = "type")]
    pub socket_type: String,
    pub source_node: String,
}

#[derive(Clone, Debug, Default, Serialize, Deserialize, PartialEq, Eq)]
pub struct ProceduralGraphBakeTarget {
    pub id: String,
    pub node_id: String,
    pub target: String,
    pub artifact_role: String,
    pub frame_start: u32,
    pub frame_end: u32,
}

#[derive(Clone, Debug, Default, Serialize, Deserialize, PartialEq, Eq)]
pub struct AssetGraphProofArtifact {
    pub id: String,
    pub role: String,
    pub path: String,
    pub kind: String,
    pub hash: String,
    pub width: u32,
    pub height: u32,
    pub signal_tags: Vec<String>,
}

#[derive(Clone, Debug, Serialize, Deserialize, PartialEq, Eq)]
pub struct AssetGraphMeshDescriptor {
    pub primitive: String,
    pub uv_policy: String,
    pub tangent_policy: String,
    pub collision_proxy: String,
    pub lod_policy: String,
}

#[derive(Clone, Debug, Default, Serialize, Deserialize, PartialEq)]
pub struct AssetGraphPerceptualTemplate {
    pub id: String,
    pub valid_primitive_profile: String,
    pub surface_response: String,
    pub history_response: String,
    pub material_half_life_seconds: f32,
    pub wetness_half_life_seconds: f32,
    pub semantic_lod: f32,
    pub streaming_cost: f32,
    pub required_patch_channels: Vec<String>,
    pub required_contact_channels: Vec<String>,
    pub required_residue_channels: Vec<String>,
}

#[derive(Clone, Debug, Serialize, Deserialize, PartialEq, Eq)]
pub struct AssetGraphQualityIssue {
    pub severity: String,
    pub category: String,
    pub node: String,
    pub message: String,
}

#[derive(Clone, Debug, Serialize, Deserialize, PartialEq, Eq)]
pub struct AssetGraphQualityReport {
    pub score: u32,
    pub production_ready: bool,
    pub presentation_quality: BTreeMap<String, String>,
    pub surface_stack: BTreeMap<String, String>,
    pub visual_proof_expectations: Vec<String>,
    pub issues: Vec<AssetGraphQualityIssue>,
}

#[derive(Clone, Debug, Default, Serialize, Deserialize, PartialEq, Eq)]
pub struct AssetGraphProductionSession {
    pub session_id: String,
    pub graph_hash: String,
    pub preview_artifact_hash: String,
    pub quality_gate: String,
    pub cook_steps: Vec<String>,
}

#[derive(Clone, Debug, Default, Serialize, Deserialize, PartialEq, Eq)]
pub struct AssetGraphFactoryStageReport {
    pub id: String,
    pub kind: String,
    pub status: String,
    pub diagnostics: Vec<String>,
}

#[derive(Clone, Debug, Default, Serialize, Deserialize, PartialEq)]
pub struct AssetGraphFactorySignalCoverage {
    pub signal: String,
    pub average: f32,
    pub coverage: f32,
    pub status: String,
}

#[derive(Clone, Debug, Default, Serialize, Deserialize, PartialEq)]
pub struct AssetGraphFactoryReport {
    pub stable_recipe_hash: String,
    pub stage_diagnostics: Vec<AssetGraphFactoryStageReport>,
    pub surface_signal_coverage: Vec<AssetGraphFactorySignalCoverage>,
    pub collision_proxy_summary: BTreeMap<String, String>,
    pub visual_brief_claims: Vec<String>,
    pub visual_brief_rejections: Vec<String>,
}

#[derive(Clone, Debug, Serialize, Deserialize, PartialEq)]
pub struct AssetGraphMaterialPackage {
    pub id: String,
    pub surface_profile: String,
    pub feature_mask: u64,
    pub shader_variant_key: u64,
    pub shader_variant_tag: String,
    pub pipeline_tag: String,
    pub fallback: MaterialBinFallback,
    pub params: BTreeMap<String, f32>,
    pub features: BTreeMap<String, bool>,
}

#[derive(Clone, Debug, Serialize, Deserialize, PartialEq)]
pub struct AssetGraphBin {
    pub schema_version: u32,
    pub asset_guid: String,
    pub id: String,
    pub name: String,
    pub kind: String,
    pub source_path: String,
    pub runtime_model: String,
    pub material: AssetGraphMaterialPackage,
    pub mesh: AssetGraphMeshDescriptor,
    pub perceptual_template: AssetGraphPerceptualTemplate,
    pub nodes: Vec<ProceduralGraphNode>,
    pub edges: Vec<ProceduralGraphEdge>,
    #[serde(default)]
    pub metadata: BTreeMap<String, String>,
    #[serde(default)]
    pub sockets: Vec<ProceduralGraphSocket>,
    #[serde(default)]
    pub zones: Vec<ProceduralGraphZone>,
    #[serde(default)]
    pub bundle_items: Vec<ProceduralGraphBundleItem>,
    #[serde(default)]
    pub bake_targets: Vec<ProceduralGraphBakeTarget>,
    #[serde(default)]
    pub proof_artifacts: Vec<AssetGraphProofArtifact>,
    pub preview: BTreeMap<String, String>,
    #[serde(default)]
    pub production_session: AssetGraphProductionSession,
    #[serde(default)]
    pub factory_report: AssetGraphFactoryReport,
    pub quality: AssetGraphQualityReport,
    pub derived_hashes: AssetDerivedHashes,
    pub diagnostics: Vec<AssetCookDiagnostic>,
}

#[derive(Clone, Debug, PartialEq)]
pub struct AssetGraphCookResult {
    pub graph_bin_path: PathBuf,
    pub report_path: PathBuf,
    pub graph_bin: AssetGraphBin,
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct CookProjectResult {
    pub database: AssetDatabase,
    pub database_path: PathBuf,
    pub manifest_path: PathBuf,
    pub error_count: usize,
    pub warning_count: usize,
}

#[derive(Clone, Debug, Serialize, Deserialize, PartialEq, Eq)]
pub struct MaterialInspectTexture {
    pub role: String,
    pub path: String,
    pub present: bool,
    pub kind: String,
    pub color_space: String,
    pub source_format: String,
    pub width: u32,
    pub height: u32,
    pub mip_count: u32,
    pub source_hash: String,
    pub diagnostics: Vec<String>,
}

#[derive(Clone, Debug, Serialize, Deserialize, PartialEq, Eq)]
pub struct MaterialInspectReport {
    pub schema_version: u32,
    pub id: String,
    pub name: String,
    pub source_path: String,
    pub shading_model: String,
    pub required_runtime_roles: Vec<String>,
    pub textures: Vec<MaterialInspectTexture>,
    pub dependencies: Vec<AssetDependencyRecord>,
    pub diagnostics: Vec<AssetCookDiagnostic>,
    #[serde(default)]
    pub provenance: BTreeMap<String, String>,
    #[serde(default)]
    pub authoring: BTreeMap<String, String>,
    #[serde(default)]
    pub preview: BTreeMap<String, String>,
    #[serde(default)]
    pub quality_profile: BTreeMap<String, String>,
    pub production_ready: bool,
    pub platform_compatibility: String,
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
    let path = path.as_ref();
    if !options.unit_scale.is_finite() || options.unit_scale <= 0.0 {
        return Err(ContentError::new(
            "unit scale must be finite and greater than zero",
        ));
    }
    if let Some(kind) = dcc_source_kind(path) {
        return Err(ContentError::new(format!(
            "embedded DCC importer for {kind} is declared by Asset Pipeline v2 but not enabled in this build"
        )));
    }
    let data = load_asset_data(path)?;
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
        (CHUNK_GEOMETRY_AUDIT, write_geometry_audit_chunk(asset)?),
        (CHUNK_MESHLETS, write_empty_named_chunk("meshlets:v2")?),
        (CHUNK_LODS, write_empty_named_chunk("lod-chain:v2")?),
        (CHUNK_SKELETONS, write_empty_named_chunk("skeletons:v2")?),
        (CHUNK_ANIMATIONS, write_empty_named_chunk("animations:v2")?),
        (CHUNK_MORPHS, write_empty_named_chunk("morph-targets:v2")?),
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
        diagnostics.push("error: texture dimensions could not be decoded from header".to_string());
    }
    let kind = TextureImportKind::role(role);
    if kind == TextureImportKind::Unknown {
        diagnostics.push(format!("error: unsupported texture role '{role}'"));
    }
    if format == "ktx2" && bytes.len() >= 16 {
        let vk_format = u32::from_le_bytes([bytes[12], bytes[13], bytes[14], bytes[15]]);
        let expected_vk_format = if kind.color_space() == "srgb" {
            43u32
        } else {
            37u32
        };
        if vk_format != expected_vk_format {
            diagnostics.push(format!(
                "error: texture role '{}' expects {} KTX2 VkFormat {}, found {}",
                role,
                kind.color_space(),
                expected_vk_format,
                vk_format
            ));
        }
    }
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
    if texture_summary_has_error(&summary) {
        return Err(ContentError::new(format!(
            "texture '{}' failed validation: {}",
            input.as_ref().display(),
            summary.diagnostics.join("; ")
        )));
    }
    if let Some(parent) = output.as_ref().parent() {
        if !parent.as_os_str().is_empty() {
            fs::create_dir_all(parent)?;
        }
    }
    if summary.format == "ktx2" {
        if input.as_ref() != output.as_ref() {
            fs::copy(input.as_ref(), output.as_ref())?;
        }
        return Ok(summary);
    }
    let encoder = std::env::var("ASTER_TEXTURE_ENCODER").map_err(|_| {
        ContentError::new(format!(
            "texture '{}' is {} source; strict cook requires KTX2 source or ASTER_TEXTURE_ENCODER",
            input.as_ref().display(),
            summary.format
        ))
    })?;
    let status = Command::new(&encoder)
        .arg("--input")
        .arg(input.as_ref())
        .arg("--output")
        .arg(output.as_ref())
        .arg("--role")
        .arg(role)
        .arg("--color-space")
        .arg(&summary.color_space)
        .arg("--mips")
        .arg(summary.mip_count.to_string())
        .status()
        .map_err(|error| {
            ContentError::new(format!(
                "failed to run texture encoder '{encoder}': {error}"
            ))
        })?;
    if !status.success() {
        return Err(ContentError::new(format!(
            "texture encoder '{encoder}' failed for {}",
            input.as_ref().display()
        )));
    }
    let output_summary = inspect_texture(output.as_ref(), role)?;
    if texture_summary_has_error(&output_summary) {
        return Err(ContentError::new(format!(
            "texture encoder '{encoder}' wrote invalid KTX2 for {}: {}",
            input.as_ref().display(),
            output_summary.diagnostics.join("; ")
        )));
    }
    Ok(summary)
}

pub fn read_asset_database(path: impl AsRef<Path>) -> Result<AssetDatabase> {
    let mut database: AssetDatabase = serde_json::from_slice(&fs::read(path)?)?;
    refresh_asset_database_truth(&mut database);
    Ok(database)
}

fn default_tool_versions() -> Vec<AssetToolVersionRecord> {
    vec![
        AssetToolVersionRecord {
            name: "aster_content".to_string(),
            version: format!("compiler-v{}", COMPILER_VERSION),
        },
        AssetToolVersionRecord {
            name: "asset-pipeline".to_string(),
            version: format!("schema-v{}", ASSET_DATABASE_SCHEMA_VERSION),
        },
    ]
}

fn cook_error(message: impl Into<String>) -> AssetCookDiagnostic {
    AssetCookDiagnostic {
        severity: "error".to_string(),
        message: message.into(),
        source_path: None,
        line: None,
        column: None,
        source_locations: Vec::new(),
    }
}

fn cook_warning(message: impl Into<String>) -> AssetCookDiagnostic {
    AssetCookDiagnostic {
        severity: "warning".to_string(),
        message: message.into(),
        source_path: None,
        line: None,
        column: None,
        source_locations: Vec::new(),
    }
}

fn cook_error_at(
    source_path: impl Into<String>,
    line: usize,
    column: usize,
    message: impl Into<String>,
) -> AssetCookDiagnostic {
    AssetCookDiagnostic {
        severity: "error".to_string(),
        message: message.into(),
        source_path: Some(source_path.into()),
        line: Some(line),
        column: Some(column),
        source_locations: Vec::new(),
    }
}

fn diagnostics_have_errors(diagnostics: &[AssetCookDiagnostic]) -> bool {
    diagnostics
        .iter()
        .any(|diagnostic| diagnostic.severity == "error")
}

fn texture_summary_has_error(summary: &TextureImportSummary) -> bool {
    summary
        .diagnostics
        .iter()
        .any(|diagnostic| diagnostic.starts_with("error:"))
}

fn texture_encoder_name(summary: &TextureImportSummary) -> String {
    if summary.format == "ktx2" {
        "passthrough-ktx2".to_string()
    } else {
        std::env::var("ASTER_TEXTURE_ENCODER")
            .map(|value| format!("external:{value}"))
            .unwrap_or_else(|_| "missing-external-encoder".to_string())
    }
}

pub fn asset_database_error_count(database: &AssetDatabase) -> usize {
    database
        .records
        .iter()
        .flat_map(|record| record.diagnostics.iter())
        .filter(|diagnostic| diagnostic.severity == "error")
        .count()
}

pub fn asset_database_warning_count(database: &AssetDatabase) -> usize {
    database
        .records
        .iter()
        .flat_map(|record| record.diagnostics.iter())
        .filter(|diagnostic| diagnostic.severity == "warning")
        .count()
}

fn value_str<'a>(value: &'a Value, key: &str) -> Option<&'a str> {
    value.get(key).and_then(Value::as_str)
}

fn value_f32(value: &Value, key: &str) -> Option<f32> {
    value.get(key).and_then(Value::as_f64).map(|v| v as f32)
}

fn preset_value<'a>(root: &'a Value, name: &str) -> Option<&'a Value> {
    root.get("import_presets")
        .and_then(Value::as_object)
        .and_then(|presets| presets.get(name))
}

fn import_preset_record(root: &Value, asset: &Value) -> AssetImportPresetRecord {
    let name = value_str(asset, "import_preset").unwrap_or("default");
    let preset = preset_value(root, name);
    let field = |key: &str, fallback: &str| -> String {
        value_str(asset, key)
            .or_else(|| preset.and_then(|value| value_str(value, key)))
            .unwrap_or(fallback)
            .to_string()
    };
    let unit_scale = value_f32(asset, "unit_scale")
        .or_else(|| value_f32(asset, "scale"))
        .or_else(|| preset.and_then(|value| value_f32(value, "unit_scale")))
        .or_else(|| preset.and_then(|value| value_f32(value, "scale")))
        .unwrap_or(1.0);
    let origin_policy = value_str(asset, "origin_policy")
        .or_else(|| value_str(asset, "origin"))
        .or_else(|| preset.and_then(|value| value_str(value, "origin_policy")))
        .or_else(|| preset.and_then(|value| value_str(value, "origin")))
        .unwrap_or("keep");
    AssetImportPresetRecord {
        name: name.to_string(),
        origin_policy: origin_policy.to_string(),
        unit_scale: format!("{unit_scale:.9}"),
        texture_role_policy: field("texture_role_policy", "strict-litpbr-v2"),
        material_slot_policy: field("material_slot_policy", "preserve"),
        collision_policy: field("collision_policy", "static-triangle-mesh"),
        lod_policy: field("lod_policy", "audit-only"),
        meshlet_policy: field("meshlet_policy", "deterministic-64-126-audit"),
        skeleton_policy: field("skeleton_policy", "import-and-validate"),
        animation_policy: field("animation_policy", "import-clips"),
        morph_policy: field("morph_policy", "preserve-and-validate"),
    }
}

fn platform_profile_record(root: &Value, platform: &str) -> AssetPlatformProfileRecord {
    let profile = root
        .get("platform_profiles")
        .and_then(Value::as_object)
        .and_then(|profiles| profiles.get(platform));
    let field = |key: &str, fallback: &str| -> String {
        profile
            .and_then(|value| value_str(value, key))
            .unwrap_or(fallback)
            .to_string()
    };
    AssetPlatformProfileRecord {
        name: platform.to_string(),
        runtime_texture_format: field("runtime_texture_format", "ktx2"),
        compression: field("compression", "ktx2-basis"),
        target: field("target", platform),
    }
}

fn compile_options_from_preset(preset: &AssetImportPresetRecord) -> CompileOptions {
    CompileOptions {
        origin_policy: OriginPolicy::parse(&preset.origin_policy).unwrap_or(OriginPolicy::Keep),
        unit_scale: preset.unit_scale.parse::<f32>().unwrap_or(1.0),
    }
}

#[derive(Clone, Debug, PartialEq)]
struct ParsedAssetGraphSource {
    id: String,
    schema_version: u32,
    name: String,
    material_id: String,
    source_path: String,
    surface_profile: String,
    primitive: String,
    uv_policy: String,
    tangent_policy: String,
    collision_proxy: String,
    lod_policy: String,
    base_color: [f32; 3],
    emission_color: [f32; 3],
    roughness: f32,
    metallic: f32,
    emission_strength: f32,
    opacity: f32,
    double_sided: bool,
    alpha_mode: String,
    receives_shadows: bool,
    params: BTreeMap<String, f32>,
    features: BTreeMap<String, bool>,
    preview: BTreeMap<String, String>,
    perceptual_template: Option<AssetGraphPerceptualTemplate>,
    nodes: Vec<ProceduralGraphNode>,
    edges: Vec<ProceduralGraphEdge>,
    metadata: BTreeMap<String, String>,
    sockets: Vec<ProceduralGraphSocket>,
    zones: Vec<ProceduralGraphZone>,
    bundle_items: Vec<ProceduralGraphBundleItem>,
    bake_targets: Vec<ProceduralGraphBakeTarget>,
    proof_artifacts: Vec<AssetGraphProofArtifact>,
    diagnostics: Vec<AssetCookDiagnostic>,
}

impl ParsedAssetGraphSource {
    fn new(fallback_id: &str, source_path: &Path) -> Self {
        Self {
            id: fallback_id.to_string(),
            schema_version: 1,
            name: fallback_id.to_string(),
            material_id: fallback_id.to_string(),
            source_path: source_path.to_string_lossy().replace('\\', "/"),
            surface_profile: "stratified-rock".to_string(),
            primitive: "rock".to_string(),
            uv_policy: "triplanar".to_string(),
            tangent_policy: "validate-or-generate".to_string(),
            collision_proxy: "none".to_string(),
            lod_policy: "single-lod".to_string(),
            base_color: [1.0, 1.0, 1.0],
            emission_color: [0.0, 0.0, 0.0],
            roughness: 0.55,
            metallic: 0.0,
            emission_strength: 0.0,
            opacity: 1.0,
            double_sided: false,
            alpha_mode: "Opaque".to_string(),
            receives_shadows: true,
            params: BTreeMap::new(),
            features: BTreeMap::new(),
            preview: BTreeMap::new(),
            perceptual_template: None,
            nodes: Vec::new(),
            edges: Vec::new(),
            metadata: BTreeMap::new(),
            sockets: Vec::new(),
            zones: Vec::new(),
            bundle_items: Vec::new(),
            bake_targets: Vec::new(),
            proof_artifacts: Vec::new(),
            diagnostics: Vec::new(),
        }
    }
}

fn strip_graph_comment(line: &str) -> String {
    let mut out = String::new();
    let mut quoted = false;
    let mut previous = '\0';
    let chars = line.chars().peekable();
    for c in chars {
        if c == '"' && previous != '\\' {
            quoted = !quoted;
        }
        if !quoted && c == '#' {
            break;
        }
        if !quoted && previous == '/' && c == '/' {
            out.pop();
            break;
        }
        out.push(c);
        previous = c;
    }
    out
}

fn tokenize_graph_line(line: &str) -> Vec<String> {
    let line = strip_graph_comment(line);
    let mut tokens = Vec::new();
    let mut current = String::new();
    let mut quoted = false;
    let mut escaped = false;
    for c in line.chars() {
        if escaped {
            current.push(c);
            escaped = false;
            continue;
        }
        if quoted && c == '\\' {
            escaped = true;
            continue;
        }
        if c == '"' {
            quoted = !quoted;
            continue;
        }
        if !quoted && c.is_whitespace() {
            if !current.is_empty() {
                tokens.push(std::mem::take(&mut current));
            }
            continue;
        }
        current.push(c);
    }
    if !current.is_empty() {
        tokens.push(current);
    }
    tokens
}

fn parse_graph_f32(token: Option<&String>, field: &str) -> Result<f32> {
    let Some(token) = token else {
        return Err(ContentError::new(format!(
            "missing value for graph field '{field}'"
        )));
    };
    let value = token.parse::<f32>().map_err(|_| {
        ContentError::new(format!(
            "invalid number '{token}' for graph field '{field}'"
        ))
    })?;
    if !value.is_finite() {
        return Err(ContentError::new(format!(
            "non-finite number for graph field '{field}'"
        )));
    }
    Ok(value)
}

fn parse_graph_bool(token: Option<&String>, field: &str) -> Result<bool> {
    let Some(token) = token else {
        return Err(ContentError::new(format!(
            "missing value for graph field '{field}'"
        )));
    };
    match token.as_str() {
        "true" | "yes" | "1" => Ok(true),
        "false" | "no" | "0" => Ok(false),
        _ => Err(ContentError::new(format!(
            "invalid boolean '{token}' for graph field '{field}'"
        ))),
    }
}

fn canonical_graph_node_kind(kind: &str) -> String {
    kind.replace('-', "_").to_ascii_lowercase()
}

fn graph_node_kind_is_runtime_geometry_alias(kind: &str) -> bool {
    matches!(
        kind,
        "mesh_primitive_cube"
            | "mesh_primitive_grid"
            | "mesh_primitive_uv_sphere"
            | "mesh_primitive_cylinder"
            | "mesh_primitive_cone"
            | "mesh_primitive_line"
            | "curve_primitive_line"
            | "bounding_box"
            | "mesh_to_points"
            | "distribute_points_on_faces"
            | "scatter_points"
            | "instance_on_points"
            | "transform_geometry"
            | "join_geometry"
            | "separate_geometry"
            | "realize_instances"
            | "merge_by_distance"
            | "triangulate"
            | "extrude_mesh"
            | "flip_faces"
            | "uv_pack_islands"
            | "set_material"
            | "set_material_index"
    )
}

fn graph_node_kind_is_descriptor_only_geometry(kind: &str) -> bool {
    matches!(
        kind,
        "attribute_statistic"
            | "attribute_capture"
            | "evaluate_on_domain"
            | "evaluate_at_index"
            | "field_average"
            | "field_min_and_max"
            | "field_variance"
            | "accumulate_field"
            | "field_to_list"
            | "field_to_grid"
            | "grid_curl"
            | "grid_gradient"
            | "grid_laplacian"
            | "grid_mean"
            | "grid_median"
            | "grid_dilate_erode"
            | "grid_advect"
            | "grid_clip"
            | "grid_prune"
            | "grid_info"
            | "grid_to_mesh"
            | "grid_to_points"
            | "grid_voxelize"
            | "sdf_grid_boolean"
            | "sdf_grid_offset"
            | "sdf_grid_fillet"
            | "sdf_grid_laplacian"
            | "sdf_grid_mean"
            | "sdf_grid_median"
            | "sdf_grid_mean_curvature"
            | "mesh_to_volume"
            | "volume_to_mesh"
            | "volume_cube"
            | "points_to_volume"
            | "points_to_sdf_grid"
            | "mesh_to_sdf_grid"
            | "mesh_to_density_grid"
            | "raycast"
            | "sample_nearest"
            | "sample_nearest_surface"
            | "sample_index"
            | "sample_uv_surface"
            | "sample_grid"
            | "sample_grid_index"
            | "subdivision_surface"
            | "convex_hull"
            | "boolean"
            | "carve"
            | "bevel"
            | "fracture"
            | "mesh_boolean"
            | "dual_mesh"
            | "mesh_subdivide"
            | "mesh_to_curve"
            | "curve_to_mesh"
            | "curve_to_points"
            | "points_to_curves"
            | "curve_fill"
            | "curve_sample"
            | "curve_trim"
            | "curve_length"
            | "curve_reverse"
            | "curve_resample"
            | "curve_subdivide"
            | "curve_fillet"
            | "curve_primitive_circle"
            | "curve_primitive_arc"
            | "curve_primitive_spiral"
            | "curve_primitive_star"
            | "curve_primitive_quadrilateral"
            | "curve_primitive_bezier_segment"
            | "curve_primitive_quadratic_bezier"
            | "string_to_curves"
            | "gizmo_transform"
            | "gizmo_linear"
            | "gizmo_dial"
            | "viewer"
            | "tool_selection"
            | "tool_set_selection"
            | "tool_face_set"
            | "tool_set_face_set"
            | "tool_3d_cursor"
            | "tool_active_element"
            | "object_info"
            | "collection_info"
            | "collection_children"
            | "self_object"
            | "camera_info"
            | "viewport_transform"
            | "is_viewport"
            | "import_obj"
            | "import_ply"
            | "import_stl"
            | "import_vdb"
            | "import_csv"
            | "import_text"
            | "image"
            | "image_texture"
            | "image_info"
            | "sample_sound_frequencies"
            | "xpbd_solver"
    )
}

fn graph_node_capability_status(kind: &str) -> &'static str {
    let canonical = canonical_graph_node_kind(kind);
    let kind = canonical.as_str();
    if graph_node_kind_is_runtime_geometry_alias(kind) {
        return "runtime-procedural-reference";
    }
    if graph_node_kind_is_descriptor_only_geometry(kind) {
        return "descriptor-only-reference";
    }
    match kind {
        "mesh_primitive"
        | "grid_primitive"
        | "uv_sphere"
        | "cone_primitive"
        | "cylinder_cone"
        | "uv_policy"
        | "uv_pack"
        | "attribute_transfer"
        | "tangent_validation"
        | "material_assignment"
        | "mask_generator"
        | "noise"
        | "cellular"
        | "curvature_mask"
        | "slope_mask"
        | "cavity_dirt"
        | "edge_wear"
        | "wetness_flow"
        | "rust_spread"
        | "moss_growth"
        | "decal_layer"
        | "orm_baker"
        | "normal_height"
        | "normal_baker"
        | "height_baker"
        | "collision_proxy"
        | "lod_generator"
        | "pipe_body"
        | "bevel_modifier"
        | "soft_rim_normals"
        | "weld_seam"
        | "contact_skirt"
        | "seam_inset"
        | "depth_bias_policy"
        | "layered_corrosion"
        | "roughness_metalness_split"
        | "weld_slag"
        | "rust_bloom"
        | "black_scab"
        | "paint_remnant"
        | "weld_scorch"
        | "weld_bead_displacement"
        | "rim_soot"
        | "rust_mask"
        | "voronoi_pitting"
        | "oxide_layer"
        | "cavity_occlusion"
        | "edge_angle_wear"
        | "weld_heat_tint"
        | "wet_film"
        | "axial_scratch"
        | "triplanar_domain"
        | "baked_mask_preview"
        | "factory_recipe"
        | "foundry_recipe"
        | "factory_stage"
        | "foundry_stage"
        | "surface_contract"
        | "foundry_surface_contract"
        | "world_perceptual_template"
        | "physics_proxy"
        | "foundry_physics_proxy"
        | "lod_recipe"
        | "foundry_lod_recipe"
        | "quality_signal"
        | "visual_brief_claim"
        | "separate_geometry"
        | "realize_instances"
        | "anatomy_landmark"
        | "ellipsoid_section"
        | "sweep_limb"
        | "bilophodont_tooth_row"
        | "joint_range"
        | "muscle_volume"
        | "tendon_band"
        | "surface_pad"
        | "rib_cage"
        | "suture_curve"
        | "fur_guide"
        | "epidermal_strata"
        | "dermal_lattice"
        | "hypodermal_vascular_field"
        | "follicle_distribution"
        | "pigment_mask"
        | "gland_cluster"
        | "capillary_translucency"
        | "surface_tension_line"
        | "micro_abrasion"
        | "surface_displacement"
        | "anatomical_texture"
        | "measurement_probe"
        | "probe_helper"
        | "prefab_variant"
        | "cook_export"
        | "diagnostic" => "runtime-procedural-reference",
        _ => "unsupported",
    }
}

fn parse_graph_param_tokens(tokens: &[String]) -> BTreeMap<String, String> {
    let mut params = BTreeMap::new();
    for token in tokens {
        if let Some((key, value)) = token.split_once('=') {
            params.insert(key.to_string(), value.to_string());
        }
    }
    params
}

fn graph_param_f32(params: &BTreeMap<String, String>, key: &str, fallback: f32) -> f32 {
    params
        .get(key)
        .and_then(|value| value.parse::<f32>().ok())
        .unwrap_or(fallback)
}

fn graph_param_list(
    params: &BTreeMap<String, String>,
    key: &str,
    fallback: &[&str],
) -> Vec<String> {
    params
        .get(key)
        .map(|value| {
            value
                .split(',')
                .map(str::trim)
                .filter(|entry| !entry.is_empty())
                .map(str::to_string)
                .collect::<Vec<_>>()
        })
        .filter(|values| !values.is_empty())
        .unwrap_or_else(|| fallback.iter().map(|entry| (*entry).to_string()).collect())
}

fn perceptual_template_from_params(
    node_id: &str,
    params: &BTreeMap<String, String>,
    surface_profile: &str,
) -> AssetGraphPerceptualTemplate {
    AssetGraphPerceptualTemplate {
        id: params
            .get("id")
            .cloned()
            .unwrap_or_else(|| node_id.to_string()),
        valid_primitive_profile: params
            .get("valid_primitive_profile")
            .or_else(|| params.get("profile"))
            .cloned()
            .unwrap_or_else(|| surface_profile.to_string()),
        surface_response: params
            .get("surface_response")
            .cloned()
            .unwrap_or_else(|| surface_profile.to_string()),
        history_response: params
            .get("history_response")
            .cloned()
            .unwrap_or_else(|| "material-memory".to_string()),
        material_half_life_seconds: graph_param_f32(params, "material_half_life_seconds", 12.0),
        wetness_half_life_seconds: graph_param_f32(params, "wetness_half_life_seconds", 12.0),
        semantic_lod: graph_param_f32(params, "semantic_lod", 0.72),
        streaming_cost: graph_param_f32(params, "streaming_cost", 0.25),
        required_patch_channels: graph_param_list(
            params,
            "patch_channels",
            &["wetness", "exposure", "material_stability"],
        ),
        required_contact_channels: graph_param_list(
            params,
            "contact_channels",
            &["contact_normal", "visual_occlusion", "traversal_affordance"],
        ),
        required_residue_channels: graph_param_list(
            params,
            "residue_channels",
            &[
                "interaction_residue",
                "acoustic_occlusion",
                "player_readable_cause",
            ],
        ),
    }
}

fn push_graph_node(parsed: &mut ParsedAssetGraphSource, tokens: &[String]) -> Result<()> {
    if tokens.len() < 3 {
        return Err(ContentError::new(
            "graph node requires: node <id> <kind> [key=value...]",
        ));
    }
    let params = parse_graph_param_tokens(&tokens[3..]);
    let kind = canonical_graph_node_kind(&tokens[2]);
    let status = graph_node_capability_status(&kind).to_string();
    let role = params.get("role").cloned().unwrap_or_default();
    let label = params.get("label").cloned().unwrap_or_default();
    if status == "unsupported" {
        parsed.diagnostics.push(cook_warning(format!(
            "asset graph node '{}' uses unsupported kind '{}'",
            tokens[1], tokens[2]
        )));
    }
    if kind == "mesh_primitive" {
        if let Some(primitive) = params.get("primitive") {
            parsed.primitive = primitive.clone();
        }
    } else if kind == "pipe_body" {
        parsed.primitive = params
            .get("primitive")
            .cloned()
            .unwrap_or_else(|| "rusted-pipe".to_string());
    } else if kind == "uv_policy" {
        if let Some(policy) = params.get("mapping").or_else(|| params.get("policy")) {
            parsed.uv_policy = policy.clone();
        }
    } else if kind == "tangent_validation" {
        if let Some(policy) = params.get("policy") {
            parsed.tangent_policy = policy.clone();
        }
    } else if kind == "collision_proxy" {
        if let Some(policy) = params.get("shape").or_else(|| params.get("policy")) {
            parsed.collision_proxy = policy.clone();
        }
    } else if kind == "lod_generator" {
        if let Some(policy) = params.get("policy") {
            parsed.lod_policy = policy.clone();
        }
    } else if kind == "world_perceptual_template" {
        parsed.perceptual_template = Some(perceptual_template_from_params(
            &tokens[1],
            &params,
            &parsed.surface_profile,
        ));
    }
    parsed.nodes.push(ProceduralGraphNode {
        id: tokens[1].clone(),
        kind,
        role,
        label,
        params,
        capability_status: status,
    });
    Ok(())
}

fn push_graph_socket(parsed: &mut ParsedAssetGraphSource, tokens: &[String]) -> Result<()> {
    if tokens.len() < 4 {
        return Err(ContentError::new(
            "socket requires: socket <node_id> <name> <type> [key=value...]",
        ));
    }
    let params = parse_graph_param_tokens(&tokens[4..]);
    parsed.sockets.push(ProceduralGraphSocket {
        node_id: tokens[1].clone(),
        name: tokens[2].clone(),
        socket_type: tokens[3].clone(),
        direction: params
            .get("direction")
            .cloned()
            .unwrap_or_else(|| "input".to_string()),
        role: params.get("role").cloned().unwrap_or_default(),
        default_value: params.get("default").cloned().unwrap_or_default(),
    });
    Ok(())
}

fn push_graph_zone(parsed: &mut ParsedAssetGraphSource, tokens: &[String]) -> Result<()> {
    if tokens.len() < 3 {
        return Err(ContentError::new(
            "zone requires: zone <id> <kind> [key=value...]",
        ));
    }
    let params = parse_graph_param_tokens(&tokens[3..]);
    parsed.zones.push(ProceduralGraphZone {
        id: tokens[1].clone(),
        kind: canonical_graph_node_kind(&tokens[2]),
        input_node: params.get("input").cloned().unwrap_or_default(),
        output_node: params.get("output").cloned().unwrap_or_default(),
        items: graph_param_list(&params, "items", &[]),
    });
    Ok(())
}

fn push_graph_bundle_item(parsed: &mut ParsedAssetGraphSource, tokens: &[String]) -> Result<()> {
    if tokens.len() < 4 {
        return Err(ContentError::new(
            "bundle_item requires: bundle_item <bundle_id> <name> <type> [key=value...]",
        ));
    }
    let params = parse_graph_param_tokens(&tokens[4..]);
    parsed.bundle_items.push(ProceduralGraphBundleItem {
        bundle_id: tokens[1].clone(),
        name: tokens[2].clone(),
        socket_type: tokens[3].clone(),
        source_node: params.get("source").cloned().unwrap_or_default(),
    });
    Ok(())
}

fn push_graph_bake_target(parsed: &mut ParsedAssetGraphSource, tokens: &[String]) -> Result<()> {
    if tokens.len() < 3 {
        return Err(ContentError::new(
            "bake_target requires: bake_target <id> <node_id> [key=value...]",
        ));
    }
    let params = parse_graph_param_tokens(&tokens[3..]);
    parsed.bake_targets.push(ProceduralGraphBakeTarget {
        id: tokens[1].clone(),
        node_id: tokens[2].clone(),
        target: params
            .get("target")
            .cloned()
            .unwrap_or_else(|| "preview".to_string()),
        artifact_role: params
            .get("artifact")
            .or_else(|| params.get("role"))
            .cloned()
            .unwrap_or_else(|| "preview".to_string()),
        frame_start: params
            .get("frame_start")
            .and_then(|value| value.parse::<u32>().ok())
            .unwrap_or(0),
        frame_end: params
            .get("frame_end")
            .and_then(|value| value.parse::<u32>().ok())
            .unwrap_or(0),
    });
    Ok(())
}

fn push_graph_proof_artifact(parsed: &mut ParsedAssetGraphSource, tokens: &[String]) -> Result<()> {
    if tokens.len() < 4 {
        return Err(ContentError::new(
            "proof_artifact requires: proof_artifact <id> <role> <path> [key=value...]",
        ));
    }
    let params = parse_graph_param_tokens(&tokens[4..]);
    parsed.proof_artifacts.push(AssetGraphProofArtifact {
        id: tokens[1].clone(),
        role: tokens[2].clone(),
        path: tokens[3].clone(),
        kind: params
            .get("kind")
            .cloned()
            .unwrap_or_else(|| "png".to_string()),
        hash: params.get("hash").cloned().unwrap_or_default(),
        width: params
            .get("width")
            .and_then(|value| value.parse::<u32>().ok())
            .unwrap_or(0),
        height: params
            .get("height")
            .and_then(|value| value.parse::<u32>().ok())
            .unwrap_or(0),
        signal_tags: graph_param_list(&params, "signals", &[]),
    });
    Ok(())
}

fn parse_asset_graph_source(
    source: &str,
    fallback_id: &str,
    source_path: &Path,
) -> Result<ParsedAssetGraphSource> {
    let mut parsed = ParsedAssetGraphSource::new(fallback_id, source_path);
    for (line_index, line) in source.lines().enumerate() {
        let tokens = tokenize_graph_line(line);
        if tokens.is_empty() {
            continue;
        }
        match tokens[0].as_str() {
            "astergraph" | "asset_graph" => {
                if let Some(id) = tokens.get(1) {
                    parsed.id = id.clone();
                    parsed.material_id = id.clone();
                    if parsed.name == fallback_id {
                        parsed.name = id.clone();
                    }
                }
            }
            "schema_version" => {
                parsed.schema_version = parse_graph_f32(tokens.get(1), "schema_version")? as u32;
            }
            "name" => parsed.name = tokens.get(1).cloned().unwrap_or_default(),
            "material_id" => {
                parsed.material_id = tokens.get(1).cloned().unwrap_or_else(|| parsed.id.clone());
            }
            "surface_profile" => {
                parsed.surface_profile = tokens
                    .get(1)
                    .cloned()
                    .unwrap_or_else(|| "stratified-rock".to_string());
            }
            "primitive" => {
                parsed.primitive = tokens.get(1).cloned().unwrap_or_else(|| "rock".to_string());
            }
            "uv_policy" => {
                parsed.uv_policy = tokens
                    .get(1)
                    .cloned()
                    .unwrap_or_else(|| "triplanar".to_string());
            }
            "tangent_policy" => {
                parsed.tangent_policy = tokens
                    .get(1)
                    .cloned()
                    .unwrap_or_else(|| "validate-or-generate".to_string());
            }
            "collision_proxy" => {
                parsed.collision_proxy =
                    tokens.get(1).cloned().unwrap_or_else(|| "none".to_string());
            }
            "lod_policy" => {
                parsed.lod_policy = tokens
                    .get(1)
                    .cloned()
                    .unwrap_or_else(|| "single-lod".to_string());
            }
            "base_color" => {
                parsed.base_color = [
                    parse_graph_f32(tokens.get(1), "base_color.r")?,
                    parse_graph_f32(tokens.get(2), "base_color.g")?,
                    parse_graph_f32(tokens.get(3), "base_color.b")?,
                ];
            }
            "emission_color" => {
                parsed.emission_color = [
                    parse_graph_f32(tokens.get(1), "emission_color.r")?,
                    parse_graph_f32(tokens.get(2), "emission_color.g")?,
                    parse_graph_f32(tokens.get(3), "emission_color.b")?,
                ];
            }
            "roughness" => parsed.roughness = parse_graph_f32(tokens.get(1), "roughness")?,
            "metallic" => parsed.metallic = parse_graph_f32(tokens.get(1), "metallic")?,
            "emission_strength" => {
                parsed.emission_strength = parse_graph_f32(tokens.get(1), "emission_strength")?;
            }
            "opacity" => parsed.opacity = parse_graph_f32(tokens.get(1), "opacity")?,
            "double_sided" => {
                parsed.double_sided = parse_graph_bool(tokens.get(1), "double_sided")?;
            }
            "alpha_mode" => {
                parsed.alpha_mode = tokens
                    .get(1)
                    .cloned()
                    .unwrap_or_else(|| "Opaque".to_string());
            }
            "receives_shadows" => {
                parsed.receives_shadows = parse_graph_bool(tokens.get(1), "receives_shadows")?;
            }
            "param" => {
                if tokens.len() < 3 {
                    return Err(ContentError::new("param requires: param <name> <value>"));
                }
                parsed.params.insert(
                    tokens[1].clone(),
                    parse_graph_f32(tokens.get(2), &tokens[1])?,
                );
            }
            "feature" => {
                if tokens.len() < 3 {
                    return Err(ContentError::new("feature requires: feature <name> <bool>"));
                }
                parsed.features.insert(
                    tokens[1].clone(),
                    parse_graph_bool(tokens.get(2), &tokens[1])?,
                );
            }
            "preview" => {
                if tokens.len() < 3 {
                    return Err(ContentError::new(
                        "preview requires: preview <name> <value>",
                    ));
                }
                parsed.preview.insert(tokens[1].clone(), tokens[2].clone());
            }
            "metadata" => {
                if tokens.len() < 3 {
                    return Err(ContentError::new(
                        "metadata requires: metadata <name> <value>",
                    ));
                }
                parsed.metadata.insert(tokens[1].clone(), tokens[2].clone());
            }
            "node" => push_graph_node(&mut parsed, &tokens)?,
            "socket" => push_graph_socket(&mut parsed, &tokens)?,
            "zone" => push_graph_zone(&mut parsed, &tokens)?,
            "bundle_item" => push_graph_bundle_item(&mut parsed, &tokens)?,
            "bake_target" => push_graph_bake_target(&mut parsed, &tokens)?,
            "proof_artifact" => push_graph_proof_artifact(&mut parsed, &tokens)?,
            "edge" => {
                if tokens.len() >= 5 && tokens[2] == "->" {
                    parsed.edges.push(ProceduralGraphEdge {
                        from: tokens[1].clone(),
                        to: tokens[3].clone(),
                        role: tokens[4].clone(),
                    });
                } else if tokens.len() >= 4 {
                    parsed.edges.push(ProceduralGraphEdge {
                        from: tokens[1].clone(),
                        to: tokens[2].clone(),
                        role: tokens[3].clone(),
                    });
                } else {
                    return Err(ContentError::new("edge requires: edge <from> <to> <role>"));
                }
            }
            value => parsed.diagnostics.push(AssetCookDiagnostic {
                severity: "warning".to_string(),
                message: format!("unknown asset graph directive '{value}'"),
                source_path: Some(source_path.to_string_lossy().to_string()),
                line: Some(line_index + 1),
                column: Some(1),
                source_locations: Vec::new(),
            }),
        }
    }
    for (name, fallback) in [
        ("roughness", parsed.roughness),
        ("metallic", parsed.metallic),
        ("wetness", 0.0),
        ("macro_variation", 0.0),
        ("micro_normal_strength", 0.0),
        ("roughness_variation", 0.0),
        ("physical_texel_density", 512.0),
        ("height_normal_coupling", 0.0),
        ("roughness_height_coupling", 0.0),
        ("macro_frequency_breakup", 0.0),
        ("micro_frequency_breakup", 0.0),
        ("height_shading", 0.0),
        ("pitting_density", 0.0),
        ("pitting_depth", 0.0),
        ("oxide_layering", 0.0),
        ("cavity_grime", 0.0),
        ("edge_polish", 0.0),
        ("weld_heat_tint", 0.0),
        ("axial_scratches", 0.0),
        ("wet_streaks", 0.0),
        ("rust_bloom", 0.0),
        ("black_scab", 0.0),
        ("paint_remnant", 0.0),
        ("weld_slag", 0.0),
        ("rim_soot", 0.0),
    ] {
        parsed.params.entry(name.to_string()).or_insert(fallback);
    }
    if parsed.perceptual_template.is_none() {
        return Err(ContentError::new(format!(
            "asset graph '{}' exports without required world_perceptual_template node",
            parsed.id
        )));
    }
    Ok(parsed)
}

fn graph_feature_mask(parsed: &ParsedAssetGraphSource) -> u64 {
    let mut mask = 1u64;
    let mut set = |bit: u64| {
        mask |= 1u64 << bit;
    };
    for node in &parsed.nodes {
        match node.kind.as_str() {
            "mesh_primitive"
            | "grid_primitive"
            | "uv_sphere"
            | "cone_primitive"
            | "cylinder_cone"
            | "mesh_primitive_cube"
            | "mesh_primitive_grid"
            | "mesh_primitive_uv_sphere"
            | "mesh_primitive_cylinder"
            | "mesh_primitive_cone"
            | "mesh_primitive_line"
            | "curve_primitive_line" => set(1),
            "material_assignment" | "set_material" | "set_material_index" => set(2),
            "noise" => set(3),
            "cellular" => set(4),
            "curvature_mask" | "slope_mask" | "cavity_dirt" | "mask_generator" => set(5),
            "wetness_flow" => set(6),
            "rust_spread" => set(7),
            "moss_growth" => set(8),
            "decal_layer" => set(9),
            "orm_baker" => set(10),
            "normal_height" | "normal_baker" => set(11),
            "height_baker" => set(12),
            "collision_proxy" => set(13),
            "lod_generator" => set(14),
            "world_perceptual_template" => set(56),
            "factory_recipe" | "foundry_recipe" => set(57),
            "factory_stage" | "foundry_stage" => set(58),
            "surface_contract" | "foundry_surface_contract" => set(59),
            "physics_proxy" | "foundry_physics_proxy" => set(60),
            "lod_recipe" | "foundry_lod_recipe" => set(61),
            "quality_signal" => set(62),
            "visual_brief_claim" => set(63),
            "pipe_body" => set(39),
            "bevel_modifier" => set(40),
            "weld_seam" => set(41),
            "soft_rim_normals" => set(51),
            "contact_skirt" => set(52),
            "seam_inset" => set(53),
            "depth_bias_policy" => set(54),
            "layered_corrosion"
            | "weld_slag"
            | "rust_bloom"
            | "black_scab"
            | "paint_remnant"
            | "weld_scorch"
            | "weld_bead_displacement"
            | "rim_soot" => set(55),
            "roughness_metalness_split" => set(56),
            "rust_mask" => set(42),
            "voronoi_pitting" => set(43),
            "oxide_layer" => set(44),
            "cavity_occlusion" => set(45),
            "edge_angle_wear" => set(46),
            "weld_heat_tint" => set(47),
            "wet_film" => set(48),
            "axial_scratch" => set(49),
            "triplanar_domain" | "baked_mask_preview" => set(50),
            "probe_helper" | "prefab_variant" | "cook_export" | "diagnostic" => set(15),
            kind if graph_node_kind_is_runtime_geometry_alias(kind)
                || graph_node_kind_is_descriptor_only_geometry(kind) =>
            {
                set(15)
            }
            "anatomy_landmark" | "measurement_probe" => set(20),
            "ellipsoid_section" | "sweep_limb" => set(21),
            "bilophodont_tooth_row" => set(22),
            "joint_range" => set(23),
            "muscle_volume" | "tendon_band" => set(24),
            "surface_pad" => set(25),
            "rib_cage" | "suture_curve" => set(26),
            "fur_guide" => set(27),
            "surface_displacement" => set(28),
            "anatomical_texture" => set(29),
            "epidermal_strata" => set(30),
            "dermal_lattice" => set(31),
            "hypodermal_vascular_field" => set(32),
            "follicle_distribution" => set(33),
            "pigment_mask" => set(34),
            "gland_cluster" => set(35),
            "capillary_translucency" => set(36),
            "surface_tension_line" => set(37),
            "micro_abrasion" => set(38),
            _ => {}
        }
    }
    for (feature, enabled) in &parsed.features {
        if *enabled {
            match feature.as_str() {
                "triplanar" => set(16),
                "parallax" => set(17),
                "normal_map" => set(18),
                "decal_receiver" => set(19),
                _ => {}
            }
        }
    }
    mask
}

fn graph_hash_u64<T: Serialize>(tag: &str, value: &T) -> u64 {
    let bytes = serde_json::to_vec(value).unwrap_or_default();
    let mut hasher = blake3::Hasher::new();
    hasher.update(tag.as_bytes());
    hasher.update(b"\0");
    hasher.update(&bytes);
    let digest = hasher.finalize();
    let mut out = [0u8; 8];
    out.copy_from_slice(&digest.as_bytes()[..8]);
    u64::from_le_bytes(out)
}

fn asset_graph_quality_report(
    parsed: &ParsedAssetGraphSource,
    diagnostics: &[AssetCookDiagnostic],
) -> AssetGraphQualityReport {
    let mut issues = Vec::new();
    let mut push_issue = |severity: &str, category: &str, node: &str, message: &str| {
        issues.push(AssetGraphQualityIssue {
            severity: severity.to_string(),
            category: category.to_string(),
            node: node.to_string(),
            message: message.to_string(),
        });
    };
    let has_kind = |kind: &str| parsed.nodes.iter().any(|node| node.kind == kind);
    let has_mesh_primitive = parsed.nodes.iter().any(|node| {
        matches!(
            node.kind.as_str(),
            "mesh_primitive"
                | "pipe_body"
                | "mesh_primitive_cube"
                | "mesh_primitive_grid"
                | "mesh_primitive_uv_sphere"
                | "mesh_primitive_cylinder"
                | "mesh_primitive_cone"
                | "mesh_primitive_line"
                | "curve_primitive_line"
                | "grid_primitive"
                | "uv_sphere"
                | "cone_primitive"
                | "cylinder_cone"
        )
    });
    let normalized_surface_profile = parsed
        .surface_profile
        .replace('_', "-")
        .to_ascii_lowercase();
    for required in [
        "mesh_primitive",
        "uv_policy",
        "tangent_validation",
        "material_assignment",
        "collision_proxy",
        "lod_generator",
        "cook_export",
        "diagnostic",
    ] {
        if required == "mesh_primitive" && has_mesh_primitive {
            continue;
        }
        if !has_kind(required) {
            push_issue(
                "warning",
                "graph",
                required,
                "full asset graph v1 expects this node family to be represented",
            );
        }
    }
    if matches!(
        normalized_surface_profile.as_str(),
        "biological-integument" | "integument" | "skin-fur" | "fur-skin" | "dermal-fur"
    ) {
        for required in [
            "epidermal_strata",
            "dermal_lattice",
            "hypodermal_vascular_field",
            "follicle_distribution",
            "pigment_mask",
            "gland_cluster",
            "capillary_translucency",
            "surface_tension_line",
            "micro_abrasion",
        ] {
            if !has_kind(required) {
                push_issue(
                    "warning",
                    "integument",
                    required,
                    "biological integument graphs should expose this runtime-supported node family",
                );
            }
        }
    }
    let normalized_primitive = parsed.primitive.replace('_', "-").to_ascii_lowercase();
    let is_rusted_pipe = normalized_primitive.contains("pipe")
        || matches!(
            normalized_surface_profile.as_str(),
            "corroded-metal" | "weathered-metal" | "rusted-metal"
        );
    if is_rusted_pipe {
        for required in [
            "pipe_body",
            "factory_recipe",
            "factory_stage",
            "surface_contract",
            "physics_proxy",
            "lod_recipe",
            "quality_signal",
            "visual_brief_claim",
            "bevel_modifier",
            "soft_rim_normals",
            "weld_seam",
            "contact_skirt",
            "seam_inset",
            "depth_bias_policy",
            "layered_corrosion",
            "roughness_metalness_split",
            "rust_bloom",
            "black_scab",
            "paint_remnant",
            "rust_mask",
            "voronoi_pitting",
            "oxide_layer",
            "cavity_occlusion",
            "edge_angle_wear",
            "weld_scorch",
            "weld_slag",
            "weld_bead_displacement",
            "rim_soot",
            "wet_film",
            "axial_scratch",
            "normal_height",
        ] {
            if !has_kind(required) {
                push_issue(
                    "warning",
                    "pipe-realism",
                    required,
                    "rusted pipe graphs should expose this material realism node family",
                );
            }
        }
        if parsed.params.get("pitting_density").copied().unwrap_or(0.0) < 0.20 {
            push_issue(
                "warning",
                "pipe-realism",
                "voronoi_pitting",
                "pitting density is too low for the industrial pipe realism profile",
            );
        }
        if parsed.params.get("oxide_layering").copied().unwrap_or(0.0) < 0.20 {
            push_issue(
                "warning",
                "pipe-realism",
                "oxide_layer",
                "oxide layering is too narrow to separate dark metal from rust",
            );
        }
        if parsed.params.get("cavity_grime").copied().unwrap_or(0.0) < 0.20 {
            push_issue(
                "warning",
                "pipe-realism",
                "cavity_occlusion",
                "cavity grime is too low for weld, flange, and bolt contact areas",
            );
        }
        if parsed.params.get("rust_bloom").copied().unwrap_or(0.0) < 0.30 {
            push_issue(
                "warning",
                "pipe-realism",
                "rust_bloom",
                "rust bloom is too weak; the pipe may read as painted clay instead of layered corrosion",
            );
        }
        if parsed.params.get("black_scab").copied().unwrap_or(0.0) < 0.25 {
            push_issue(
                "warning",
                "pipe-realism",
                "black_scab",
                "black oxide/scab response is too low for aged industrial metal",
            );
        }
        if !parsed
            .preview
            .values()
            .any(|value| value.contains("pipe") || value.contains("inspection"))
        {
            push_issue(
                "warning",
                "preview",
                "rig",
                "rusted pipe graph should name an inspection or pipe preview rig",
            );
        }
    }
    if parsed
        .nodes
        .iter()
        .any(|node| node.capability_status == "unsupported")
    {
        push_issue(
            "error",
            "backend",
            "procedural",
            "one or more graph nodes are not supported by the runtime procedural reference path",
        );
    }
    let roughness_variation = parsed
        .params
        .get("roughness_variation")
        .copied()
        .unwrap_or(0.0);
    if roughness_variation < 0.05 {
        push_issue(
            "warning",
            "material",
            "roughness",
            "roughness distribution is too narrow for the production material profile",
        );
    }
    let normal_strength = parsed
        .params
        .get("micro_normal_strength")
        .copied()
        .unwrap_or(0.0);
    if normal_strength > 1.0 {
        push_issue(
            "warning",
            "material",
            "normal",
            "normal intensity may alias under grazing light",
        );
    }
    let height = parsed.params.get("height_shading").copied().unwrap_or(0.0);
    if height > 0.65 {
        push_issue(
            "warning",
            "material",
            "height",
            "height response is high enough to risk aliasing",
        );
    }
    let wetness = parsed.params.get("wetness").copied().unwrap_or(0.0);
    if wetness > 0.0 && !has_kind("wetness_flow") {
        push_issue(
            "warning",
            "material",
            "wetness",
            "wet material response has no wetness_flow node",
        );
    }
    let luminance = parsed.base_color[0] * 0.2126
        + parsed.base_color[1] * 0.7152
        + parsed.base_color[2] * 0.0722;
    if luminance < 0.08
        && !parsed
            .preview
            .values()
            .any(|value| value.contains("dark") || value.contains("cave"))
    {
        push_issue(
            "warning",
            "preview",
            "environment",
            "material may be unreadable in cave-dark preview environments",
        );
    }
    let diagnostic_errors = diagnostics
        .iter()
        .filter(|diagnostic| diagnostic.severity == "error")
        .count();
    let diagnostic_warnings = diagnostics
        .iter()
        .filter(|diagnostic| diagnostic.severity == "warning")
        .count();
    let issue_errors = issues
        .iter()
        .filter(|issue| issue.severity == "error")
        .count();
    let issue_warnings = issues
        .iter()
        .filter(|issue| issue.severity == "warning")
        .count();
    let penalty = (diagnostic_errors + issue_errors) as u32 * 30
        + (diagnostic_warnings + issue_warnings) as u32 * 8;
    let score = 100u32.saturating_sub(penalty);
    let param = |name: &str, fallback: f32| parsed.params.get(name).copied().unwrap_or(fallback);
    let mut presentation_quality = BTreeMap::new();
    presentation_quality.insert(
        "scale_cues".to_string(),
        if parsed.preview.values().any(|value| {
            value.contains("scale") || value.contains("inspection") || value.contains("cave")
        }) {
            "declared".to_string()
        } else {
            "implicit".to_string()
        },
    );
    presentation_quality.insert(
        "contact_shadows".to_string(),
        if has_kind("contact_skirt") || has_kind("shadow_receiver") {
            "authored".to_string()
        } else {
            "renderer-grounding".to_string()
        },
    );
    presentation_quality.insert(
        "surface_occlusion".to_string(),
        if has_kind("cavity_occlusion") || param("cavity_grime", 0.0) > 0.0 {
            "authored".to_string()
        } else {
            "renderer-surface-occlusion".to_string()
        },
    );
    presentation_quality.insert(
        "camera_language".to_string(),
        parsed
            .preview
            .get("rig")
            .cloned()
            .unwrap_or_else(|| "production-frame-required".to_string()),
    );
    let mut surface_stack = BTreeMap::new();
    surface_stack.insert(
        "physical_texel_density".to_string(),
        format!(
            "{:.3}",
            param("physical_texel_density", param("texel_density", 512.0))
        ),
    );
    surface_stack.insert(
        "height_normal_coupling".to_string(),
        format!("{:.3}", param("height_normal_coupling", 0.0)),
    );
    surface_stack.insert(
        "roughness_height_coupling".to_string(),
        format!("{:.3}", param("roughness_height_coupling", 0.0)),
    );
    surface_stack.insert(
        "macro_frequency_breakup".to_string(),
        format!("{:.3}", param("macro_frequency_breakup", 0.0)),
    );
    surface_stack.insert(
        "micro_frequency_breakup".to_string(),
        format!("{:.3}", param("micro_frequency_breakup", 0.0)),
    );
    AssetGraphQualityReport {
        score,
        production_ready: diagnostic_errors == 0 && issue_errors == 0 && score >= 60,
        presentation_quality,
        surface_stack,
        visual_proof_expectations: vec![
            "scale cues visible".to_string(),
            "contact shadows visible".to_string(),
            "surface occlusion visible".to_string(),
            "height normal roughness coupling declared".to_string(),
            "preview artifact listed".to_string(),
        ],
        issues,
    }
}

fn asset_graph_factory_report(parsed: &ParsedAssetGraphSource) -> AssetGraphFactoryReport {
    let factory_nodes = parsed
        .nodes
        .iter()
        .filter(|node| {
            matches!(
                node.kind.as_str(),
                "factory_recipe"
                    | "foundry_recipe"
                    | "factory_stage"
                    | "foundry_stage"
                    | "surface_contract"
                    | "foundry_surface_contract"
                    | "physics_proxy"
                    | "foundry_physics_proxy"
                    | "lod_recipe"
                    | "foundry_lod_recipe"
                    | "quality_signal"
                    | "visual_brief_claim"
            )
        })
        .cloned()
        .collect::<Vec<_>>();
    let stable_recipe_hash = format!(
        "0x{:016x}",
        graph_hash_u64(
            "aster.assetfoundry.recipe.v1",
            &(
                parsed.id.as_str(),
                parsed.primitive.as_str(),
                parsed.params.clone(),
                factory_nodes.clone(),
                parsed.edges.clone()
            ),
        )
    );

    let stage_diagnostics = parsed
        .nodes
        .iter()
        .filter(|node| node.kind == "factory_stage" || node.kind == "foundry_stage")
        .map(|node| {
            let kind = node
                .params
                .get("kind")
                .cloned()
                .unwrap_or_else(|| "unspecified".to_string());
            let mut diagnostics = Vec::new();
            if kind == "unspecified" {
                diagnostics.push("warning: factory stage is missing a kind".to_string());
            }
            if node.capability_status == "unsupported" {
                diagnostics
                    .push("error: factory stage is unsupported by runtime reference".to_string());
            }
            AssetGraphFactoryStageReport {
                id: node.id.clone(),
                kind,
                status: if diagnostics
                    .iter()
                    .any(|message| message.starts_with("error:"))
                {
                    "failed".to_string()
                } else {
                    "ready".to_string()
                },
                diagnostics,
            }
        })
        .collect::<Vec<_>>();

    let param = |name: &str, fallback: f32| parsed.params.get(name).copied().unwrap_or(fallback);
    let signal =
        |name: &str, average: f32, coverage: f32, floor: f32| AssetGraphFactorySignalCoverage {
            signal: name.to_string(),
            average,
            coverage,
            status: if average >= floor {
                "claimed".to_string()
            } else {
                "needs-work".to_string()
            },
        };
    let rust = param("rust_strength", param("rust_bloom", 0.0));
    let oxide = param("oxide_layering", 0.0);
    let pitting = param("pitting_density", 0.0);
    let cavity = param("cavity_grime", 0.0);
    let wet = param("wetness", 0.0);
    let scratches = param("axial_scratches", 0.0);
    let rim = param("rim_soot", 0.0);
    let weld = param("weld_slag", 0.0).max(param("weld_heat_tint", 0.0));
    let surface_signal_coverage = vec![
        signal(
            "corroded_orange_brown_rust",
            (rust * oxide).min(1.0),
            rust.min(1.0),
            0.50,
        ),
        signal(
            "dark_oxide_cavities",
            (oxide * cavity).min(1.0),
            cavity.min(1.0),
            0.35,
        ),
        signal("uneven_pitting", pitting.min(1.0), pitting.min(1.0), 0.35),
        signal(
            "axial_scratches",
            scratches.min(1.0),
            scratches.min(1.0),
            0.30,
        ),
        signal("open_hollow_rims", rim.min(1.0), rim.min(1.0), 0.30),
        signal("raised_weld_rings", weld.min(1.0), weld.min(1.0), 0.30),
        signal("moisture_response", wet.min(1.0), wet.min(1.0), 0.05),
    ];

    let mut collision_proxy_summary = BTreeMap::new();
    collision_proxy_summary.insert("declared".to_string(), parsed.collision_proxy.clone());
    if let Some(proxy) = parsed
        .nodes
        .iter()
        .find(|node| node.kind == "physics_proxy" || node.kind == "foundry_physics_proxy")
    {
        collision_proxy_summary.insert(
            "shape".to_string(),
            proxy
                .params
                .get("shape")
                .cloned()
                .unwrap_or_else(|| parsed.collision_proxy.clone()),
        );
        if let Some(triangles) = proxy.params.get("triangles") {
            collision_proxy_summary.insert("triangle_budget".to_string(), triangles.clone());
        }
        if let Some(material) = proxy.params.get("material") {
            collision_proxy_summary.insert("material".to_string(), material.clone());
        }
    }
    collision_proxy_summary
        .entry("lod_policy".to_string())
        .or_insert_with(|| parsed.lod_policy.clone());

    let mut visual_brief_claims = parsed
        .nodes
        .iter()
        .filter(|node| node.kind == "visual_brief_claim")
        .filter_map(|node| node.params.get("signal").cloned())
        .collect::<Vec<_>>();
    if visual_brief_claims.is_empty()
        && (parsed.primitive.contains("pipe") || parsed.surface_profile.contains("metal"))
    {
        visual_brief_claims = vec![
            "corroded_orange_brown_rust".to_string(),
            "dark_oxide_cavities".to_string(),
            "raised_weld_rings".to_string(),
            "open_hollow_rims".to_string(),
            "uneven_pitting".to_string(),
            "axial_scratches".to_string(),
            "reference_silhouette".to_string(),
        ];
    }

    let visual_brief_rejections = vec![
        "smooth_black_pipe".to_string(),
        "decorative_bolts_without_reference".to_string(),
        "clean_plastic_surface".to_string(),
        "monochrome_material".to_string(),
    ];

    AssetGraphFactoryReport {
        stable_recipe_hash,
        stage_diagnostics,
        surface_signal_coverage,
        collision_proxy_summary,
        visual_brief_claims,
        visual_brief_rejections,
    }
}

fn build_asset_graph_bin(
    parsed: ParsedAssetGraphSource,
    source: &str,
    asset_guid: String,
    source_path: String,
) -> AssetGraphBin {
    let feature_mask = graph_feature_mask(&parsed);
    let shader_seed = (
        parsed.id.as_str(),
        parsed.material_id.as_str(),
        parsed.surface_profile.as_str(),
        parsed.params.clone(),
        parsed.features.clone(),
        parsed.perceptual_template.clone(),
        parsed.nodes.clone(),
        parsed.edges.clone(),
        parsed.metadata.clone(),
        parsed.sockets.clone(),
        parsed.zones.clone(),
        parsed.bundle_items.clone(),
        parsed.bake_targets.clone(),
        parsed.proof_artifacts.clone(),
    );
    let shader_variant_key = graph_hash_u64("aster.assetgraph.shader.v1", &shader_seed);
    let shader_variant_tag = format!("AssetGraph.{}.runtime-procedural", parsed.material_id);
    let pipeline_tag = format!(
        "material:{}:{}:runtime-procedural",
        parsed.material_id, parsed.surface_profile
    );
    let dependency_hash = hash_serializable("aster.assetgraph.dependencies.v1", &parsed.edges);
    let artifact_hash = hash_serializable("aster.assetgraph.artifacts.v1", &parsed.nodes);
    let derived_hashes = AssetDerivedHashes {
        source_hash: hash_hex_text(source),
        options_hash: hash_hex_text(&format!(
            "assetgraphbin:{}:{}",
            ASSET_GRAPH_BIN_SCHEMA_VERSION, parsed.schema_version
        )),
        dependency_hash,
        artifact_hash,
        material_hash: hash_hex_text(&format!(
            "assetgraph:{}:{}:{}",
            parsed.id, parsed.material_id, pipeline_tag
        )),
        shader_variant_key: format!("0x{shader_variant_key:016x}"),
        pipeline_cache_key: hash_hex_text(&pipeline_tag),
        vertex_input_contract: hash_hex_text(&format!(
            "assetgraph-vertex:{}:{}:{}:{}",
            parsed.primitive, parsed.uv_policy, parsed.tangent_policy, parsed.lod_policy
        )),
        frame_plan_fingerprint: hash_hex_text(&format!(
            "assetgraph-frame:{}:{}:{}",
            parsed.id,
            parsed.nodes.len(),
            parsed.edges.len()
        )),
    };
    let quality = asset_graph_quality_report(&parsed, &parsed.diagnostics);
    let perceptual_template = parsed.perceptual_template.clone().unwrap_or_default();
    let production_session = AssetGraphProductionSession {
        session_id: format!("asset-production:{}", parsed.id),
        graph_hash: derived_hashes.source_hash.clone(),
        preview_artifact_hash: hash_hex_text(&format!(
            "{}:{}:{}:{}",
            parsed.id,
            feature_mask,
            quality.score,
            parsed
                .preview
                .iter()
                .map(|(key, value)| format!("{key}={value}"))
                .collect::<Vec<_>>()
                .join("|")
        )),
        quality_gate: if quality.production_ready {
            "production-ready".to_string()
        } else {
            "needs-review".to_string()
        },
        cook_steps: vec![
            "graph-inspect".to_string(),
            "graph-package".to_string(),
            "cook-project".to_string(),
            "preview-render".to_string(),
        ],
    };
    let factory_report = asset_graph_factory_report(&parsed);
    AssetGraphBin {
        schema_version: ASSET_GRAPH_BIN_SCHEMA_VERSION,
        asset_guid,
        id: parsed.id.clone(),
        name: parsed.name.clone(),
        kind: "asset_graph".to_string(),
        source_path,
        runtime_model: "runtime-procedural".to_string(),
        material: AssetGraphMaterialPackage {
            id: parsed.material_id.clone(),
            surface_profile: parsed.surface_profile.clone(),
            feature_mask,
            shader_variant_key,
            shader_variant_tag,
            pipeline_tag,
            fallback: MaterialBinFallback {
                base_color: parsed.base_color,
                emission_color: parsed.emission_color,
                roughness: parsed.roughness,
                metallic: parsed.metallic,
                emission_strength: parsed.emission_strength,
                opacity: parsed.opacity,
                double_sided: parsed.double_sided,
                alpha_mode: parsed.alpha_mode.clone(),
                receives_shadows: parsed.receives_shadows,
                surface_profile: parsed.surface_profile.clone(),
            },
            params: parsed.params.clone(),
            features: parsed.features.clone(),
        },
        mesh: AssetGraphMeshDescriptor {
            primitive: parsed.primitive.clone(),
            uv_policy: parsed.uv_policy.clone(),
            tangent_policy: parsed.tangent_policy.clone(),
            collision_proxy: parsed.collision_proxy.clone(),
            lod_policy: parsed.lod_policy.clone(),
        },
        perceptual_template,
        nodes: parsed.nodes,
        edges: parsed.edges,
        metadata: parsed.metadata,
        sockets: parsed.sockets,
        zones: parsed.zones,
        bundle_items: parsed.bundle_items,
        bake_targets: parsed.bake_targets,
        proof_artifacts: parsed.proof_artifacts,
        preview: parsed.preview,
        production_session,
        factory_report,
        quality,
        derived_hashes,
        diagnostics: parsed.diagnostics,
    }
}

fn load_asset_graph_bin_from_source(
    input: &Path,
    fallback_id: &str,
    asset_guid_override: Option<&str>,
    project_root: &Path,
) -> Result<AssetGraphBin> {
    let source = fs::read_to_string(input)?;
    let source_rel = relative_path_string(input, project_root);
    let parsed = parse_asset_graph_source(&source, fallback_id, input)?;
    let guid = asset_guid_override
        .map(str::to_string)
        .unwrap_or_else(|| asset_guid("asset_graph", &parsed.id, &source_rel));
    Ok(build_asset_graph_bin(parsed, &source, guid, source_rel))
}

pub fn inspect_asset_graph(input: impl AsRef<Path>) -> Result<AssetGraphBin> {
    let input = input.as_ref();
    let fallback_id = input
        .file_stem()
        .and_then(|value| value.to_str())
        .unwrap_or("asset_graph");
    load_asset_graph_bin_from_source(
        input,
        fallback_id,
        None,
        input.parent().unwrap_or_else(|| Path::new("")),
    )
}

pub fn asset_graph_inspect_report_json(input: impl AsRef<Path>) -> Result<String> {
    let graph = inspect_asset_graph(input)?;
    Ok(serde_json::to_string_pretty(&graph)?)
}

pub fn package_asset_graph(
    input: impl AsRef<Path>,
    output_root: impl AsRef<Path>,
) -> Result<AssetGraphCookResult> {
    let input = input.as_ref();
    let output_root = output_root.as_ref();
    let fallback_id = input
        .file_stem()
        .and_then(|value| value.to_str())
        .unwrap_or("asset_graph");
    cook_asset_graph_asset(
        input,
        input.parent().unwrap_or_else(|| Path::new("")),
        output_root,
        fallback_id,
        "desktop",
        None,
    )
}

fn read_asset_meta_guid(source: &Path) -> Result<Option<String>> {
    let mut meta_path = source.to_path_buf();
    meta_path.set_extension("astermeta");
    if !meta_path.exists() {
        return Ok(None);
    }
    let value: Value = serde_json::from_slice(&fs::read(&meta_path)?)?;
    Ok(value_str(&value, "guid").map(str::to_string))
}

fn push_output(record: &mut AssetDatabaseRecord, output: AssetCookedOutput, reused: bool) {
    record.artifacts.push(AssetArtifactRecord {
        role: output.role.clone(),
        kind: output.kind.clone(),
        path: output.path.clone(),
        hash: output.hash.clone(),
        reused,
    });
    record.outputs.push(output);
}

fn refresh_dependency_edges(record: &mut AssetDatabaseRecord) {
    record.dependency_edges = record
        .dependencies
        .iter()
        .map(|dependency| AssetDependencyEdge {
            from: record.guid.clone(),
            to: dependency.path.clone(),
            role: dependency.role.clone(),
            present: dependency.present,
            hash: dependency.hash.clone(),
        })
        .collect();
}

fn sync_record_source(record: &mut AssetDatabaseRecord) {
    record.source.path = record.source_path.clone();
    record.source.hash = record.source_hash.clone();
}

fn hash_serializable<T: Serialize>(tag: &str, value: &T) -> String {
    let bytes = serde_json::to_vec(value).unwrap_or_default();
    let mut hasher = blake3::Hasher::new();
    hasher.update(tag.as_bytes());
    hasher.update(b"\0");
    hasher.update(&bytes);
    hash_hex_bytes(hasher.finalize().as_bytes())
}

fn record_has_errors(record: &AssetDatabaseRecord) -> bool {
    record
        .diagnostics
        .iter()
        .any(|diagnostic| diagnostic.severity == "error")
}

fn render_contract_for(record: &AssetDatabaseRecord) -> Vec<String> {
    let mut contract = Vec::new();
    match record.kind.as_str() {
        "material" => {
            contract.push("material-source".to_string());
            if !record.derived_hashes.material_hash.is_empty() {
                contract.push(format!(
                    "material-hash:{}",
                    record.derived_hashes.material_hash
                ));
            }
            if !record.derived_hashes.shader_variant_key.is_empty() {
                contract.push(format!(
                    "shader-variant:{}",
                    record.derived_hashes.shader_variant_key
                ));
            }
            if !record.derived_hashes.pipeline_cache_key.is_empty() {
                contract.push(format!(
                    "pipeline-cache:{}",
                    record.derived_hashes.pipeline_cache_key
                ));
            }
        }
        "scene" => {
            contract.push("scene-package".to_string());
            if !record.derived_hashes.vertex_input_contract.is_empty() {
                contract.push(format!(
                    "vertex-input:{}",
                    record.derived_hashes.vertex_input_contract
                ));
            }
            if !record.derived_hashes.frame_plan_fingerprint.is_empty() {
                contract.push(format!(
                    "frame-plan:{}",
                    record.derived_hashes.frame_plan_fingerprint
                ));
            }
        }
        "texture" => {
            contract.push("texture-package".to_string());
            contract.push(format!(
                "role-policy:{}",
                record.import_preset.texture_role_policy
            ));
        }
        "asset_graph" => {
            contract.push("asset-graph-package".to_string());
            contract.push("runtime-procedural".to_string());
            if !record.derived_hashes.material_hash.is_empty() {
                contract.push(format!(
                    "material-hash:{}",
                    record.derived_hashes.material_hash
                ));
            }
            if !record.derived_hashes.shader_variant_key.is_empty() {
                contract.push(format!(
                    "shader-variant:{}",
                    record.derived_hashes.shader_variant_key
                ));
            }
            if !record.derived_hashes.pipeline_cache_key.is_empty() {
                contract.push(format!(
                    "pipeline-cache:{}",
                    record.derived_hashes.pipeline_cache_key
                ));
            }
            if !record.derived_hashes.vertex_input_contract.is_empty() {
                contract.push(format!(
                    "vertex-input:{}",
                    record.derived_hashes.vertex_input_contract
                ));
            }
        }
        _ => contract.push("asset-package".to_string()),
    }
    let world_ready = world_ready_report_for(record);
    if !world_ready.report_hash.is_empty() {
        contract.push(format!("world-ready:{}", world_ready.report_hash));
    }
    contract
}

fn world_ready_report_for(record: &AssetDatabaseRecord) -> WorldReadyAssetReport {
    let mut readiness_signals = vec![
        "topology".to_string(),
        "uv".to_string(),
        "normal_tangent_health".to_string(),
        "pbr_correctness".to_string(),
        "collision_proxy".to_string(),
        "lod_impostor_continuity".to_string(),
        "interaction_affordance".to_string(),
        "wetness_propagation".to_string(),
        "dirt_accumulation".to_string(),
        "damage_semantics".to_string(),
        "ai_occlusion".to_string(),
        "nav_obstruction".to_string(),
        "ecology_compatibility".to_string(),
        "material_uniqueness".to_string(),
        "streaming_residency".to_string(),
        "perceptual_stability".to_string(),
    ];
    readiness_signals.sort();
    let mut diagnostics = Vec::new();
    if record_has_errors(record) {
        diagnostics.push("error: cook diagnostics prevent world-ready acceptance".to_string());
    }
    if record.outputs.is_empty() {
        diagnostics.push("error: no cooked runtime outputs for streaming residency".to_string());
    }
    if record.source_hash.is_empty() {
        diagnostics.push(
            "warning: missing source hash weakens topology and material uniqueness".to_string(),
        );
    }
    match record.kind.as_str() {
        "material" => {
            if record.derived_hashes.material_hash.is_empty() {
                diagnostics.push("warning: material hash missing for PBR correctness".to_string());
            }
            if record.derived_hashes.shader_variant_key.is_empty() {
                diagnostics.push(
                    "warning: shader variant key missing for perceptual stability".to_string(),
                );
            }
        }
        "scene" => {
            if record.derived_hashes.vertex_input_contract.is_empty() {
                diagnostics.push("warning: vertex input contract missing for topology".to_string());
            }
            if record.derived_hashes.frame_plan_fingerprint.is_empty() {
                diagnostics.push(
                    "warning: frame plan fingerprint missing for streaming residency".to_string(),
                );
            }
        }
        "asset_graph" => {
            if record.derived_hashes.material_hash.is_empty() {
                diagnostics.push(
                    "warning: graph material hash missing for material uniqueness".to_string(),
                );
            }
        }
        "texture" => {
            if record.import_preset.texture_role_policy.is_empty() {
                diagnostics
                    .push("warning: texture role policy missing for PBR correctness".to_string());
            }
        }
        _ => diagnostics.push(format!(
            "warning: world-ready policy uses generic checks for kind '{}'",
            record.kind
        )),
    }
    let accepted = !diagnostics
        .iter()
        .any(|diagnostic| diagnostic.starts_with("error:"));
    let report_hash = hash_serializable(
        "aster.asset.world-ready.v1",
        &(
            record.guid.as_str(),
            record.id.as_str(),
            record.kind.as_str(),
            record.source_hash.as_str(),
            record.derived_hashes.artifact_hash.as_str(),
            readiness_signals.as_slice(),
            diagnostics.as_slice(),
            accepted,
        ),
    );
    WorldReadyAssetReport {
        accepted,
        report_hash,
        readiness_signals,
        diagnostics,
    }
}

fn refresh_record_truth(record: &mut AssetDatabaseRecord) {
    if record.source.path.is_empty() {
        record.source.path = record.source_path.clone();
    }
    if record.source.hash.is_empty() {
        record.source.hash = record.source_hash.clone();
    }
    if record.artifacts.is_empty() {
        record.artifacts = record
            .outputs
            .iter()
            .map(|output| AssetArtifactRecord {
                role: output.role.clone(),
                kind: output.kind.clone(),
                path: output.path.clone(),
                hash: output.hash.clone(),
                reused: false,
            })
            .collect();
    }
    if record.dependency_edges.is_empty() {
        refresh_dependency_edges(record);
    }
    record.derived_hashes.source_hash = record.source_hash.clone();
    record.derived_hashes.options_hash = record.options_hash.clone();
    record.derived_hashes.dependency_hash =
        hash_serializable("aster.asset.dependencies.v2", &record.dependencies);
    record.derived_hashes.artifact_hash =
        hash_serializable("aster.asset.artifacts.v2", &record.artifacts);
    if record.kind == "material" && record.derived_hashes.material_hash.is_empty() {
        record.derived_hashes.material_hash = hash_hex_text(&format!(
            "material:{}:{}:{}",
            record.source_hash, record.derived_hashes.dependency_hash, record.options_hash
        ));
    }
    if record.kind == "texture" && record.derived_hashes.material_hash.is_empty() {
        record.derived_hashes.material_hash = hash_hex_text(&format!(
            "texture:{}:{}",
            record.source_hash, record.derived_hashes.artifact_hash
        ));
    }
    if record.kind == "asset_graph" && record.derived_hashes.material_hash.is_empty() {
        record.derived_hashes.material_hash = hash_hex_text(&format!(
            "assetgraph:{}:{}",
            record.source_hash, record.derived_hashes.artifact_hash
        ));
    }
    if record.kind == "scene" {
        if record.derived_hashes.vertex_input_contract.is_empty() {
            record.derived_hashes.vertex_input_contract = hash_hex_text(&format!(
                "vertex-input:{}:{}",
                record.source_hash, record.options_hash
            ));
        }
        if record.derived_hashes.frame_plan_fingerprint.is_empty() {
            record.derived_hashes.frame_plan_fingerprint = hash_hex_text(&format!(
                "frame-plan:{}:{}:{}",
                record.source_hash,
                record.derived_hashes.dependency_hash,
                record.derived_hashes.artifact_hash
            ));
        }
    }
    let mut chain = vec![format!("source:{}", record.source_path)];
    chain.extend(record.dependencies.iter().map(|dependency| {
        format!(
            "dependency:{}:{}:{}",
            dependency.role,
            dependency.path,
            if dependency.present {
                "present"
            } else {
                "missing"
            }
        )
    }));
    chain.extend(
        record
            .outputs
            .iter()
            .map(|output| format!("output:{}:{}:{}", output.role, output.kind, output.path)),
    );
    let world_ready = world_ready_report_for(record);
    record.fate_report = AssetFateReport {
        asset_id: record.id.clone(),
        asset_guid: record.guid.clone(),
        kind: record.kind.clone(),
        source_path: record.source_path.clone(),
        production_ready: !record_has_errors(record)
            && !record.outputs.is_empty()
            && world_ready.accepted,
        dependency_count: record.dependencies.len(),
        output_count: record.outputs.len(),
        diagnostic_count: record.diagnostics.len(),
        derived_hashes: record.derived_hashes.clone(),
        chain,
        render_contract: render_contract_for(record),
        artifact_provenance: record.artifacts.clone(),
        world_ready,
    };
}

pub fn refresh_asset_database_truth(database: &mut AssetDatabase) {
    database.schema_version = ASSET_DATABASE_SCHEMA_VERSION;
    if database.artifact_manifest.is_empty() {
        database.artifact_manifest = "asset-manifest.astermanifest.json".to_string();
    }
    for record in &mut database.records {
        refresh_record_truth(record);
    }
    database.fate_reports = database
        .records
        .iter()
        .map(|record| record.fate_report.clone())
        .collect();
    let nodes = database
        .records
        .iter()
        .map(|record| AssetGraphNode {
            guid: record.guid.clone(),
            id: record.id.clone(),
            kind: record.kind.clone(),
            source_path: record.source_path.clone(),
            production_ready: record.fate_report.production_ready,
        })
        .collect::<Vec<_>>();
    let edges = database
        .records
        .iter()
        .flat_map(|record| {
            record.dependency_edges.iter().map(|edge| AssetGraphEdge {
                from: if edge.from.is_empty() {
                    record.guid.clone()
                } else {
                    edge.from.clone()
                },
                to: edge.to.clone(),
                role: edge.role.clone(),
                present: edge.present,
                hash: edge.hash.clone(),
            })
        })
        .collect::<Vec<_>>();
    let project_fingerprint = hash_serializable(
        "aster.asset.graph.v2",
        &database
            .records
            .iter()
            .map(|record| {
                (
                    record.guid.as_str(),
                    record.source_hash.as_str(),
                    record.options_hash.as_str(),
                    record.derived_hashes.dependency_hash.as_str(),
                    record.derived_hashes.artifact_hash.as_str(),
                )
            })
            .collect::<Vec<_>>(),
    );
    database.asset_graph = AssetGraph {
        schema_version: ASSET_DATABASE_SCHEMA_VERSION,
        project_fingerprint,
        nodes,
        edges,
    };
}

fn clean_catalog_component(value: &str) -> String {
    value
        .trim()
        .chars()
        .map(|c| if c == ':' || c == '\\' { '-' } else { c })
        .collect()
}

fn clean_catalog_path(path: &str) -> String {
    let mut components = Vec::<String>::new();
    for component in path.split(['/', '\\']) {
        let component = clean_catalog_component(component);
        if component.is_empty() || component == "." {
            continue;
        }
        if component == ".." {
            components.pop();
        } else {
            components.push(component);
        }
    }
    components.join("/")
}

fn catalog_path_for_record(record: &AssetDatabaseRecord) -> String {
    let mut chars = record.kind.chars();
    let kind = match chars.next() {
        Some(first) => first.to_uppercase().collect::<String>() + chars.as_str(),
        None => "Asset".to_string(),
    };
    clean_catalog_path(&format!("Assets/{kind}"))
}

fn catalog_tags_for_record(record: &AssetDatabaseRecord) -> Vec<String> {
    let mut tags = Vec::new();
    if !record.kind.is_empty() {
        tags.push(record.kind.clone());
    }
    if !record.platform.is_empty() {
        tags.push(record.platform.clone());
    }
    if !record.import_preset.name.is_empty() {
        tags.push(format!("preset:{}", record.import_preset.name));
    }
    if record.fate_report.production_ready {
        tags.push("production-ready".to_string());
    }
    for output in &record.outputs {
        if !output.role.is_empty() {
            tags.push(format!("output:{}", output.role));
        }
    }
    tags.sort();
    tags.dedup();
    tags
}

pub fn stable_asset_catalog_id(path: &str) -> String {
    let clean = clean_catalog_path(path);
    let hash = hash_hex_text(&format!("aster.catalog.v1:{clean}"));
    format!("aster-catalog-{}", &hash[..16])
}

pub fn catalog_store_from_database(database: &AssetDatabase) -> AssetCatalogStore {
    let mut database = database.clone();
    refresh_asset_database_truth(&mut database);
    let mut by_path = BTreeMap::<String, AssetCatalogRecord>::new();
    for record in &database.records {
        let path = catalog_path_for_record(record);
        let catalog = by_path.entry(path.clone()).or_insert_with(|| {
            let simple_name = path.split('/').next_back().unwrap_or("Assets").to_string();
            let mut metadata = BTreeMap::new();
            metadata.insert("source".to_string(), "asset-database".to_string());
            metadata.insert("kind".to_string(), record.kind.clone());
            AssetCatalogRecord {
                id: stable_asset_catalog_id(&path),
                path: path.clone(),
                simple_name,
                tags: Vec::new(),
                metadata,
                deleted: false,
            }
        });
        catalog.tags.extend(catalog_tags_for_record(record));
        catalog.tags.sort();
        catalog.tags.dedup();
        let asset_count = catalog
            .metadata
            .get("asset_count")
            .and_then(|value| value.parse::<usize>().ok())
            .unwrap_or(0)
            + 1;
        catalog
            .metadata
            .insert("asset_count".to_string(), asset_count.to_string());
    }
    AssetCatalogStore {
        schema_version: 1,
        catalogs: by_path.into_values().collect(),
    }
}

pub fn asset_catalog_store_json(database: &AssetDatabase) -> Result<String> {
    Ok(serde_json::to_string_pretty(&catalog_store_from_database(
        database,
    ))?)
}

pub fn write_asset_catalog_store(
    database: &AssetDatabase,
    path: impl AsRef<Path>,
) -> Result<usize> {
    let store = catalog_store_from_database(database);
    let count = store.catalogs.len();
    write_json(path.as_ref(), &store)?;
    Ok(count)
}

fn write_artifact_manifest(path: &Path, database: &AssetDatabase) -> Result<()> {
    let assets = database
        .records
        .iter()
        .map(|record| {
            serde_json::json!({
                "guid": record.guid.clone(),
                "id": record.id.clone(),
                "kind": record.kind.clone(),
                "source": record.source.clone(),
                "import_preset": record.import_preset.clone(),
                "platform_profile": record.platform_profile.clone(),
                "dependency_edges": record.dependency_edges.clone(),
                "artifacts": record.artifacts.clone(),
                "diagnostics": record.diagnostics.clone(),
            })
        })
        .collect::<Vec<_>>();
    let manifest = serde_json::json!({
        "schema_version": ASSET_MANIFEST_SCHEMA_VERSION,
        "asset_database_schema_version": database.schema_version,
        "platform": database.platform.clone(),
        "tool_versions": database.tool_versions.clone(),
        "assets": assets,
    });
    write_json(path, &manifest)
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
            "unsupported Asset v2 platform '{platform}', expected desktop"
        )));
    }
    fs::create_dir_all(output)?;
    let project_bytes = fs::read(project)?;
    let root: Value = serde_json::from_slice(&project_bytes)?;
    let project_root = project.parent().unwrap_or_else(|| Path::new(""));
    let platform_profile = platform_profile_record(&root, platform);
    let mut records = Vec::new();
    if let Some(assets) = root.get("assets").and_then(Value::as_array) {
        for asset in assets {
            let id = asset.get("id").and_then(Value::as_str).unwrap_or("");
            let declared_kind = asset.get("kind").and_then(Value::as_str).unwrap_or("");
            let Some(source) = asset.get("path").and_then(Value::as_str) else {
                continue;
            };
            let source_path = project_root.join(source);
            let import_preset = import_preset_record(&root, asset);
            let stable_guid = value_str(asset, "guid")
                .map(str::to_string)
                .or(read_asset_meta_guid(&source_path)?);
            records.push(cook_asset(
                &source_path,
                id,
                stable_guid.as_deref(),
                declared_kind,
                project_root,
                output,
                platform,
                import_preset,
                platform_profile.clone(),
            )?);
        }
    }
    records.sort_by(|lhs, rhs| lhs.guid.cmp(&rhs.guid));
    let manifest_name = "asset-manifest.astermanifest.json".to_string();
    let mut database = AssetDatabase {
        schema_version: ASSET_DATABASE_SCHEMA_VERSION,
        platform: platform.to_string(),
        artifact_manifest: manifest_name.clone(),
        tool_versions: default_tool_versions(),
        asset_graph: AssetGraph::default(),
        fate_reports: Vec::new(),
        records,
    };
    refresh_asset_database_truth(&mut database);
    let database_path = output.join("assetdb.asterdb.json");
    write_json(&database_path, &database)?;
    let manifest_path = output.join(&manifest_name);
    write_artifact_manifest(&manifest_path, &database)?;
    let error_count = asset_database_error_count(&database);
    let warning_count = asset_database_warning_count(&database);
    Ok(CookProjectResult {
        database,
        database_path,
        manifest_path,
        error_count,
        warning_count,
    })
}

fn cave_vec3(value: &Value) -> Option<[f64; 3]> {
    let array = value.as_array()?;
    if array.len() != 3 {
        return None;
    }
    Some([array[0].as_f64()?, array[1].as_f64()?, array[2].as_f64()?])
}

fn cave_distance(lhs: [f64; 3], rhs: [f64; 3]) -> f64 {
    let dx = lhs[0] - rhs[0];
    let dy = lhs[1] - rhs[1];
    let dz = lhs[2] - rhs[2];
    (dx * dx + dy * dy + dz * dz).sqrt()
}

fn cave_volume_overlaps(lhs: &Value, rhs: &Value) -> bool {
    let Some(lhs_center) = lhs.get("center").and_then(cave_vec3) else {
        return false;
    };
    let Some(lhs_half) = lhs.get("half_extents").and_then(cave_vec3) else {
        return false;
    };
    let Some(rhs_center) = rhs.get("center").and_then(cave_vec3) else {
        return false;
    };
    let Some(rhs_half) = rhs.get("half_extents").and_then(cave_vec3) else {
        return false;
    };
    (lhs_center[0] - rhs_center[0]).abs() <= lhs_half[0] + rhs_half[0]
        && (lhs_center[1] - rhs_center[1]).abs() <= lhs_half[1] + rhs_half[1]
        && (lhs_center[2] - rhs_center[2]).abs() <= lhs_half[2] + rhs_half[2]
}

fn perceptual_continuity_channel_bit(channel: &str) -> u32 {
    match channel {
        "spatial_affordance" => 1 << 0,
        "motion_continuity" => 1 << 1,
        "hazard_readability" => 1 << 2,
        "material_memory" => 1 << 3,
        "lighting_atmosphere" => 1 << 4,
        "event_residue" => 1 << 5,
        "sensory_feedback" => 1 << 6,
        "ai_attention" => 1 << 7,
        "streaming_residency" => 1 << 8,
        "ui_feedback" => 1 << 9,
        "resource_state" => 1 << 10,
        _ => 0,
    }
}

fn perceptual_continuity_mask(value: Option<&Value>) -> u32 {
    value
        .and_then(Value::as_array)
        .map(|channels| {
            channels
                .iter()
                .filter_map(Value::as_str)
                .fold(0u32, |mask, channel| {
                    mask | perceptual_continuity_channel_bit(channel)
                })
        })
        .unwrap_or(0)
}

fn perceptual_continuity_score(required: u32, observed: u32) -> f64 {
    if required == 0 {
        return 1.0;
    }
    let covered = (required & observed).count_ones() as f64;
    covered / required.count_ones() as f64
}

fn coal_mining_reaction_observed_mask(action: &str) -> u32 {
    if action != "action.mine.coal_ore" {
        return 0;
    }
    perceptual_continuity_channel_bit("material_memory")
        | perceptual_continuity_channel_bit("event_residue")
        | perceptual_continuity_channel_bit("sensory_feedback")
        | perceptual_continuity_channel_bit("resource_state")
        | perceptual_continuity_channel_bit("ai_attention")
        | perceptual_continuity_channel_bit("ui_feedback")
}

fn perception_ledger_channel_bit(channel: &str) -> u32 {
    match channel {
        "material_memory" => 1 << 0,
        "contact_history" => 1 << 1,
        "lighting_exposure" => 1 << 2,
        "atmosphere_cell" => 1 << 3,
        "occlusion_role" => 1 << 4,
        "gameplay_affordance" => 1 << 5,
        "wear_continuity" => 1 << 6,
        "streaming_semantic_lod" => 1 << 7,
        "audio_visual_cue_budget" => 1 << 8,
        _ => 0,
    }
}

fn perception_ledger_mask(value: Option<&Value>) -> u32 {
    value
        .and_then(Value::as_array)
        .map(|channels| {
            channels
                .iter()
                .filter_map(Value::as_str)
                .fold(0u32, |mask, channel| {
                    mask | perception_ledger_channel_bit(channel)
                })
        })
        .unwrap_or(0)
}

fn perception_ledger_score(required: u32, observed: u32) -> f64 {
    if required == 0 {
        return 1.0;
    }
    let covered = (required & observed).count_ones() as f64;
    covered / required.count_ones() as f64
}

fn perceptual_causality_channel_bit(channel: &str) -> u32 {
    match channel {
        "material_memory" => 1 << 0,
        "contact_residue" | "contact_history" => 1 << 1,
        "light_history" | "lighting_exposure" => 1 << 2,
        "acoustic_surface" | "audio_visual_cue_budget" => 1 << 3,
        "traversal_affordance" | "traversal_pressure" => 1 << 4,
        "threat_cover" | "ai_attention" => 1 << 5,
        "semantic_lod" | "streaming_semantic_lod" => 1 << 6,
        "streaming_cost" | "streaming_residency" => 1 << 7,
        "player_readable_cause" | "gameplay_affordance" => 1 << 8,
        "neural_irradiance" => 1 << 9,
        _ => 0,
    }
}

fn perceptual_causality_mask(value: Option<&Value>) -> u32 {
    value
        .and_then(Value::as_array)
        .map(|channels| {
            channels
                .iter()
                .filter_map(Value::as_str)
                .fold(0u32, |mask, channel| {
                    mask | perceptual_causality_channel_bit(channel)
                })
        })
        .unwrap_or(0)
}

const CAVE_BELIEF_FINDING_KINDS: [&str; 9] = [
    "material_family_collapse",
    "contextual_grounding_failure",
    "contact_shadow_credibility_failure",
    "volumetric_scene_coupling_failure",
    "material_response_instability",
    "lod_transition_visibility",
    "asset_scale_incoherence",
    "environmental_entropy_deficit",
    "backend_visual_truth_gap",
];

fn cave_belief_finding(
    kind: &str,
    severity: &str,
    subject: &str,
    score: f64,
    threshold: f64,
    evidence_hash: &str,
    message: String,
) -> Value {
    serde_json::json!({
        "kind": kind,
        "severity": severity,
        "subject": subject,
        "score": score,
        "threshold": threshold,
        "evidence_hash": evidence_hash,
        "source": "assetc.cave_world_gate",
        "message": message,
    })
}

fn cave_world_gate_report(
    root: &Value,
    id: &str,
    guid: &str,
    source_rel: &str,
    source_hash: &str,
) -> (Value, Vec<AssetCookDiagnostic>) {
    let validation = root.get("validation").unwrap_or(&Value::Null);
    let mut reasons = Vec::<String>::new();
    let mut checked_steps = 0usize;
    let mut blocked_steps = 0usize;

    if let Some(routes) = validation.get("walkable_routes").and_then(Value::as_array) {
        for route in routes {
            let route_id = route.get("id").and_then(Value::as_str).unwrap_or("unnamed");
            let points = route.get("points").and_then(Value::as_array);
            let max_segment = route
                .get("max_segment_length")
                .and_then(Value::as_f64)
                .unwrap_or(1.5);
            let tolerance = route
                .get("support_tolerance")
                .and_then(Value::as_f64)
                .unwrap_or(0.25);
            let Some(points) = points else {
                reasons.push(format!("route '{route_id}' has no probe points"));
                blocked_steps += 1;
                continue;
            };
            if points.len() < 2 {
                reasons.push(format!(
                    "route '{route_id}' has fewer than two probe points"
                ));
                blocked_steps += 1;
                continue;
            }
            for pair in points.windows(2) {
                checked_steps += 1;
                let Some(from) = cave_vec3(&pair[0]) else {
                    reasons.push(format!("route '{route_id}' has an invalid start point"));
                    blocked_steps += 1;
                    continue;
                };
                let Some(to) = cave_vec3(&pair[1]) else {
                    reasons.push(format!("route '{route_id}' has an invalid end point"));
                    blocked_steps += 1;
                    continue;
                };
                if cave_distance(from, to) > max_segment + tolerance {
                    reasons.push(format!(
                        "route '{route_id}' exceeds deterministic probe step length"
                    ));
                    blocked_steps += 1;
                }
            }
        }
    } else {
        reasons.push("validation.walkable_routes is required for cave world gate".to_string());
        blocked_steps += 1;
    }

    let empty = Vec::new();
    let spawn_volumes = validation
        .get("spawn_volumes")
        .and_then(Value::as_array)
        .unwrap_or(&empty);
    let collision_volumes = validation
        .get("collision_volumes")
        .and_then(Value::as_array)
        .unwrap_or(&empty);
    for spawn in spawn_volumes {
        let spawn_id = spawn.get("id").and_then(Value::as_str).unwrap_or("unnamed");
        for collision in collision_volumes {
            if cave_volume_overlaps(spawn, collision) {
                let collision_id = collision
                    .get("id")
                    .and_then(Value::as_str)
                    .unwrap_or("unnamed");
                reasons.push(format!(
                    "spawn volume '{spawn_id}' overlaps collision volume '{collision_id}'"
                ));
                blocked_steps += 1;
            }
        }
    }

    let resource_capacity = root
        .get("sections")
        .and_then(Value::as_array)
        .map(|sections| {
            sections
                .iter()
                .filter_map(|section| {
                    section
                        .get("ore")
                        .and_then(|ore| ore.get("max_nodes"))
                        .and_then(Value::as_i64)
                })
                .filter(|count| *count > 0)
                .sum::<i64>()
        })
        .unwrap_or(0);
    let mut resource_valid = true;
    if let Some(resource_probes) = validation.get("resource_probes").and_then(Value::as_array) {
        for probe in resource_probes {
            let probe_id = probe.get("id").and_then(Value::as_str).unwrap_or("unnamed");
            let minimum = probe
                .get("minimum_count")
                .and_then(Value::as_i64)
                .unwrap_or(1);
            if resource_capacity < minimum {
                resource_valid = false;
                reasons.push(format!(
                    "resource probe '{probe_id}' requires {minimum} nodes but cave budgets {resource_capacity}"
                ));
            }
        }
    }

    let encounter_count = root
        .get("placements")
        .and_then(Value::as_array)
        .map(|placements| {
            placements
                .iter()
                .filter(|placement| {
                    placement
                        .get("kind")
                        .and_then(Value::as_str)
                        .map(|kind| kind.contains("enemy") || kind.contains("encounter"))
                        .unwrap_or(false)
                })
                .count()
        })
        .unwrap_or(0);
    let mut encounter_valid = true;
    let encounter_budget = (encounter_count as f64 * 0.25).clamp(0.0, 1.0);
    if let Some(encounter_probes) = validation.get("encounter_probes").and_then(Value::as_array) {
        for probe in encounter_probes {
            let probe_id = probe.get("id").and_then(Value::as_str).unwrap_or("unnamed");
            let minimum = probe
                .get("minimum_budget")
                .and_then(Value::as_f64)
                .unwrap_or(0.0);
            let maximum = probe
                .get("maximum_budget")
                .and_then(Value::as_f64)
                .unwrap_or(1.0);
            if encounter_budget < minimum || encounter_budget > maximum {
                encounter_valid = false;
                reasons.push(format!(
                    "encounter probe '{probe_id}' budget {encounter_budget:.2} is outside [{minimum:.2}, {maximum:.2}]"
                ));
            }
        }
    }

    let fixture_count = root
        .get("sections")
        .and_then(Value::as_array)
        .map(|sections| {
            sections
                .iter()
                .map(|section| {
                    section
                        .get("fixtures")
                        .and_then(Value::as_array)
                        .map(Vec::len)
                        .unwrap_or(0)
                })
                .sum::<usize>()
        })
        .unwrap_or(0);
    let route_count = validation
        .get("walkable_routes")
        .and_then(Value::as_array)
        .map(Vec::len)
        .unwrap_or(0);
    let salience_score = (0.34
        + fixture_count as f64 * 0.04
        + route_count as f64 * 0.10
        + resource_capacity as f64 * 0.006
        + encounter_count as f64 * 0.12)
        .clamp(0.0, 1.0);
    let minimum_salience = validation
        .get("perceptual_budget")
        .and_then(|value| value.get("minimum_salience"))
        .and_then(Value::as_f64)
        .unwrap_or(0.50);
    let perceptual_valid = salience_score >= minimum_salience;
    if !perceptual_valid {
        reasons.push(format!(
            "perceptual salience {salience_score:.2} is below minimum {minimum_salience:.2}"
        ));
    }

    let nav_valid = blocked_steps == 0;
    let section_count = root
        .get("sections")
        .and_then(Value::as_array)
        .map(Vec::len)
        .unwrap_or(0);
    let collision_count = collision_volumes.len();
    let ledger_budget = validation.get("perception_ledger");
    let ledger_required =
        perception_ledger_mask(ledger_budget.and_then(|value| value.get("required_channels")));
    let ledger_minimum = ledger_budget
        .and_then(|value| value.get("minimum_score"))
        .and_then(Value::as_f64)
        .unwrap_or(0.0);
    let mut ledger_observed = 0u32;
    if resource_capacity > 0 {
        ledger_observed |= perception_ledger_channel_bit("material_memory")
            | perception_ledger_channel_bit("wear_continuity");
    }
    if collision_count > 0 {
        ledger_observed |= perception_ledger_channel_bit("contact_history")
            | perception_ledger_channel_bit("occlusion_role");
    }
    if fixture_count > 0 {
        ledger_observed |= perception_ledger_channel_bit("lighting_exposure")
            | perception_ledger_channel_bit("audio_visual_cue_budget");
    }
    if section_count > 0 {
        ledger_observed |= perception_ledger_channel_bit("atmosphere_cell");
    }
    if resource_capacity > 0 || encounter_count > 0 {
        ledger_observed |= perception_ledger_channel_bit("gameplay_affordance")
            | perception_ledger_channel_bit("audio_visual_cue_budget");
    }
    if nav_valid && checked_steps > 0 {
        ledger_observed |= perception_ledger_channel_bit("streaming_semantic_lod");
    }
    let ledger_material_memory_hash = hash_hex_text(&format!(
        "{id}:ledger:material:{resource_capacity}:{source_hash}"
    ));
    let ledger_contact_history_hash = hash_hex_text(&format!(
        "{id}:ledger:contact:{collision_count}:{blocked_steps}"
    ));
    let ledger_lighting_exposure_hash =
        hash_hex_text(&format!("{id}:ledger:lighting:{fixture_count}"));
    let ledger_atmosphere_cell_hash = hash_hex_text(&format!(
        "{id}:ledger:atmosphere:{section_count}:{route_count}"
    ));
    let ledger_occlusion_role_hash = hash_hex_text(&format!(
        "{id}:ledger:occlusion:{collision_count}:{fixture_count}"
    ));
    let ledger_gameplay_affordance_hash = hash_hex_text(&format!(
        "{id}:ledger:affordance:{resource_capacity}:{encounter_count}"
    ));
    let ledger_wear_continuity_hash =
        hash_hex_text(&format!("{id}:ledger:wear:{resource_capacity}"));
    let ledger_streaming_semantic_lod_hash = hash_hex_text(&format!(
        "{id}:ledger:streaming:{checked_steps}:{blocked_steps}"
    ));
    let ledger_audio_visual_cue_budget_hash = hash_hex_text(&format!(
        "{id}:ledger:cue:{fixture_count}:{resource_capacity}:{encounter_count}"
    ));
    let mut ledger_cells = Vec::new();
    if let Some(cells) = ledger_budget
        .and_then(|value| value.get("cells"))
        .and_then(Value::as_array)
    {
        for cell in cells {
            let cell_id = cell.get("id").and_then(Value::as_str).unwrap_or("unnamed");
            let required = {
                let cell_required = perception_ledger_mask(cell.get("required_channels"));
                if cell_required == 0 {
                    ledger_required
                } else {
                    cell_required
                }
            };
            let minimum = cell
                .get("minimum_score")
                .and_then(Value::as_f64)
                .unwrap_or(ledger_minimum);
            let missing = required & !ledger_observed;
            let score = perception_ledger_score(required, ledger_observed);
            let accepted = missing == 0 && score + f64::EPSILON >= minimum;
            ledger_cells.push(serde_json::json!({
                "id": cell_id,
                "accepted": accepted,
                "required_channel_mask": required,
                "observed_channel_mask": ledger_observed,
                "missing_channel_mask": missing,
                "score": score,
                "minimum_score": minimum,
                "ledger_hash": hash_hex_text(&format!("{id}:ledger:cell:{cell_id}:{required}:{ledger_observed}:{missing}:{score:.3}:{minimum:.3}")),
            }));
        }
    }
    if ledger_cells.is_empty() {
        let cell_id = "runtime";
        let missing = ledger_required & !ledger_observed;
        let score = perception_ledger_score(ledger_required, ledger_observed);
        let accepted = missing == 0 && score + f64::EPSILON >= ledger_minimum;
        ledger_cells.push(serde_json::json!({
            "id": cell_id,
            "accepted": accepted,
            "required_channel_mask": ledger_required,
            "observed_channel_mask": ledger_observed,
            "missing_channel_mask": missing,
            "score": score,
            "minimum_score": ledger_minimum,
            "ledger_hash": hash_hex_text(&format!("{id}:ledger:cell:{cell_id}:{ledger_required}:{ledger_observed}:{missing}:{score:.3}:{ledger_minimum:.3}")),
        }));
    }
    let ledger_missing = ledger_required & !ledger_observed;
    let ledger_score = perception_ledger_score(ledger_required, ledger_observed);
    let ledger_cells_valid = ledger_cells.iter().all(|cell| {
        cell.get("accepted")
            .and_then(Value::as_bool)
            .unwrap_or(false)
    });
    let ledger_valid = ledger_cells_valid
        && (ledger_required == 0
            || (ledger_missing == 0 && ledger_score + f64::EPSILON >= ledger_minimum));
    if !ledger_valid {
        reasons.push(format!(
            "world perception ledger score {ledger_score:.2} is below minimum {ledger_minimum:.2}"
        ));
    }
    let ledger_hash = hash_hex_text(&format!(
        "{id}:ledger:{ledger_required}:{ledger_observed}:{ledger_missing}:{ledger_score:.3}:{ledger_minimum:.3}:{source_hash}"
    ));

    let continuity_budget = validation.get("perceptual_continuity_budget");
    let continuity_required = perceptual_continuity_mask(
        continuity_budget.and_then(|value| value.get("required_channels")),
    );
    let mut continuity_observed = 0u32;
    if nav_valid && route_count > 0 {
        continuity_observed |= perceptual_continuity_channel_bit("spatial_affordance");
    }
    if nav_valid && checked_steps > 1 {
        continuity_observed |= perceptual_continuity_channel_bit("motion_continuity");
    }
    if encounter_count > 0 {
        continuity_observed |= perceptual_continuity_channel_bit("hazard_readability")
            | perceptual_continuity_channel_bit("ai_attention");
    }
    if resource_capacity > 0 {
        continuity_observed |= perceptual_continuity_channel_bit("material_memory")
            | perceptual_continuity_channel_bit("event_residue")
            | perceptual_continuity_channel_bit("resource_state");
    }
    if fixture_count > 0 {
        continuity_observed |= perceptual_continuity_channel_bit("lighting_atmosphere");
    }
    if checked_steps > 0 {
        continuity_observed |= perceptual_continuity_channel_bit("streaming_residency");
    }
    if ledger_observed & perception_ledger_channel_bit("material_memory") != 0 {
        continuity_observed |= perceptual_continuity_channel_bit("material_memory")
            | perceptual_continuity_channel_bit("resource_state");
    }
    if ledger_observed & perception_ledger_channel_bit("contact_history") != 0
        || ledger_observed & perception_ledger_channel_bit("wear_continuity") != 0
    {
        continuity_observed |= perceptual_continuity_channel_bit("event_residue");
    }
    if ledger_observed & perception_ledger_channel_bit("lighting_exposure") != 0
        || ledger_observed & perception_ledger_channel_bit("atmosphere_cell") != 0
    {
        continuity_observed |= perceptual_continuity_channel_bit("lighting_atmosphere");
    }
    if ledger_observed & perception_ledger_channel_bit("gameplay_affordance") != 0
        || ledger_observed & perception_ledger_channel_bit("occlusion_role") != 0
    {
        continuity_observed |= perceptual_continuity_channel_bit("hazard_readability")
            | perceptual_continuity_channel_bit("ai_attention");
    }
    if ledger_observed & perception_ledger_channel_bit("streaming_semantic_lod") != 0 {
        continuity_observed |= perceptual_continuity_channel_bit("streaming_residency")
            | perceptual_continuity_channel_bit("motion_continuity");
    }
    if ledger_observed & perception_ledger_channel_bit("audio_visual_cue_budget") != 0 {
        continuity_observed |= perceptual_continuity_channel_bit("sensory_feedback");
    }

    let mut reaction_reports = Vec::new();
    let mut reaction_packages_valid = true;
    if let Some(packages) = continuity_budget
        .and_then(|value| value.get("reaction_packages"))
        .and_then(Value::as_array)
    {
        for package in packages {
            let package_id = package
                .get("id")
                .and_then(Value::as_str)
                .unwrap_or("unnamed");
            let action = package
                .get("action")
                .and_then(Value::as_str)
                .unwrap_or_default();
            let required = perceptual_continuity_mask(package.get("required_channels"));
            let observed = coal_mining_reaction_observed_mask(action);
            continuity_observed |= observed;
            let missing = required & !observed;
            let minimum = package
                .get("minimum_score")
                .and_then(Value::as_f64)
                .unwrap_or(0.0);
            let score = perceptual_continuity_score(required, observed);
            let accepted = missing == 0 && score + f64::EPSILON >= minimum;
            if !accepted {
                reaction_packages_valid = false;
                reasons.push(format!(
                    "reaction package '{package_id}' continuity score {score:.2} is below minimum {minimum:.2}"
                ));
            }
            reaction_reports.push(serde_json::json!({
                "id": package_id,
                "action": action,
                "accepted": accepted,
                "required_channel_mask": required,
                "observed_channel_mask": observed,
                "missing_channel_mask": missing,
                "continuity_score": score,
                "minimum_score": minimum,
                "report_hash": hash_hex_text(&format!("{id}:reaction:{package_id}:{action}:{required}:{observed}:{missing}:{score:.3}:{minimum:.3}")),
            }));
        }
    }
    let continuity_minimum = continuity_budget
        .and_then(|value| value.get("minimum_score"))
        .and_then(Value::as_f64)
        .unwrap_or(0.0);
    let continuity_score = perceptual_continuity_score(continuity_required, continuity_observed);
    let continuity_missing = continuity_required & !continuity_observed;
    let continuity_valid = (continuity_required == 0
        || (continuity_missing == 0 && continuity_score + f64::EPSILON >= continuity_minimum))
        && reaction_packages_valid;
    if !continuity_valid {
        reasons.push(format!(
            "perceptual continuity score {continuity_score:.2} is below minimum {continuity_minimum:.2}"
        ));
    }
    let runtime_budget = validation.get("perceptual_runtime");
    let runtime_id = runtime_budget
        .and_then(|value| value.get("id"))
        .and_then(Value::as_str)
        .unwrap_or("perceptual_world_runtime");
    let exposure_horizon_seconds = runtime_budget
        .and_then(|value| value.get("exposure_horizon_seconds"))
        .and_then(Value::as_f64)
        .unwrap_or(47.0)
        .max(0.001);
    let minimum_runtime_continuity = runtime_budget
        .and_then(|value| value.get("minimum_continuity_score"))
        .and_then(Value::as_f64)
        .unwrap_or(0.62)
        .clamp(0.0, 1.0);
    let minimum_occlusion_trust = runtime_budget
        .and_then(|value| value.get("minimum_occlusion_trust"))
        .and_then(Value::as_f64)
        .unwrap_or(0.45)
        .clamp(0.0, 1.0);
    let minimum_lighting_believability = runtime_budget
        .and_then(|value| value.get("minimum_lighting_believability"))
        .and_then(Value::as_f64)
        .unwrap_or(0.45)
        .clamp(0.0, 1.0);
    let minimum_player_readable_cause = runtime_budget
        .and_then(|value| value.get("minimum_player_readable_cause"))
        .and_then(Value::as_f64)
        .unwrap_or(0.45)
        .clamp(0.0, 1.0);
    let runtime_material_memory =
        if ledger_observed & perception_ledger_channel_bit("material_memory") != 0 {
            0.84
        } else {
            0.0
        };
    let runtime_interaction_residue =
        if ledger_observed & perception_ledger_channel_bit("contact_history") != 0
            || ledger_observed & perception_ledger_channel_bit("wear_continuity") != 0
            || continuity_observed & perceptual_continuity_channel_bit("event_residue") != 0
        {
            0.78
        } else {
            0.0
        };
    let runtime_traversal_pressure = (if nav_valid && checked_steps > 0 {
        0.62
    } else {
        0.0
    } + (route_count as f64 * 0.08).min(0.18)
        + if ledger_observed & perception_ledger_channel_bit("streaming_semantic_lod") != 0 {
            0.16
        } else {
            0.0
        })
    .clamp(0.0_f64, 1.0_f64);
    let runtime_lighting_believability =
        (if ledger_observed & perception_ledger_channel_bit("lighting_exposure") != 0 {
            0.38
        } else {
            0.0
        } + if ledger_observed & perception_ledger_channel_bit("atmosphere_cell") != 0 {
            0.24
        } else {
            0.0
        } + if fixture_count > 0 { 0.20 } else { 0.0 }
            + salience_score * 0.18)
            .clamp(0.0, 1.0);
    let runtime_occlusion_trust: f64 =
        (if ledger_observed & perception_ledger_channel_bit("occlusion_role") != 0 {
            0.44_f64
        } else {
            0.0_f64
        } + if collision_count > 0 {
            0.18_f64
        } else {
            0.0_f64
        } + if nav_valid { 0.18_f64 } else { 0.0_f64 }
            + if checked_steps > 0 { 0.12_f64 } else { 0.0_f64 })
        .clamp(0.0_f64, 1.0_f64);
    let runtime_ecology_signal: f64 = ((resource_capacity as f64 * 0.025).min(0.26)
        + (encounter_count as f64 * 0.18).min(0.28)
        + if ledger_observed & perception_ledger_channel_bit("gameplay_affordance") != 0 {
            0.28
        } else {
            0.0
        }
        + if reaction_packages_valid { 0.12 } else { 0.0 })
    .clamp(0.0_f64, 1.0_f64);
    let runtime_player_readable_cause: f64 =
        (if reaction_packages_valid {
            0.26_f64
        } else {
            0.0_f64
        } + if resource_capacity > 0 {
            0.18_f64
        } else {
            0.0_f64
        } + if fixture_count > 0 { 0.16_f64 } else { 0.0_f64 }
            + if encounter_count > 0 {
                0.16_f64
            } else {
                0.0_f64
            }
            + if continuity_observed & perceptual_continuity_channel_bit("sensory_feedback") != 0 {
                0.18_f64
            } else {
                0.0_f64
            })
        .clamp(0.0_f64, 1.0_f64);
    let runtime_continuity_score = ((runtime_material_memory
        + runtime_interaction_residue
        + runtime_traversal_pressure
        + runtime_lighting_believability
        + runtime_occlusion_trust
        + runtime_ecology_signal
        + runtime_player_readable_cause)
        / 7.0)
        .clamp(0.0, 1.0);
    let runtime_continuity_debt = (minimum_runtime_continuity - runtime_continuity_score)
        .max(0.0)
        .clamp(0.0, 1.0);
    let runtime_valid = runtime_continuity_score + f64::EPSILON >= minimum_runtime_continuity
        && runtime_occlusion_trust + f64::EPSILON >= minimum_occlusion_trust
        && runtime_lighting_believability + f64::EPSILON >= minimum_lighting_believability
        && runtime_player_readable_cause + f64::EPSILON >= minimum_player_readable_cause
        && nav_valid;
    if !runtime_valid {
        reasons.push(format!(
            "perceptual runtime score {runtime_continuity_score:.2} is below minimum {minimum_runtime_continuity:.2}"
        ));
    }
    let runtime_semantic_budget_hash = hash_hex_text(&format!(
        "{id}:perceptual-runtime:budget:{runtime_lighting_believability:.3}:{runtime_occlusion_trust:.3}:{runtime_player_readable_cause:.3}:{runtime_ecology_signal:.3}"
    ));
    let runtime_state_hash = hash_hex_text(&format!(
        "{id}:perceptual-runtime:{runtime_id}:{exposure_horizon_seconds:.3}:{runtime_continuity_score:.3}:{runtime_continuity_debt:.3}:{runtime_semantic_budget_hash}"
    ));
    let scheduler_memory_residue: f64 =
        (if ledger_observed & perception_ledger_channel_bit("contact_history") != 0 {
            0.16_f64
        } else {
            0.0_f64
        } + if ledger_observed & perception_ledger_channel_bit("wear_continuity") != 0 {
            0.16_f64
        } else {
            0.0_f64
        } + if continuity_observed & perceptual_continuity_channel_bit("event_residue") != 0 {
            0.22_f64
        } else {
            0.0_f64
        } + if continuity_observed & perceptual_continuity_channel_bit("sensory_feedback") != 0 {
            0.14_f64
        } else {
            0.0_f64
        } + runtime_interaction_residue * 0.32)
            .clamp(0.0_f64, 1.0_f64);
    let scheduler_threat_signal: f64 = (encounter_budget * 0.30
        + if ledger_observed & perception_ledger_channel_bit("occlusion_role") != 0 {
            0.20_f64
        } else {
            0.0_f64
        }
        + if continuity_observed & perceptual_continuity_channel_bit("ai_attention") != 0 {
            0.20_f64
        } else {
            0.0_f64
        }
        + runtime_occlusion_trust * 0.14
        + salience_score * 0.16)
        .clamp(0.0_f64, 1.0_f64);
    let scheduler_material_age: f64 =
        (if ledger_observed & perception_ledger_channel_bit("material_memory") != 0 {
            0.22_f64
        } else {
            0.0_f64
        } + if ledger_observed & perception_ledger_channel_bit("wear_continuity") != 0 {
            0.26_f64
        } else {
            0.0_f64
        } + if continuity_observed & perceptual_continuity_channel_bit("resource_state") != 0 {
            0.14_f64
        } else {
            0.0_f64
        } + runtime_material_memory * 0.26
            + (resource_capacity as f64 * 0.012).min(0.12))
        .clamp(0.0_f64, 1.0_f64);
    let scheduler_interaction_debt: f64 = ((1.0 - runtime_player_readable_cause) * 0.22
        + (1.0 - runtime_continuity_score) * 0.18
        + scheduler_memory_residue * 0.20
        + scheduler_threat_signal * 0.18
        + if reaction_packages_valid { 0.0 } else { 0.16 }
        + if nav_valid { 0.0 } else { 0.06 })
    .clamp(0.0_f64, 1.0_f64);
    let scheduler_perceptual_priority: f64 = (scheduler_threat_signal * 0.30
        + scheduler_interaction_debt * 0.24
        + scheduler_memory_residue * 0.18
        + salience_score * 0.16
        + (1.0 - runtime_continuity_debt) * 0.12)
        .clamp(0.0_f64, 1.0_f64);
    let scheduler_streaming_budget: f64 =
        (if ledger_observed & perception_ledger_channel_bit("streaming_semantic_lod") != 0 {
            0.32_f64
        } else {
            0.0_f64
        } + scheduler_perceptual_priority * 0.30
            + if nav_valid { 0.16 } else { 0.0 }
            + runtime_traversal_pressure * 0.22)
            .clamp(0.0_f64, 1.0_f64);
    let scheduler_belief_stability: f64 = (ledger_score * 0.20
        + runtime_continuity_score * 0.24
        + runtime_occlusion_trust * 0.14
        + runtime_lighting_believability * 0.14
        + runtime_player_readable_cause * 0.14
        + (1.0 - scheduler_interaction_debt) * 0.14)
        .clamp(0.0_f64, 1.0_f64);
    let scheduler_decision_impact: f64 = (scheduler_perceptual_priority * 0.28
        + scheduler_threat_signal * 0.18
        + scheduler_material_age * 0.16
        + scheduler_memory_residue * 0.16
        + runtime_player_readable_cause * 0.12
        + scheduler_streaming_budget * 0.10)
        .clamp(0.0_f64, 1.0_f64);
    let scheduler_minimum_belief_stability = validation
        .get("belief_contract")
        .and_then(|value| value.get("minimum_score"))
        .and_then(Value::as_f64)
        .unwrap_or(0.58)
        .clamp(0.0, 0.92);
    let scheduler_valid = scheduler_belief_stability + f64::EPSILON
        >= scheduler_minimum_belief_stability
        && scheduler_streaming_budget > 0.0
        && nav_valid;
    if !scheduler_valid {
        reasons.push(format!(
            "perceptual scheduler belief stability {scheduler_belief_stability:.2} is below minimum {scheduler_minimum_belief_stability:.2}"
        ));
    }
    let scheduler_memory_residue_hash = hash_hex_text(&format!(
        "{id}:scheduler:memory:{scheduler_memory_residue:.3}:{ledger_contact_history_hash}:{continuity_required}:{continuity_observed}"
    ));
    let scheduler_threat_signal_hash = hash_hex_text(&format!(
        "{id}:scheduler:threat:{scheduler_threat_signal:.3}:{encounter_count}:{encounter_budget:.3}:{ledger_occlusion_role_hash}"
    ));
    let scheduler_material_age_hash = hash_hex_text(&format!(
        "{id}:scheduler:material-age:{scheduler_material_age:.3}:{ledger_material_memory_hash}:{ledger_wear_continuity_hash}"
    ));
    let scheduler_interaction_debt_hash = hash_hex_text(&format!(
        "{id}:scheduler:debt:{scheduler_interaction_debt:.3}:{continuity_required}:{continuity_observed}:{continuity_missing}"
    ));
    let scheduler_perceptual_priority_hash = hash_hex_text(&format!(
        "{id}:scheduler:priority:{scheduler_perceptual_priority:.3}:{runtime_semantic_budget_hash}"
    ));
    let scheduler_streaming_budget_hash = hash_hex_text(&format!(
        "{id}:scheduler:streaming:{scheduler_streaming_budget:.3}:{ledger_streaming_semantic_lod_hash}"
    ));
    let scheduler_hash = hash_hex_text(&format!(
        "{id}:scheduler:{scheduler_memory_residue_hash}:{scheduler_threat_signal_hash}:{scheduler_material_age_hash}:{scheduler_interaction_debt_hash}:{scheduler_perceptual_priority_hash}:{scheduler_streaming_budget_hash}:{scheduler_belief_stability:.3}:{scheduler_decision_impact:.3}:{runtime_state_hash}"
    ));
    let region_id = hash_hex_text(&format!("{id}:{guid}:{source_hash}:region"));
    let probe_trace_hash = hash_hex_text(&format!(
        "{id}:{source_hash}:{checked_steps}:{blocked_steps}:{resource_capacity}:{encounter_count}:{salience_score:.3}:{continuity_score:.3}:{continuity_required}:{continuity_observed}:{ledger_hash}:{runtime_state_hash}:{scheduler_hash}"
    ));
    let nav_report_hash = hash_hex_text(&format!(
        "{id}:nav:{checked_steps}:{blocked_steps}:{}",
        reasons.join("|")
    ));
    let resource_probe_hash = hash_hex_text(&format!("{id}:resource:{resource_capacity}"));
    let encounter_budget_hash = hash_hex_text(&format!(
        "{id}:encounter:{encounter_count}:{encounter_budget:.3}"
    ));
    let perceptual_report_hash = hash_hex_text(&format!(
        "{id}:perceptual:{salience_score:.3}:{minimum_salience:.3}"
    ));
    let continuity_report_hash = hash_hex_text(&format!(
        "{id}:continuity:{continuity_required}:{continuity_observed}:{continuity_missing}:{continuity_score:.3}:{continuity_minimum:.3}"
    ));
    let causality_budget = validation.get("perceptual_causality_graph");
    let causality_id = causality_budget
        .and_then(|value| value.get("id"))
        .and_then(Value::as_str)
        .unwrap_or("perceptual_causality_graph");
    let causality_required_changed = perceptual_causality_mask(
        causality_budget.and_then(|value| value.get("required_causal_edges")),
    );
    let causality_required_decision = perceptual_causality_mask(
        causality_budget.and_then(|value| value.get("required_decision_channels")),
    );
    let causality_minimum_decision_impact = causality_budget
        .and_then(|value| value.get("minimum_decision_impact"))
        .and_then(Value::as_f64)
        .unwrap_or(0.50)
        .clamp(0.0, 1.0);
    let mut causality_changed = 0u32;
    if ledger_observed & perception_ledger_channel_bit("material_memory") != 0 {
        causality_changed |= perceptual_causality_channel_bit("material_memory");
    }
    if ledger_observed & perception_ledger_channel_bit("contact_history") != 0
        || continuity_observed & perceptual_continuity_channel_bit("event_residue") != 0
    {
        causality_changed |= perceptual_causality_channel_bit("contact_residue");
    }
    if ledger_observed & perception_ledger_channel_bit("lighting_exposure") != 0 {
        causality_changed |= perceptual_causality_channel_bit("light_history");
    }
    if ledger_observed & perception_ledger_channel_bit("audio_visual_cue_budget") != 0 {
        causality_changed |= perceptual_causality_channel_bit("acoustic_surface");
    }
    if nav_valid || ledger_observed & perception_ledger_channel_bit("streaming_semantic_lod") != 0 {
        causality_changed |= perceptual_causality_channel_bit("traversal_affordance")
            | perceptual_causality_channel_bit("semantic_lod")
            | perceptual_causality_channel_bit("streaming_cost");
    }
    if ledger_observed & perception_ledger_channel_bit("occlusion_role") != 0
        || continuity_observed & perceptual_continuity_channel_bit("ai_attention") != 0
    {
        causality_changed |= perceptual_causality_channel_bit("threat_cover");
    }
    if ledger_observed & perception_ledger_channel_bit("gameplay_affordance") != 0
        || runtime_player_readable_cause > 0.0
    {
        causality_changed |= perceptual_causality_channel_bit("player_readable_cause");
    }
    if fixture_count > 0 {
        causality_changed |= perceptual_causality_channel_bit("neural_irradiance");
    }
    let causality_decision = causality_changed
        & (perceptual_causality_channel_bit("material_memory")
            | perceptual_causality_channel_bit("light_history")
            | perceptual_causality_channel_bit("acoustic_surface")
            | perceptual_causality_channel_bit("threat_cover")
            | perceptual_causality_channel_bit("traversal_affordance")
            | perceptual_causality_channel_bit("player_readable_cause"));
    let causality_primitive_count = resource_capacity.max(0) as u64
        + encounter_count as u64
        + fixture_count as u64
        + checked_steps.max(1) as u64;
    let causality_decision_impact = scheduler_decision_impact
        .max(runtime_player_readable_cause * 0.72)
        .clamp(0.0, 1.0);
    let causality_valid = causality_primitive_count > 0
        && (causality_required_changed == 0
            || (causality_changed & causality_required_changed) == causality_required_changed)
        && (causality_required_decision == 0
            || (causality_decision & causality_required_decision) == causality_required_decision)
        && causality_decision_impact + f64::EPSILON >= causality_minimum_decision_impact;
    if !causality_valid {
        reasons.push(format!(
            "perceptual causality graph decision impact {causality_decision_impact:.2} is below minimum {causality_minimum_decision_impact:.2}"
        ));
    }
    let causality_graph_hash = hash_hex_text(&format!(
        "{id}:causality:{causality_id}:{probe_trace_hash}:{causality_primitive_count}:{causality_changed}:{causality_decision}:{causality_decision_impact:.3}"
    ));
    let belief_budget = validation.get("belief_contract");
    let belief_id = belief_budget
        .and_then(|value| value.get("id"))
        .and_then(Value::as_str)
        .unwrap_or("default_belief_contract");
    let belief_minimum_score = belief_budget
        .and_then(|value| value.get("minimum_score"))
        .and_then(Value::as_f64)
        .unwrap_or(0.70)
        .clamp(0.0, 1.0);
    let belief_required_checks = belief_budget
        .and_then(|value| value.get("required_checks"))
        .and_then(Value::as_array)
        .map(|checks| {
            checks
                .iter()
                .filter_map(Value::as_str)
                .filter(|check| CAVE_BELIEF_FINDING_KINDS.contains(check))
                .map(str::to_string)
                .collect::<Vec<_>>()
        })
        .filter(|checks| !checks.is_empty())
        .unwrap_or_else(|| {
            CAVE_BELIEF_FINDING_KINDS
                .iter()
                .map(|check| (*check).to_string())
                .collect()
        });
    let material_family_score: f64 = if resource_capacity > 0 || fixture_count > 1 {
        0.76
    } else if fixture_count > 0 {
        0.42
    } else {
        0.22
    };
    let contextual_grounding_raw: f64 = if nav_valid { 0.34 } else { 0.08 }
        + if collision_count > 0 { 0.28 } else { 0.0 }
        + if fixture_count > 0 { 0.14 } else { 0.0 }
        + if runtime_interaction_residue > 0.0 {
            0.10
        } else {
            0.0
        };
    let contextual_grounding_score = contextual_grounding_raw.clamp(0.0, 1.0);
    let contact_shadow_score = (if collision_count > 0 { 0.46 } else { 0.10 }
        + runtime_occlusion_trust * 0.42
        + if ledger_observed & perception_ledger_channel_bit("contact_history") != 0 {
            0.14
        } else {
            0.0
        })
    .clamp(0.0_f64, 1.0_f64);
    let volumetric_scene_score = (runtime_lighting_believability * 0.66
        + if section_count > 0 { 0.18 } else { 0.0 }
        + if fixture_count > 0 { 0.16 } else { 0.0 })
    .clamp(0.0_f64, 1.0_f64);
    let material_response_score = (ledger_score * 0.56
        + runtime_material_memory * 0.30
        + if resource_capacity > 0 { 0.14 } else { 0.0 })
    .clamp(0.0_f64, 1.0_f64);
    let lod_transition_raw: f64 =
        if nav_valid && checked_steps > 0 {
            0.64
        } else {
            0.18
        } + if ledger_observed & perception_ledger_channel_bit("streaming_semantic_lod") != 0 {
            0.18
        } else {
            0.0
        } + if continuity_observed & perceptual_continuity_channel_bit("streaming_residency") != 0 {
            0.12
        } else {
            0.0
        };
    let lod_transition_score = lod_transition_raw.clamp(0.0, 1.0);
    let asset_scale_raw: f64 = if nav_valid { 0.44 } else { 0.12 }
        + if checked_steps > 0 { 0.18 } else { 0.0 }
        + if blocked_steps == 0 { 0.18 } else { 0.0 }
        + if collision_count > 0 { 0.08 } else { 0.0 };
    let asset_scale_score = asset_scale_raw.clamp(0.0, 1.0);
    let environmental_entropy_score = (if section_count > 0 { 0.14 } else { 0.0 }
        + (fixture_count.min(4) as f64 / 4.0) * 0.20
        + ((resource_capacity.max(0).min(8) as f64) / 8.0) * 0.20
        + (encounter_count.min(2) as f64 / 2.0) * 0.16
        + if runtime_ecology_signal >= 0.40 {
            0.24
        } else {
            runtime_ecology_signal * 0.50
        })
    .clamp(0.0, 1.0);
    let backend_visual_truth_score = 1.0_f64;
    let mut belief_scores = Vec::<f64>::new();
    let mut belief_findings = Vec::<Value>::new();
    {
        let mut record_belief_check =
            |kind: &str, score: f64, threshold: f64, evidence_hash: &str, message: String| {
                belief_scores.push(score);
                if score + f64::EPSILON >= threshold
                    || !belief_required_checks.iter().any(|check| check == kind)
                {
                    return;
                }
                let severity = if score < threshold * 0.5 {
                    "error"
                } else {
                    "warning"
                };
                belief_findings.push(cave_belief_finding(
                    kind,
                    severity,
                    id,
                    score,
                    threshold,
                    evidence_hash,
                    message,
                ));
            };
        record_belief_check(
            "material_family_collapse",
            material_family_score,
            0.34,
            &ledger_material_memory_hash,
            format!(
                "material family evidence score {material_family_score:.2} is below threshold 0.34"
            ),
        );
        record_belief_check(
            "contextual_grounding_failure",
            contextual_grounding_score,
            0.62,
            &nav_report_hash,
            format!(
                "contextual grounding score {contextual_grounding_score:.2} is below threshold 0.62"
            ),
        );
        record_belief_check(
            "contact_shadow_credibility_failure",
            contact_shadow_score,
            0.64,
            &ledger_contact_history_hash,
            format!(
                "contact shadow credibility score {contact_shadow_score:.2} is below threshold 0.64"
            ),
        );
        record_belief_check(
            "volumetric_scene_coupling_failure",
            volumetric_scene_score,
            0.58,
            &runtime_semantic_budget_hash,
            format!(
                "volumetric scene coupling score {volumetric_scene_score:.2} is below threshold 0.58"
            ),
        );
        record_belief_check(
            "material_response_instability",
            material_response_score,
            0.62,
            &ledger_hash,
            format!(
                "material response stability score {material_response_score:.2} is below threshold 0.62"
            ),
        );
        record_belief_check(
            "lod_transition_visibility",
            lod_transition_score,
            0.70,
            &ledger_streaming_semantic_lod_hash,
            format!(
                "LOD transition invisibility score {lod_transition_score:.2} is below threshold 0.70"
            ),
        );
        record_belief_check(
            "asset_scale_incoherence",
            asset_scale_score,
            0.70,
            &probe_trace_hash,
            format!("asset scale coherence score {asset_scale_score:.2} is below threshold 0.70"),
        );
        record_belief_check(
            "environmental_entropy_deficit",
            environmental_entropy_score,
            0.58,
            &perceptual_report_hash,
            format!(
                "environmental entropy score {environmental_entropy_score:.2} is below threshold 0.58"
            ),
        );
        record_belief_check(
            "backend_visual_truth_gap",
            backend_visual_truth_score,
            0.74,
            &scheduler_hash,
            format!(
                "backend visual truth score {backend_visual_truth_score:.2} is below threshold 0.74"
            ),
        );
    }
    let belief_score = if belief_scores.is_empty() {
        1.0
    } else {
        (belief_scores.iter().sum::<f64>() / belief_scores.len() as f64).clamp(0.0, 1.0)
    };
    let belief_finding_keys = belief_findings
        .iter()
        .map(|finding| {
            finding
                .get("kind")
                .and_then(Value::as_str)
                .unwrap_or("unknown")
                .to_string()
        })
        .collect::<Vec<_>>()
        .join("|");
    let belief_contract_hash = hash_hex_text(&format!(
        "{id}:belief:{belief_id}:{belief_minimum_score:.3}:{belief_score:.3}:{}:{belief_finding_keys}:{source_hash}:{ledger_hash}:{continuity_report_hash}:{runtime_state_hash}",
        belief_required_checks.join("|")
    ));
    let readability_audit_hash = hash_hex_text(&format!(
        "{id}:belief-readability:{material_family_score:.3}:{contextual_grounding_score:.3}:{contact_shadow_score:.3}:{volumetric_scene_score:.3}:{material_response_score:.3}:{lod_transition_score:.3}:{asset_scale_score:.3}:{environmental_entropy_score:.3}"
    ));
    let belief_valid =
        belief_findings.is_empty() && belief_score + f64::EPSILON >= belief_minimum_score;
    if !belief_valid {
        reasons.push(format!(
            "belief contract score {belief_score:.2} is below minimum {belief_minimum_score:.2}"
        ));
    }
    let world_transition_hash = hash_hex_text(&format!(
        "{id}:world-transition:{probe_trace_hash}:{ledger_hash}:{runtime_state_hash}:{scheduler_hash}:{causality_graph_hash}:{belief_contract_hash}"
    ));
    let extraction_hash = hash_hex_text(&format!(
        "{id}:render-extraction:{probe_trace_hash}:{runtime_state_hash}:{scheduler_hash}:{causality_graph_hash}:{belief_contract_hash}:{readability_audit_hash}"
    ));
    let verdict = nav_valid
        && resource_valid
        && encounter_valid
        && perceptual_valid
        && continuity_valid
        && ledger_valid
        && runtime_valid
        && scheduler_valid
        && causality_valid
        && belief_valid
        && checked_steps > 0;
    let diagnostic = if verdict {
        "accepted".to_string()
    } else {
        reasons.join("; ")
    };
    let torch_socket_count = root
        .get("required_assets")
        .and_then(Value::as_array)
        .map(|assets| {
            assets
                .iter()
                .filter_map(Value::as_str)
                .filter(|asset| asset.contains("torch_socket"))
                .count()
        })
        .unwrap_or(0)
        + fixture_count;
    let neural_light_field_hash = hash_hex_text(&format!(
        "{id}:neural-light-field:{fixture_count}:{torch_socket_count}:{ledger_lighting_exposure_hash}:{runtime_state_hash}:{scheduler_hash}:{probe_trace_hash}"
    ));
    let neural_light_field = serde_json::json!({
        "schema_version": 1,
        "kind": "neural_light_field_report",
        "model": "aster.deterministic_mlp_feature_grid.v1",
        "accepted": verdict && fixture_count > 0 && ledger_lighting_exposure_hash.len() >= 16,
        "model_hash": neural_light_field_hash,
        "fixture_count": fixture_count,
        "torch_socket_count": torch_socket_count,
        "input_channels": [
            "cell_position",
            "normal",
            "torch_intensity",
            "fixture_intensity",
            "exposure_age",
            "wetness",
            "material_memory",
            "occlusion_trust",
            "semantic_lod"
        ],
        "outputs": [
            "diffuse_irradiance_rgb",
            "confidence"
        ],
        "linked_world_gate_hash": probe_trace_hash,
        "linked_ledger_hash": ledger_hash,
        "linked_runtime_state_hash": runtime_state_hash,
        "linked_scheduler_hash": scheduler_hash,
        "backend_native_support": "future-gated",
        "cpu_inference": true,
        "deterministic_single_step": true,
    });
    let mut diagnostics = Vec::new();
    if !verdict {
        diagnostics.push(cook_error(format!(
            "cave world gate rejected: {diagnostic}"
        )));
    }
    let report = serde_json::json!({
        "schema_version": 1,
        "kind": "cave_world_gate_report",
        "id": id,
        "guid": guid,
        "source_path": source_rel,
        "source_hash": source_hash,
        "region_id": region_id,
        "probe_trace_hash": probe_trace_hash,
        "world_transition_hash": world_transition_hash,
        "extraction_hash": extraction_hash,
        "belief_contract_hash": belief_contract_hash,
        "verdict": if verdict { "accepted" } else { "quarantined" },
        "neural_light_field": neural_light_field.clone(),
        "navigation": {
            "valid": nav_valid,
            "checked_steps": checked_steps,
            "blocked_steps": blocked_steps,
            "report_hash": nav_report_hash,
            "diagnostic": if nav_valid { "valid" } else { diagnostic.as_str() },
        },
        "resource_probe": {
            "valid": resource_valid,
            "capacity": resource_capacity,
            "report_hash": resource_probe_hash,
        },
        "encounter_budget": {
            "valid": encounter_valid,
            "budget": encounter_budget,
            "encounter_count": encounter_count,
            "report_hash": encounter_budget_hash,
        },
        "perceptual_budget": {
            "accepted": perceptual_valid,
            "salience_score": salience_score,
            "minimum_salience": minimum_salience,
            "report_hash": perceptual_report_hash,
        },
        "perceptual_continuity_budget": {
            "accepted": continuity_valid,
            "required_channel_mask": continuity_required,
            "observed_channel_mask": continuity_observed,
            "missing_channel_mask": continuity_missing,
            "continuity_score": continuity_score,
            "minimum_score": continuity_minimum,
            "reaction_package_hash": continuity_report_hash,
            "material_memory_hash": hash_hex_text(&format!("{id}:continuity:material:{resource_capacity}")),
            "lighting_atmosphere_hash": hash_hex_text(&format!("{id}:continuity:lighting:{fixture_count}")),
            "ai_attention_hash": hash_hex_text(&format!("{id}:continuity:ai:{encounter_count}")),
            "streaming_residency_lod_hash": hash_hex_text(&format!("{id}:continuity:streaming:{checked_steps}:{blocked_steps}")),
            "resource_state_hash": resource_probe_hash,
            "event_residue_hash": hash_hex_text(&format!("{id}:continuity:residue:{resource_capacity}:{encounter_count}")),
            "readability_audit_hash": perceptual_report_hash,
            "report_hash": continuity_report_hash,
            "reaction_packages": reaction_reports,
        },
        "perception_ledger": {
            "accepted": ledger_valid,
            "required_channel_mask": ledger_required,
            "observed_channel_mask": ledger_observed,
            "missing_channel_mask": ledger_missing,
            "score": ledger_score,
            "minimum_score": ledger_minimum,
            "ledger_hash": ledger_hash,
            "cell_count": ledger_cells.len(),
            "material_memory_hash": ledger_material_memory_hash,
            "contact_history_hash": ledger_contact_history_hash,
            "lighting_exposure_hash": ledger_lighting_exposure_hash,
            "atmosphere_cell_hash": ledger_atmosphere_cell_hash,
            "occlusion_role_hash": ledger_occlusion_role_hash,
            "gameplay_affordance_hash": ledger_gameplay_affordance_hash,
            "wear_continuity_hash": ledger_wear_continuity_hash,
            "streaming_semantic_lod_hash": ledger_streaming_semantic_lod_hash,
            "audio_visual_cue_budget_hash": ledger_audio_visual_cue_budget_hash,
            "cells": ledger_cells,
        },
        "perceptual_runtime": {
            "id": runtime_id,
            "accepted": runtime_valid,
            "exposure_horizon_seconds": exposure_horizon_seconds,
            "minimum_continuity_score": minimum_runtime_continuity,
            "minimum_occlusion_trust": minimum_occlusion_trust,
            "minimum_lighting_believability": minimum_lighting_believability,
            "minimum_player_readable_cause": minimum_player_readable_cause,
            "perceptual_state_hash": runtime_state_hash,
            "continuity_debt": runtime_continuity_debt,
            "material_memory": runtime_material_memory,
            "interaction_residue": runtime_interaction_residue,
            "traversal_pressure": runtime_traversal_pressure,
            "lighting_believability": runtime_lighting_believability,
            "occlusion_trust": runtime_occlusion_trust,
            "ecology_signal": runtime_ecology_signal,
            "player_readable_cause": runtime_player_readable_cause,
            "semantic_budget_hash": runtime_semantic_budget_hash,
            "continuity_score": runtime_continuity_score,
        },
        "perceptual_causality_graph": {
            "id": causality_id,
            "accepted": causality_valid,
            "graph_hash": causality_graph_hash,
            "source_world_transition_hash": probe_trace_hash,
            "primitive_count": causality_primitive_count,
            "changed_channel_mask": causality_changed,
            "decision_channel_mask": causality_decision,
            "required_changed_channel_mask": causality_required_changed,
            "required_decision_channel_mask": causality_required_decision,
            "decision_impact_score": causality_decision_impact,
            "minimum_decision_impact": causality_minimum_decision_impact,
            "diagnostic": if causality_valid { "perceptual causality graph accepted" } else { "perceptual causality graph missing required causal decision evidence" },
        },
        "perceptual_world_scheduler": {
            "schema_version": 1,
            "kind": "perceptual_world_scheduler_report",
            "accepted": scheduler_valid,
            "scheduler_hash": scheduler_hash,
            "memory_residue_hash": scheduler_memory_residue_hash,
            "threat_signal_hash": scheduler_threat_signal_hash,
            "material_age_hash": scheduler_material_age_hash,
            "interaction_debt_hash": scheduler_interaction_debt_hash,
            "perceptual_priority_hash": scheduler_perceptual_priority_hash,
            "streaming_budget_hash": scheduler_streaming_budget_hash,
            "memory_residue": scheduler_memory_residue,
            "threat_signal": scheduler_threat_signal,
            "material_age": scheduler_material_age,
            "interaction_debt": scheduler_interaction_debt,
            "perceptual_priority": scheduler_perceptual_priority,
            "streaming_budget": scheduler_streaming_budget,
            "belief_stability": scheduler_belief_stability,
            "decision_impact_score": scheduler_decision_impact,
            "minimum_belief_stability": scheduler_minimum_belief_stability,
            "diagnostic": if scheduler_valid { "perceptual world scheduler accepted" } else { "perceptual world scheduler reports degraded belief stability" },
        },
        "decision_impact": {
            "score": scheduler_decision_impact,
            "threat_signal": scheduler_threat_signal,
            "material_age": scheduler_material_age,
            "interaction_debt": scheduler_interaction_debt,
            "streaming_budget": scheduler_streaming_budget,
            "report_hash": scheduler_hash,
        },
        "belief_contract": {
            "schema_version": 1,
            "kind": "belief_contract_report",
            "id": belief_id,
            "accepted": belief_valid,
            "score": belief_score,
            "minimum_score": belief_minimum_score,
            "belief_contract_hash": belief_contract_hash,
            "readability_audit_hash": readability_audit_hash,
            "required_checks": belief_required_checks,
            "findings": belief_findings.clone(),
        },
        "falseness_report": {
            "schema_version": 1,
            "kind": "belief_falseness_report",
            "world_transition_hash": world_transition_hash,
            "extraction_hash": extraction_hash,
            "belief_contract_hash": belief_contract_hash,
            "readability_audit_hash": readability_audit_hash,
            "perceptual_scheduler_hash": scheduler_hash,
            "decision_impact_score": scheduler_decision_impact,
            "accepted": belief_valid,
            "score": belief_score,
            "minimum_score": belief_minimum_score,
            "backend_visual_truth": {
                "required": false,
                "score": backend_visual_truth_score,
                "hdr_equivalent": true,
                "msaa_equivalent": true,
                "timestamp_equivalent": true,
                "swapchain_equivalent": true,
                "fog_probe_shadow_equivalent": true,
            },
            "findings": belief_findings,
        },
        "diagnostic": diagnostic,
    });
    (report, diagnostics)
}

pub fn cook_asset(
    source: impl AsRef<Path>,
    id: &str,
    guid: Option<&str>,
    declared_kind: &str,
    project_root: impl AsRef<Path>,
    output_root: impl AsRef<Path>,
    platform: &str,
    import_preset: AssetImportPresetRecord,
    platform_profile: AssetPlatformProfileRecord,
) -> Result<AssetDatabaseRecord> {
    let source = source.as_ref();
    let project_root = project_root.as_ref();
    let output_root = output_root.as_ref();
    let source_rel = relative_path_string(source, project_root);
    let kind = canonical_asset_kind(source, declared_kind);
    let fallback_guid = generate_stable_asset_guid(&kind, id);
    let mut record = base_asset_record(
        id,
        guid.unwrap_or(&fallback_guid),
        &kind,
        &source_rel,
        platform,
        import_preset.clone(),
        platform_profile,
    );
    if guid.is_none() {
        record.diagnostics.push(cook_error(format!(
            "asset '{}' is missing a stable V2 guid; add a project asset guid, create '{}', or run `aster_assetc guid-init --project <file.asterproj>`",
            id,
            source.with_extension("astermeta").display()
        )));
        return Ok(record);
    }
    if !source.exists() {
        record.diagnostics.push(cook_error(format!(
            "asset source is missing: {}",
            source.display()
        )));
        return Ok(record);
    }
    let source_bytes = fs::read(source)?;
    record.source_hash = hash_hex_bytes(&source_bytes);
    sync_record_source(&mut record);

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
                compile_options_from_preset(&import_preset),
            ) {
                Ok(asset) => {
                    record.source_hash = hex_hash(&asset.metadata.source_hash);
                    record.options_hash = hex_hash(&asset.metadata.options_hash);
                    sync_record_source(&mut record);
                    for material in &asset.materials {
                        for dependency in &material.texture_dependencies {
                            record.dependencies.push(AssetDependencyRecord {
                                role: dependency.role.clone(),
                                path: dependency.uri.clone(),
                                present: dependency.present,
                                hash: hex_hash(&dependency.hash),
                            });
                            if !dependency.present {
                                record.diagnostics.push(cook_warning(format!(
                                    "scene material '{}' references missing texture '{}'",
                                    material.name, dependency.uri
                                )));
                            }
                        }
                    }
                    push_output(
                        &mut record,
                        AssetCookedOutput {
                            role: "runtime-cache".to_string(),
                            kind: "astercache".to_string(),
                            path: relative_path_string(&cache_path, output_root),
                            hash: hash_file_hex(&cache_path)?,
                        },
                        false,
                    );
                    let report_path = output_root
                        .join("reports")
                        .join(format!("{}.report.json", safe_stem(id, source)));
                    let report = serde_json::json!({
                        "schema_version": 2,
                        "kind": "scene",
                        "id": id,
                        "guid": record.guid.clone(),
                        "source_path": source_rel,
                        "source_hash": record.source_hash,
                        "options_hash": record.options_hash,
                        "import_preset": record.import_preset.clone(),
                        "materials": asset.metadata.material_count,
                        "meshes": asset.metadata.mesh_count,
                        "collision_meshes": asset.metadata.collision_mesh_count,
                        "vertices": asset.metadata.total_vertices,
                        "indices": asset.metadata.total_indices,
                        "diagnostics": record.diagnostics,
                    });
                    write_json(&report_path, &report)?;
                    push_output(
                        &mut record,
                        AssetCookedOutput {
                            role: "report".to_string(),
                            kind: "json".to_string(),
                            path: relative_path_string(&report_path, output_root),
                            hash: hash_file_hex(&report_path)?,
                        },
                        false,
                    );
                }
                Err(error) => {
                    let source_scene = serde_json::from_slice::<Value>(&source_bytes)
                        .ok()
                        .is_some_and(|value| {
                            value.get("entities").and_then(Value::as_array).is_some()
                        });
                    if source_scene {
                        record.diagnostics.push(cook_warning(format!(
                            "source-level scene '{}' emitted authoring report; runtime cache compiler skipped: {}",
                            id, error
                        )));
                        let report_path = output_root.join("reports").join(format!(
                            "{}.source-scene.report.json",
                            safe_stem(id, source)
                        ));
                        let report = serde_json::json!({
                            "schema_version": 2,
                            "kind": "source_scene",
                            "id": id,
                            "guid": record.guid.clone(),
                            "source_path": source_rel,
                            "source_hash": record.source_hash,
                            "options_hash": record.options_hash,
                            "diagnostics": record.diagnostics,
                        });
                        write_json(&report_path, &report)?;
                        push_output(
                            &mut record,
                            AssetCookedOutput {
                                role: "source-scene-report".to_string(),
                                kind: "json".to_string(),
                                path: relative_path_string(&report_path, output_root),
                                hash: hash_file_hex(&report_path)?,
                            },
                            false,
                        );
                    } else {
                        record.diagnostics.push(cook_error(error.to_string()));
                    }
                }
            }
        }
        "material" if source.extension().and_then(|v| v.to_str()) == Some("astermat") => {
            let cooked = cook_material_asset(
                source,
                project_root,
                output_root,
                id,
                platform,
                Some(record.guid.as_str()),
            )?;
            record.dependencies = cooked.material_bin.dependency_hashes.clone();
            record.diagnostics = cooked.material_bin.diagnostics.clone();
            record.options_hash = hash_hex_text(&format!(
                "materialbin:{}:{}",
                MATERIAL_BIN_SCHEMA_VERSION, platform
            ));
            record.derived_hashes = cooked.material_bin.derived_hashes.clone();
            record.derived_hashes.source_hash = record.source_hash.clone();
            record.derived_hashes.options_hash = record.options_hash.clone();
            if cooked.emitted_runtime_outputs {
                if let Some(material_bin_path) = &cooked.material_bin_path {
                    push_output(
                        &mut record,
                        AssetCookedOutput {
                            role: "materialbin".to_string(),
                            kind: "materialbin".to_string(),
                            path: relative_path_string(material_bin_path, output_root),
                            hash: hash_file_hex(material_bin_path)?,
                        },
                        false,
                    );
                }
                push_output(
                    &mut record,
                    AssetCookedOutput {
                        role: "report".to_string(),
                        kind: "json".to_string(),
                        path: relative_path_string(&cooked.report_path, output_root),
                        hash: hash_file_hex(&cooked.report_path)?,
                    },
                    false,
                );
                if let Some(preview_path) = &cooked.preview_path {
                    push_output(
                        &mut record,
                        AssetCookedOutput {
                            role: "preview".to_string(),
                            kind: "ppm".to_string(),
                            path: relative_path_string(preview_path, output_root),
                            hash: hash_file_hex(preview_path)?,
                        },
                        false,
                    );
                }
                for texture in &cooked.material_bin.textures {
                    push_output(
                        &mut record,
                        AssetCookedOutput {
                            role: format!("texture:{}", texture.role),
                            kind: "ktx2".to_string(),
                            path: texture.cooked_path.clone(),
                            hash: texture.cooked_hash.clone(),
                        },
                        false,
                    );
                }
            }
        }
        "asset_graph" if source.extension().and_then(|v| v.to_str()) == Some("astergraph") => {
            match cook_asset_graph_asset(
                source,
                project_root,
                output_root,
                id,
                platform,
                Some(record.guid.as_str()),
            ) {
                Ok(cooked) => {
                    record.options_hash = hash_hex_text(&format!(
                        "assetgraphbin:{}:{}",
                        ASSET_GRAPH_BIN_SCHEMA_VERSION, platform
                    ));
                    record.derived_hashes = cooked.graph_bin.derived_hashes.clone();
                    record.derived_hashes.source_hash = record.source_hash.clone();
                    record.derived_hashes.options_hash = record.options_hash.clone();
                    record.diagnostics = cooked.graph_bin.diagnostics.clone();
                    push_output(
                        &mut record,
                        AssetCookedOutput {
                            role: "assetgraphbin".to_string(),
                            kind: "assetgraphbin".to_string(),
                            path: relative_path_string(&cooked.graph_bin_path, output_root),
                            hash: hash_file_hex(&cooked.graph_bin_path)?,
                        },
                        false,
                    );
                    push_output(
                        &mut record,
                        AssetCookedOutput {
                            role: "report".to_string(),
                            kind: "json".to_string(),
                            path: relative_path_string(&cooked.report_path, output_root),
                            hash: hash_file_hex(&cooked.report_path)?,
                        },
                        false,
                    );
                    for edge in &cooked.graph_bin.edges {
                        record.dependencies.push(AssetDependencyRecord {
                            role: edge.role.clone(),
                            path: format!("{}->{}", edge.from, edge.to),
                            present: true,
                            hash: hash_hex_text(&format!(
                                "{}:{}:{}",
                                edge.from, edge.to, edge.role
                            )),
                        });
                    }
                }
                Err(error) => record.diagnostics.push(cook_error(error.to_string())),
            }
        }
        "cave" => match serde_json::from_slice::<Value>(&source_bytes) {
            Ok(root) => {
                let (report, diagnostics) = cave_world_gate_report(
                    &root,
                    id,
                    &record.guid,
                    &source_rel,
                    &record.source_hash,
                );
                record.diagnostics.extend(diagnostics);
                record.options_hash = hash_hex_text(&format!("cave-world-gate:{}:{}", 1, platform));
                let report_path = output_root
                    .join("reports")
                    .join(format!("{}.world-gate.report.json", safe_stem(id, source)));
                write_json(&report_path, &report)?;
                push_output(
                    &mut record,
                    AssetCookedOutput {
                        role: "world-gate-report".to_string(),
                        kind: "json".to_string(),
                        path: relative_path_string(&report_path, output_root),
                        hash: hash_file_hex(&report_path)?,
                    },
                    false,
                );
                let neural_report = serde_json::json!({
                    "schema_version": 1,
                    "kind": "cave_neural_light_field_artifact",
                    "id": id,
                    "source_path": source_rel,
                    "region_id": report["region_id"].clone(),
                    "world_gate_report": report["probe_trace_hash"].clone(),
                    "neural_light_field": report["neural_light_field"].clone(),
                });
                let neural_report_path = output_root.join("reports").join(format!(
                    "{}.neural-light-field.report.json",
                    safe_stem(id, source)
                ));
                write_json(&neural_report_path, &neural_report)?;
                push_output(
                    &mut record,
                    AssetCookedOutput {
                        role: "neural-light-field-report".to_string(),
                        kind: "json".to_string(),
                        path: relative_path_string(&neural_report_path, output_root),
                        hash: hash_file_hex(&neural_report_path)?,
                    },
                    false,
                );
            }
            Err(error) => record
                .diagnostics
                .push(cook_error(format!("cave world gate parse failed: {error}"))),
        },
        "texture" => {
            let role = source
                .file_stem()
                .and_then(|value| value.to_str())
                .and_then(|stem| stem.rsplit_once('_').map(|(_, role)| role))
                .unwrap_or("unknown");
            match cook_texture_asset(source, output_root, role) {
                Ok(cooked) => {
                    record.options_hash = hash_hex_text("texture:ktx2-basis:v1");
                    push_output(
                        &mut record,
                        AssetCookedOutput {
                            role: "texture".to_string(),
                            kind: "ktx2".to_string(),
                            path: relative_path_string(&cooked.output_path, output_root),
                            hash: cooked.report.cooked_hash.clone(),
                        },
                        false,
                    );
                    push_output(
                        &mut record,
                        AssetCookedOutput {
                            role: "report".to_string(),
                            kind: "json".to_string(),
                            path: relative_path_string(&cooked.report_path, output_root),
                            hash: hash_file_hex(&cooked.report_path)?,
                        },
                        false,
                    );
                    record.dependencies.push(AssetDependencyRecord {
                        role: role.to_string(),
                        path: source_rel,
                        present: true,
                        hash: cooked.report.source_hash,
                    });
                }
                Err(error) => record.diagnostics.push(cook_error(error.to_string())),
            }
        }
        _ => record.diagnostics.push(cook_warning(format!(
            "Asset v2 cook does not transform kind '{}' from '{}'",
            declared_kind,
            source.display()
        ))),
    }
    refresh_dependency_edges(&mut record);
    Ok(record)
}

pub fn cook_asset_graph_asset(
    input: impl AsRef<Path>,
    project_root: impl AsRef<Path>,
    output_root: impl AsRef<Path>,
    fallback_id: &str,
    platform: &str,
    asset_guid_override: Option<&str>,
) -> Result<AssetGraphCookResult> {
    if platform != "desktop" {
        return Err(ContentError::new(format!(
            "unsupported asset graph platform '{platform}', expected desktop"
        )));
    }
    let input = input.as_ref();
    let project_root = project_root.as_ref();
    let output_root = output_root.as_ref();
    let mut graph_bin =
        load_asset_graph_bin_from_source(input, fallback_id, asset_guid_override, project_root)?;
    let quality_diagnostics = graph_bin
        .quality
        .issues
        .iter()
        .map(|issue| AssetCookDiagnostic {
            severity: issue.severity.clone(),
            message: format!("{}:{}:{}", issue.category, issue.node, issue.message),
            source_path: Some(graph_bin.source_path.clone()),
            line: None,
            column: None,
            source_locations: Vec::new(),
        })
        .collect::<Vec<_>>();
    graph_bin.diagnostics.extend(quality_diagnostics);
    let stem = safe_stem(fallback_id, input);
    let graph_bin_path = output_root
        .join("asset_graphs")
        .join(format!("{stem}.assetgraphbin"));
    let report_path = output_root
        .join("reports")
        .join(format!("{stem}.assetgraph.report.json"));
    write_json(&graph_bin_path, &graph_bin)?;
    let report = serde_json::json!({
        "schema_version": ASSET_GRAPH_BIN_SCHEMA_VERSION,
        "kind": "asset_graph",
        "id": graph_bin.id.clone(),
        "asset_guid": graph_bin.asset_guid.clone(),
        "source_path": graph_bin.source_path.clone(),
        "runtime_model": graph_bin.runtime_model.clone(),
        "material": graph_bin.material.clone(),
        "mesh": graph_bin.mesh.clone(),
        "perceptual_template": graph_bin.perceptual_template.clone(),
        "nodes": graph_bin.nodes.clone(),
        "edges": graph_bin.edges.clone(),
        "preview": graph_bin.preview.clone(),
        "quality": graph_bin.quality.clone(),
        "derived_hashes": graph_bin.derived_hashes.clone(),
        "diagnostics": graph_bin.diagnostics.clone(),
    });
    write_json(&report_path, &report)?;
    Ok(AssetGraphCookResult {
        graph_bin_path,
        report_path,
        graph_bin,
    })
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
    asset_guid_override: Option<&str>,
) -> Result<MaterialCookResult> {
    let input = input.as_ref();
    let project_root = project_root.as_ref();
    let output_root = output_root.as_ref();
    let source = fs::read_to_string(input)?;
    let parsed = parse_astermat_source(&source, fallback_id, input);
    let id = if parsed.id.is_empty() {
        fallback_id.to_string()
    } else {
        parsed.id.clone()
    };
    let asset_guid = asset_guid_override
        .map(str::to_string)
        .unwrap_or_else(|| asset_guid("material", &id, &relative_path_string(input, project_root)));
    let material_stem = safe_identifier(&id);
    let mut texture_records = Vec::new();
    let mut emitted_texture_artifacts = Vec::new();
    let mut dependencies = Vec::new();
    let mut diagnostics = parsed.diagnostics.clone();
    let mut resolved_textures = BTreeMap::new();

    for (role, uri) in &parsed.textures {
        let source_path = if Path::new(uri).is_absolute() {
            PathBuf::from(uri)
        } else {
            input.parent().unwrap_or_else(|| Path::new("")).join(uri)
        };
        if !source_path.exists() {
            diagnostics.push(cook_error(format!(
                "material texture '{}' is missing: {}",
                role,
                source_path.display()
            )));
            dependencies.push(AssetDependencyRecord {
                role: role.clone(),
                path: relative_path_string(&source_path, project_root),
                present: false,
                hash: String::new(),
            });
            continue;
        }
        dependencies.push(AssetDependencyRecord {
            role: role.clone(),
            path: relative_path_string(&source_path, project_root),
            present: true,
            hash: hash_file_hex(&source_path)?,
        });
        resolved_textures.insert(role.clone(), source_path);
    }

    let direct_roles = direct_material_texture_roles(&parsed);
    for role in &direct_roles {
        if let Some(source_path) = resolved_textures.get(role) {
            diagnostics.extend(texture_preflight_diagnostics(source_path, role));
        }
    }
    if parsed_requires_split_orm_pack(&parsed) {
        diagnostics.extend(split_orm_preflight_diagnostics(&resolved_textures));
    }

    let report_path = output_root
        .join("reports")
        .join(format!("{}.report.json", material_stem));

    if !diagnostics_have_errors(&diagnostics) {
        for role in &direct_roles {
            let Some(source_path) = resolved_textures.get(role) else {
                continue;
            };
            let cooked_name = format!("{}_{}.ktx2", material_stem, safe_identifier(role));
            let cooked_path = output_root.join("textures").join(cooked_name);
            match cook_texture_asset_as(source_path, output_root, role, &cooked_path) {
                Ok(cooked) => {
                    emitted_texture_artifacts.push(cooked.output_path.clone());
                    emitted_texture_artifacts.push(cooked.report_path.clone());
                    texture_records.push(MaterialBinTextureRecord {
                        role: role.clone(),
                        source_path: relative_path_string(source_path, project_root),
                        cooked_path: relative_path_string(&cooked.output_path, output_root),
                        kind: cooked.report.kind.clone(),
                        color_space: cooked.report.color_space.clone(),
                        source_format: cooked.report.source_format.clone(),
                        runtime_format: cooked.report.runtime_format.clone(),
                        width: cooked.report.width,
                        height: cooked.report.height,
                        mip_count: cooked.report.mip_count,
                        byte_cost: cooked.report.byte_cost,
                        encoder: cooked.report.encoder.clone(),
                        fallback_reason: cooked.report.fallback_reason.clone(),
                        platform_compatibility: cooked.report.platform_compatibility.clone(),
                        source_hash: cooked.report.source_hash.clone(),
                        cooked_hash: cooked.report.cooked_hash.clone(),
                        diagnostics: cooked.report.diagnostics.clone(),
                    });
                }
                Err(error) => diagnostics.push(cook_error(error.to_string())),
            }
        }
        if parsed_requires_split_orm_pack(&parsed) && !diagnostics_have_errors(&diagnostics) {
            match cook_split_orm_texture_as(
                &resolved_textures,
                project_root,
                output_root,
                &material_stem,
            ) {
                Ok(record) => texture_records.push(record),
                Err(error) => {
                    let _ = fs::remove_file(
                        output_root
                            .join("textures")
                            .join(format!("{}_orm.ktx2", material_stem)),
                    );
                    let _ = fs::remove_file(
                        output_root
                            .join("reports")
                            .join(format!("{}.orm.report.json", material_stem)),
                    );
                    diagnostics.push(cook_error(error.to_string()));
                }
            }
        }
    }

    let material_bin = build_material_bin(
        &parsed,
        asset_guid,
        &id,
        relative_path_string(input, project_root),
        &source,
        texture_records,
        dependencies,
        diagnostics,
        platform,
    );
    write_json(&report_path, &material_bin)?;

    if diagnostics_have_errors(&material_bin.diagnostics) {
        for artifact in emitted_texture_artifacts {
            let _ = fs::remove_file(artifact);
        }
        return Ok(MaterialCookResult {
            material_bin_path: None,
            report_path,
            preview_path: None,
            material_bin,
            emitted_runtime_outputs: false,
        });
    }

    let material_bin_path = output_root
        .join("materials")
        .join(format!("{}.materialbin", material_stem));
    write_json(&material_bin_path, &material_bin)?;
    let preview_path = output_root
        .join("previews")
        .join(format!("{}.preview.ppm", material_stem));
    write_material_preview(&preview_path, &material_bin)?;
    Ok(MaterialCookResult {
        material_bin_path: Some(material_bin_path),
        report_path,
        preview_path: Some(preview_path),
        material_bin,
        emitted_runtime_outputs: true,
    })
}

pub fn inspect_material_asset(
    input: impl AsRef<Path>,
    asset_root: impl AsRef<Path>,
) -> Result<MaterialInspectReport> {
    let input = input.as_ref();
    let asset_root = asset_root.as_ref();
    let source = fs::read_to_string(input)?;
    let fallback_id = input
        .file_stem()
        .and_then(|value| value.to_str())
        .unwrap_or("material");
    let mut parsed = parse_astermat_source(&source, fallback_id, input);
    let root = if asset_root.as_os_str().is_empty() {
        input.parent().unwrap_or_else(|| Path::new(""))
    } else {
        asset_root
    };
    let mut dependencies = Vec::new();
    let mut textures = Vec::new();
    let texture_entries = parsed
        .textures
        .iter()
        .map(|(role, uri)| (role.clone(), uri.clone()))
        .collect::<Vec<_>>();
    for (role, uri) in texture_entries {
        let path = if Path::new(&uri).is_absolute() {
            PathBuf::from(uri)
        } else {
            root.join(uri)
        };
        if !path.exists() {
            dependencies.push(AssetDependencyRecord {
                role: role.clone(),
                path: relative_path_string(&path, root),
                present: false,
                hash: String::new(),
            });
            parsed.diagnostics.push(cook_error(format!(
                "material texture '{}' is missing: {}",
                role,
                path.display()
            )));
            textures.push(MaterialInspectTexture {
                role: role.clone(),
                path: relative_path_string(&path, root),
                present: false,
                kind: TextureImportKind::role(&role).as_str().to_string(),
                color_space: TextureImportKind::role(&role).color_space().to_string(),
                source_format: "missing".to_string(),
                width: 0,
                height: 0,
                mip_count: 0,
                source_hash: String::new(),
                diagnostics: vec![format!("error: texture is missing: {}", path.display())],
            });
            continue;
        }
        let summary = inspect_texture(&path, &role)?;
        dependencies.push(AssetDependencyRecord {
            role: role.clone(),
            path: relative_path_string(&path, root),
            present: true,
            hash: hex_hash(&summary.source_hash),
        });
        for diagnostic in &summary.diagnostics {
            if diagnostic.starts_with("error:") {
                parsed.diagnostics.push(cook_error(format!(
                    "texture '{}' for role '{}' failed validation: {}",
                    path.display(),
                    role,
                    diagnostic.trim_start_matches("error: ")
                )));
            }
        }
        textures.push(MaterialInspectTexture {
            role: role.clone(),
            path: relative_path_string(&path, root),
            present: true,
            kind: summary.kind.as_str().to_string(),
            color_space: summary.color_space,
            source_format: summary.format,
            width: summary.width,
            height: summary.height,
            mip_count: summary.mip_count,
            source_hash: hex_hash(&summary.source_hash),
            diagnostics: summary.diagnostics,
        });
    }
    Ok(MaterialInspectReport {
        schema_version: parsed.schema_version,
        id: parsed.id,
        name: parsed.name,
        source_path: input.to_string_lossy().replace('\\', "/"),
        shading_model: parsed.shading_model,
        required_runtime_roles: vec![
            "albedo".to_string(),
            "normal".to_string(),
            "orm".to_string(),
        ],
        textures,
        dependencies,
        production_ready: !diagnostics_have_errors(&parsed.diagnostics),
        diagnostics: parsed.diagnostics,
        provenance: parsed.provenance,
        authoring: parsed.authoring,
        preview: parsed.preview,
        quality_profile: parsed.quality_profile,
        platform_compatibility: "desktop".to_string(),
    })
}

pub fn material_inspect_report_json(
    input: impl AsRef<Path>,
    asset_root: impl AsRef<Path>,
) -> Result<String> {
    Ok(serde_json::to_string_pretty(&inspect_material_asset(
        input, asset_root,
    )?)?)
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
    let world_ready_count = database
        .records
        .iter()
        .filter(|record| record.fate_report.world_ready.accepted)
        .count();
    format!(
        "Aster asset database v{} platform={} assets={} outputs={} errors={} warnings={} world_ready={}",
        database.schema_version,
        database.platform,
        database.records.len(),
        output_count,
        error_count,
        warning_count,
        world_ready_count
    )
}

pub fn asset_graph_report_json(database: &AssetDatabase) -> Result<String> {
    let mut database = database.clone();
    refresh_asset_database_truth(&mut database);
    Ok(serde_json::to_string_pretty(&database.asset_graph)?)
}

pub fn asset_fate_report_json(database: &AssetDatabase, asset_id: &str) -> Result<String> {
    let mut database = database.clone();
    refresh_asset_database_truth(&mut database);
    let report = database
        .fate_reports
        .iter()
        .find(|report| report.asset_id == asset_id || report.asset_guid == asset_id)
        .ok_or_else(|| ContentError::new(format!("asset fate '{asset_id}' was not found")))?;
    Ok(serde_json::to_string_pretty(report)?)
}

pub fn asset_database_diff_json(before: &AssetDatabase, after: &AssetDatabase) -> Result<String> {
    let mut before = before.clone();
    let mut after = after.clone();
    refresh_asset_database_truth(&mut before);
    refresh_asset_database_truth(&mut after);
    let before_records = before
        .records
        .iter()
        .map(|record| (record.id.clone(), record))
        .collect::<BTreeMap<_, _>>();
    let after_records = after
        .records
        .iter()
        .map(|record| (record.id.clone(), record))
        .collect::<BTreeMap<_, _>>();
    let mut added = Vec::new();
    let mut removed = Vec::new();
    let mut changed = Vec::new();
    for id in after_records.keys() {
        if !before_records.contains_key(id) {
            added.push(id.clone());
        }
    }
    for id in before_records.keys() {
        if !after_records.contains_key(id) {
            removed.push(id.clone());
        }
    }
    for (id, after_record) in &after_records {
        let Some(before_record) = before_records.get(id) else {
            continue;
        };
        let mut reasons = Vec::new();
        if before_record.source_hash != after_record.source_hash {
            reasons.push("source_hash");
        }
        if before_record.options_hash != after_record.options_hash {
            reasons.push("options_hash");
        }
        if before_record.derived_hashes.dependency_hash
            != after_record.derived_hashes.dependency_hash
        {
            reasons.push("dependency_hash");
        }
        if before_record.derived_hashes.artifact_hash != after_record.derived_hashes.artifact_hash {
            reasons.push("artifact_hash");
        }
        if before_record.derived_hashes.material_hash != after_record.derived_hashes.material_hash {
            reasons.push("material_hash");
        }
        if before_record.derived_hashes.shader_variant_key
            != after_record.derived_hashes.shader_variant_key
        {
            reasons.push("shader_variant_key");
        }
        if before_record.derived_hashes.pipeline_cache_key
            != after_record.derived_hashes.pipeline_cache_key
        {
            reasons.push("pipeline_cache_key");
        }
        if !reasons.is_empty() {
            changed.push(serde_json::json!({
                "id": id,
                "kind": after_record.kind,
                "reasons": reasons,
                "before": before_record.derived_hashes,
                "after": after_record.derived_hashes,
            }));
        }
    }
    let diff = serde_json::json!({
        "schema_version": 1,
        "before_fingerprint": before.asset_graph.project_fingerprint,
        "after_fingerprint": after.asset_graph.project_fingerprint,
        "added": added,
        "removed": removed,
        "changed": changed,
    });
    Ok(serde_json::to_string_pretty(&diff)?)
}

fn variant_intent_tags_for_record(record: &AssetDatabaseRecord) -> Vec<String> {
    let mut tags = Vec::new();
    let mut add = |prefix: &str, value: &str| {
        if !value.is_empty() && value != "default" {
            tags.push(format!("{prefix}{value}"));
        }
    };
    add("collision:", &record.import_preset.collision_policy);
    add("lod:", &record.import_preset.lod_policy);
    add("texture-policy:", &record.import_preset.texture_role_policy);
    add(
        "material-slots:",
        &record.import_preset.material_slot_policy,
    );
    if !record.derived_hashes.material_hash.is_empty() {
        tags.push("material-variant".to_string());
    }
    if !record.derived_hashes.pipeline_cache_key.is_empty() {
        tags.push("pipeline-variant".to_string());
    }
    tags.sort();
    tags.dedup();
    tags
}

fn production_readiness_reasons_for_record(record: &AssetDatabaseRecord) -> Vec<String> {
    let mut reasons = Vec::new();
    let has_errors = record
        .diagnostics
        .iter()
        .any(|diagnostic| diagnostic.severity == "error");
    let missing_dependencies = record
        .dependencies
        .iter()
        .filter(|dependency| !dependency.present)
        .count()
        + record
            .dependency_edges
            .iter()
            .filter(|edge| !edge.present)
            .count();
    if record.fate_report.production_ready {
        reasons.push("production-ready".to_string());
    }
    if !has_errors && missing_dependencies == 0 && !record.outputs.is_empty() {
        reasons.push("runtime-artifacts-present".to_string());
    }
    if has_errors {
        reasons.push("blocked-by-errors".to_string());
    }
    if missing_dependencies > 0 {
        reasons.push("blocked-by-missing-dependencies".to_string());
    }
    if record.outputs.is_empty() {
        reasons.push("blocked-by-missing-runtime-output".to_string());
    }
    if !record.derived_hashes.shader_variant_key.is_empty() {
        reasons.push("shader-variant-tracked".to_string());
    }
    if !record.derived_hashes.pipeline_cache_key.is_empty() {
        reasons.push("pipeline-key-tracked".to_string());
    }
    reasons.sort();
    reasons.dedup();
    reasons
}

pub fn asset_foundry_report_json(database: &AssetDatabase) -> Result<String> {
    let mut database = database.clone();
    refresh_asset_database_truth(&mut database);
    let catalogs = catalog_store_from_database(&database);
    let production_ready = database
        .records
        .iter()
        .filter(|record| record.fate_report.production_ready)
        .count();
    let mut variant_intent_tags = Vec::new();
    let mut readiness_reasons = Vec::new();
    let recipes = database
        .records
        .iter()
        .map(|record| {
            let variants = variant_intent_tags_for_record(record);
            let reasons = production_readiness_reasons_for_record(record);
            variant_intent_tags.extend(variants.clone());
            readiness_reasons.extend(reasons.clone());
            serde_json::json!({
                "id": record.id,
                "guid": record.guid,
                "kind": record.kind,
                "source_path": record.source_path,
                "catalog_path": catalog_path_for_record(record),
                "import_preset": record.import_preset,
                "platform_profile": record.platform_profile,
                "dependency_count": record.dependencies.len() + record.dependency_edges.len(),
                "variant_intent_tags": variants,
                "production_readiness_reasons": reasons,
            })
        })
        .collect::<Vec<_>>();
    variant_intent_tags.sort();
    variant_intent_tags.dedup();
    readiness_reasons.sort();
    readiness_reasons.dedup();
    let report = serde_json::json!({
        "schema_version": 1,
        "platform": database.platform,
        "assets": database.records.len(),
        "catalogs": catalogs.catalogs.len(),
        "production_ready_assets": production_ready,
        "dependency_edges": database.asset_graph.edges.len(),
        "variant_intent_tags": variant_intent_tags,
        "production_readiness_reasons": readiness_reasons,
        "catalog_store": catalogs,
        "import_recipes": recipes,
    });
    Ok(serde_json::to_string_pretty(&report)?)
}

pub fn cook_lineage_report_json(database: &AssetDatabase) -> Result<String> {
    let mut database = database.clone();
    refresh_asset_database_truth(&mut database);
    let artifact_manifest_hash = hash_serializable(
        "aster.asset.artifact_manifest.v1",
        &database
            .records
            .iter()
            .map(|record| {
                (
                    record.guid.as_str(),
                    record.id.as_str(),
                    record.source_hash.as_str(),
                    record.options_hash.as_str(),
                    record.derived_hashes.dependency_hash.as_str(),
                    record.derived_hashes.artifact_hash.as_str(),
                    record
                        .outputs
                        .iter()
                        .map(|output| {
                            (
                                output.role.as_str(),
                                output.kind.as_str(),
                                output.path.as_str(),
                                output.hash.as_str(),
                            )
                        })
                        .collect::<Vec<_>>(),
                )
            })
            .collect::<Vec<_>>(),
    );
    let assets = database
        .records
        .iter()
        .map(|record| {
            serde_json::json!({
                "id": record.id,
                "guid": record.guid,
                "kind": record.kind,
                "source_path": record.source_path,
                "production_ready": record.fate_report.production_ready,
                "dependency_count": record.dependencies.len() + record.dependency_edges.len(),
                "output_count": record.outputs.len(),
                "diagnostic_count": record.diagnostics.len(),
                "hashes": record.derived_hashes,
                "referentially_transparent": true,
                "chain": record.fate_report.chain,
                "production_readiness_reasons": production_readiness_reasons_for_record(record),
            })
        })
        .collect::<Vec<_>>();
    let output_count: usize = database
        .records
        .iter()
        .map(|record| record.outputs.len())
        .sum();
    let report = serde_json::json!({
        "schema_version": 1,
        "platform": database.platform,
        "project_fingerprint": database.asset_graph.project_fingerprint,
        "artifact_manifest_hash": artifact_manifest_hash,
        "referentially_transparent_build": true,
        "asset_count": database.records.len(),
        "production_ready_assets": database
            .records
            .iter()
            .filter(|record| record.fate_report.production_ready)
            .count(),
        "dependency_edge_count": database.asset_graph.edges.len(),
        "output_count": output_count,
        "assets": assets,
    });
    Ok(serde_json::to_string_pretty(&report)?)
}

pub fn cook_lineage_diff_json(before: &AssetDatabase, after: &AssetDatabase) -> Result<String> {
    let mut before = before.clone();
    let mut after = after.clone();
    refresh_asset_database_truth(&mut before);
    refresh_asset_database_truth(&mut after);
    let before_records = before
        .records
        .iter()
        .map(|record| (record.id.clone(), record))
        .collect::<BTreeMap<_, _>>();
    let mut changed = Vec::new();
    for after_record in &after.records {
        let Some(before_record) = before_records.get(&after_record.id) else {
            changed.push(serde_json::json!({
                "id": after_record.id,
                "change": "added",
                "after_reasons": production_readiness_reasons_for_record(after_record),
            }));
            continue;
        };
        let mut reasons = Vec::new();
        if before_record.fate_report.production_ready != after_record.fate_report.production_ready {
            reasons.push("production_ready");
        }
        if before_record.derived_hashes != after_record.derived_hashes {
            reasons.push("derived_hashes");
        }
        if before_record.outputs.len() != after_record.outputs.len() {
            reasons.push("outputs");
        }
        if before_record.dependencies.len() != after_record.dependencies.len() {
            reasons.push("dependencies");
        }
        if !reasons.is_empty() {
            changed.push(serde_json::json!({
                "id": after_record.id,
                "change": "changed",
                "reasons": reasons,
                "before_reasons": production_readiness_reasons_for_record(before_record),
                "after_reasons": production_readiness_reasons_for_record(after_record),
            }));
        }
    }
    for before_record in &before.records {
        if !after
            .records
            .iter()
            .any(|record| record.id == before_record.id)
        {
            changed.push(serde_json::json!({
                "id": before_record.id,
                "change": "removed",
                "before_reasons": production_readiness_reasons_for_record(before_record),
            }));
        }
    }
    let report = serde_json::json!({
        "schema_version": 1,
        "before_fingerprint": before.asset_graph.project_fingerprint,
        "after_fingerprint": after.asset_graph.project_fingerprint,
        "changed": changed,
    });
    Ok(serde_json::to_string_pretty(&report)?)
}

#[derive(Clone, Debug, Serialize, Deserialize, PartialEq, Eq)]
pub struct ConfigLayerRecord {
    pub name: String,
    #[serde(default)]
    pub priority: u32,
    #[serde(default)]
    pub values: BTreeMap<String, String>,
}

#[derive(Clone, Debug, Default, Serialize, Deserialize, PartialEq, Eq)]
pub struct ConfigLayerStackRecord {
    pub layers: Vec<ConfigLayerRecord>,
}

impl ConfigLayerStackRecord {
    pub fn resolve(&self) -> BTreeMap<String, String> {
        let mut layers = self.layers.clone();
        layers.sort_by_key(|layer| layer.priority);
        let mut merged = BTreeMap::new();
        for layer in layers {
            for (key, value) in layer.values {
                merged.insert(key, value);
            }
        }
        merged
    }
}

#[derive(Clone, Debug, Serialize, Deserialize, PartialEq, Eq)]
pub struct SessionJournalRecord {
    pub session_id: String,
    #[serde(default)]
    pub kind: String,
    #[serde(default)]
    pub text: String,
    #[serde(default)]
    pub detail: String,
    #[serde(default)]
    pub ts: u64,
    #[serde(default)]
    pub sequence: u64,
}

pub fn session_audit_report_json(
    input: impl AsRef<Path>,
    max_bytes: Option<usize>,
) -> Result<String> {
    let input = input.as_ref();
    let text = fs::read_to_string(input)?;
    let mut total_bytes = 0usize;
    let mut entries = Vec::new();
    for line in text.lines().filter(|line| !line.trim().is_empty()) {
        total_bytes += line.len() + 1;
        let entry: SessionJournalRecord = serde_json::from_str(line)?;
        entries.push(entry);
    }
    let mut retained_entries = entries.clone();
    if let Some(max_bytes) = max_bytes {
        let mut retained_bytes = total_bytes;
        while retained_bytes > max_bytes && !retained_entries.is_empty() {
            let dropped = serde_json::to_string(&retained_entries.remove(0))?.len() + 1;
            retained_bytes = retained_bytes.saturating_sub(dropped);
        }
    }
    let mut by_session = BTreeMap::<String, usize>::new();
    for entry in &retained_entries {
        *by_session.entry(entry.session_id.clone()).or_default() += 1;
    }
    let report = serde_json::json!({
        "schema_version": 1,
        "path": input.to_string_lossy().replace('\\', "/"),
        "entries": entries.len(),
        "retained_entries": retained_entries.len(),
        "bytes": total_bytes,
        "max_bytes": max_bytes,
        "sessions": by_session,
    });
    Ok(serde_json::to_string_pretty(&report)?)
}

pub fn mesh_recipe_inspect_report_json(input: impl AsRef<Path>) -> Result<String> {
    let input = input.as_ref();
    let value: Value = serde_json::from_slice(&fs::read(input)?)?;
    let steps = value
        .get("steps")
        .and_then(Value::as_array)
        .map(Vec::len)
        .unwrap_or(0);
    let variants = value
        .get("variant_intent_tags")
        .and_then(Value::as_array)
        .map(|items| {
            items
                .iter()
                .filter_map(Value::as_str)
                .map(str::to_string)
                .collect::<Vec<_>>()
        })
        .unwrap_or_default();
    let mut operations = BTreeMap::<String, usize>::new();
    if let Some(step_values) = value.get("steps").and_then(Value::as_array) {
        for step in step_values {
            let kind = step
                .get("kind")
                .and_then(Value::as_str)
                .unwrap_or("validate")
                .to_string();
            *operations.entry(kind).or_default() += 1;
        }
    }
    let report = serde_json::json!({
        "schema_version": 1,
        "path": input.to_string_lossy().replace('\\', "/"),
        "id": value.get("id").and_then(Value::as_str).unwrap_or("mesh.recipe"),
        "steps": steps,
        "operations": operations,
        "variant_intent_tags": variants,
        "quality_floor": if steps == 0 { 0 } else { 80 },
    });
    Ok(serde_json::to_string_pretty(&report)?)
}

fn cook_texture_asset_as(
    input: &Path,
    output_root: &Path,
    role: &str,
    output_path: &Path,
) -> Result<TextureCookResult> {
    let input_summary = inspect_texture(input, role)?;
    if texture_summary_has_error(&input_summary) {
        return Err(ContentError::new(format!(
            "texture '{}' failed validation: {}",
            input.display(),
            input_summary.diagnostics.join("; ")
        )));
    }
    if input_summary.format != "ktx2" && std::env::var("ASTER_TEXTURE_ENCODER").is_err() {
        return Err(ContentError::new(format!(
            "texture '{}' is {} source; strict cook requires KTX2 source or ASTER_TEXTURE_ENCODER",
            input.display(),
            input_summary.format
        )));
    }
    let summary = bake_texture_to_ktx2(input, output_path, role)?;
    let cooked_hash = hash_file_hex(output_path)?;
    let byte_cost = fs::metadata(output_path)?.len();
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
        format: summary.format.clone(),
        source_format: summary.format,
        runtime_format: "ktx2".to_string(),
        width: summary.width,
        height: summary.height,
        mip_count: summary.mip_count,
        byte_cost,
        encoder: texture_encoder_name(&input_summary),
        fallback_reason: String::new(),
        platform_compatibility: "desktop:ok".to_string(),
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

fn is_split_orm_role(role: &str) -> bool {
    matches!(role, "roughness" | "metallic" | "ao")
}

fn direct_material_texture_roles(parsed: &ParsedMaterialSource) -> Vec<String> {
    parsed
        .textures
        .keys()
        .filter(|role| !is_split_orm_role(role))
        .cloned()
        .collect()
}

fn parsed_requires_split_orm_pack(parsed: &ParsedMaterialSource) -> bool {
    !parsed.textures.contains_key("orm")
        && ["roughness", "metallic", "ao"]
            .iter()
            .all(|role| parsed.textures.contains_key(*role))
}

fn texture_preflight_diagnostics(source_path: &Path, role: &str) -> Vec<AssetCookDiagnostic> {
    match inspect_texture(source_path, role) {
        Ok(summary) => {
            let mut diagnostics = summary
                .diagnostics
                .iter()
                .filter(|diagnostic| diagnostic.starts_with("error:"))
                .map(|diagnostic| {
                    cook_error(format!(
                        "texture '{}' for role '{}' failed validation: {}",
                        source_path.display(),
                        role,
                        diagnostic.trim_start_matches("error: ")
                    ))
                })
                .collect::<Vec<_>>();
            if summary.format != "ktx2" && std::env::var("ASTER_TEXTURE_ENCODER").is_err() {
                diagnostics.push(cook_error(format!(
                    "texture '{}' for role '{}' is {} source; strict cook requires KTX2 source or ASTER_TEXTURE_ENCODER",
                    source_path.display(),
                    role,
                    summary.format
                )));
            }
            diagnostics
        }
        Err(error) => vec![cook_error(format!(
            "texture '{}' for role '{}' failed inspection: {}",
            source_path.display(),
            role,
            error
        ))],
    }
}

fn split_orm_preflight_diagnostics(
    resolved_textures: &BTreeMap<String, PathBuf>,
) -> Vec<AssetCookDiagnostic> {
    let mut diagnostics = Vec::new();
    if std::env::var("ASTER_TEXTURE_ENCODER").is_err() {
        diagnostics.push(cook_error(
            "split roughness/metallic/ao ORM packing requires ASTER_TEXTURE_ENCODER",
        ));
    }
    for role in ["roughness", "metallic", "ao"] {
        if let Some(path) = resolved_textures.get(role) {
            diagnostics.extend(texture_preflight_diagnostics(path, role));
        }
    }
    diagnostics
}

fn cook_split_orm_texture_as(
    resolved_textures: &BTreeMap<String, PathBuf>,
    project_root: &Path,
    output_root: &Path,
    material_stem: &str,
) -> Result<MaterialBinTextureRecord> {
    let encoder = std::env::var("ASTER_TEXTURE_ENCODER").map_err(|_| {
        ContentError::new("split roughness/metallic/ao ORM packing requires ASTER_TEXTURE_ENCODER")
    })?;
    let roughness = resolved_textures
        .get("roughness")
        .ok_or_else(|| ContentError::new("split ORM packing is missing roughness source"))?;
    let metallic = resolved_textures
        .get("metallic")
        .ok_or_else(|| ContentError::new("split ORM packing is missing metallic source"))?;
    let ao = resolved_textures
        .get("ao")
        .ok_or_else(|| ContentError::new("split ORM packing is missing ao source"))?;
    let output_path = output_root
        .join("textures")
        .join(format!("{}_orm.ktx2", material_stem));
    if let Some(parent) = output_path.parent() {
        fs::create_dir_all(parent)?;
    }
    let status = Command::new(&encoder)
        .arg("--pack-orm")
        .arg("--roughness")
        .arg(roughness)
        .arg("--metallic")
        .arg(metallic)
        .arg("--ao")
        .arg(ao)
        .arg("--output")
        .arg(&output_path)
        .status()
        .map_err(|error| {
            ContentError::new(format!(
                "failed to run texture encoder '{encoder}': {error}"
            ))
        })?;
    if !status.success() {
        return Err(ContentError::new(format!(
            "texture encoder '{encoder}' failed while packing ORM"
        )));
    }
    let summary = inspect_texture(&output_path, "orm")?;
    if texture_summary_has_error(&summary) {
        return Err(ContentError::new(format!(
            "texture encoder '{encoder}' wrote invalid ORM KTX2: {}",
            summary.diagnostics.join("; ")
        )));
    }
    let mut hasher = blake3::Hasher::new();
    for (role, path) in [("roughness", roughness), ("metallic", metallic), ("ao", ao)] {
        hasher.update(role.as_bytes());
        hasher.update(&fs::read(path)?);
    }
    let source_hash = hex_hash(hasher.finalize().as_bytes());
    let cooked_hash = hash_file_hex(&output_path)?;
    let byte_cost = fs::metadata(&output_path)?.len();
    let source_path = format!(
        "roughness={};metallic={};ao={}",
        relative_path_string(roughness, project_root),
        relative_path_string(metallic, project_root),
        relative_path_string(ao, project_root)
    );
    let report = TextureCookReport {
        schema_version: 1,
        role: "orm".to_string(),
        kind: "orm".to_string(),
        color_space: "linear".to_string(),
        format: "split-orm".to_string(),
        source_format: "split-orm".to_string(),
        runtime_format: "ktx2".to_string(),
        width: summary.width,
        height: summary.height,
        mip_count: summary.mip_count,
        byte_cost,
        encoder: format!("external:{encoder}"),
        fallback_reason: String::new(),
        platform_compatibility: "desktop:ok".to_string(),
        source_hash: source_hash.clone(),
        cooked_hash: cooked_hash.clone(),
        cooked_path: relative_path_string(&output_path, output_root),
        diagnostics: summary.diagnostics,
    };
    let report_path = output_root
        .join("reports")
        .join(format!("{}.orm.report.json", material_stem));
    write_json(&report_path, &report)?;
    Ok(MaterialBinTextureRecord {
        role: "orm".to_string(),
        source_path,
        cooked_path: report.cooked_path,
        kind: report.kind,
        color_space: report.color_space,
        source_format: report.source_format,
        runtime_format: report.runtime_format,
        width: report.width,
        height: report.height,
        mip_count: report.mip_count,
        byte_cost: report.byte_cost,
        encoder: report.encoder,
        fallback_reason: report.fallback_reason,
        platform_compatibility: report.platform_compatibility,
        source_hash,
        cooked_hash,
        diagnostics: report.diagnostics,
    })
}

fn build_material_bin(
    parsed: &ParsedMaterialSource,
    asset_guid: String,
    id: &str,
    source_path: String,
    source: &str,
    texture_records: Vec<MaterialBinTextureRecord>,
    dependencies: Vec<AssetDependencyRecord>,
    diagnostics: Vec<AssetCookDiagnostic>,
    platform: &str,
) -> MaterialBin {
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
    let shader_variant_key = material_variant_key(source, &texture_records);
    let pipeline_tag = material_bin_pipeline_tag(parsed, !texture_records.is_empty());
    let dependency_hash = hash_serializable("aster.material.dependencies.v2", &dependencies);
    let artifact_seed = texture_records
        .iter()
        .map(|texture| {
            (
                texture.role.as_str(),
                texture.source_hash.as_str(),
                texture.cooked_hash.as_str(),
            )
        })
        .collect::<Vec<_>>();
    let derived_hashes = AssetDerivedHashes {
        source_hash: hash_hex_text(source),
        options_hash: hash_hex_text(&format!(
            "materialbin:{}:{}:{}",
            MATERIAL_BIN_SCHEMA_VERSION, platform, pipeline_tag
        )),
        dependency_hash: dependency_hash.clone(),
        artifact_hash: hash_serializable("aster.material.artifacts.v2", &artifact_seed),
        material_hash: hash_hex_text(&format!(
            "material:{}:{}:{}",
            source, dependency_hash, pipeline_tag
        )),
        shader_variant_key: format!("0x{shader_variant_key:016x}"),
        pipeline_cache_key: hash_hex_text(&pipeline_tag),
        vertex_input_contract: String::new(),
        frame_plan_fingerprint: String::new(),
    };
    MaterialBin {
        schema_version: MATERIAL_BIN_SCHEMA_VERSION,
        asset_guid,
        id: id.to_string(),
        name: parsed.name.clone(),
        source_path,
        feature_mask: material_feature_mask(parsed),
        shader_variant_key,
        shader_variant_tag: material_variant_tag(parsed, !texture_records.is_empty()),
        pipeline_tag,
        fallback: MaterialBinFallback {
            base_color: [
                param_or(parsed, "base_color_r", 1.0),
                param_or(parsed, "base_color_g", 1.0),
                param_or(parsed, "base_color_b", 1.0),
            ],
            emission_color: [
                param_or(parsed, "emission_color_r", 0.0),
                param_or(parsed, "emission_color_g", 0.0),
                param_or(parsed, "emission_color_b", 0.0),
            ],
            roughness: param_or(parsed, "roughness", 0.55),
            metallic: param_or(parsed, "metallic", 0.0),
            emission_strength: param_or(parsed, "emission_strength", 0.0),
            opacity: param_or(parsed, "opacity", 1.0),
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
        provenance: parsed.provenance.clone(),
        authoring: parsed.authoring.clone(),
        preview: parsed.preview.clone(),
        quality_profile: parsed.quality_profile.clone(),
        textures: texture_records,
        binding_layout,
        dependency_hashes: dependencies,
        derived_hashes,
        diagnostics: diagnostics
            .into_iter()
            .chain(if platform == "desktop" {
                Vec::new()
            } else {
                vec![cook_error(format!(
                    "unsupported material platform '{platform}', expected desktop"
                ))]
            })
            .collect(),
    }
}

#[derive(Clone, Debug, Default)]
struct ParsedMaterialLayer {
    name: String,
    operation: String,
    arguments: Vec<String>,
    raw: String,
}

#[derive(Clone, Debug, Default)]
struct ParsedMaterialSource {
    schema_version: u32,
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
    provenance: BTreeMap<String, String>,
    authoring: BTreeMap<String, String>,
    preview: BTreeMap<String, String>,
    quality_profile: BTreeMap<String, String>,
    layers: Vec<ParsedMaterialLayer>,
    diagnostics: Vec<AssetCookDiagnostic>,
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
enum MaterialTokenKind {
    Identifier,
    String,
    Number,
    LeftBrace,
    RightBrace,
    LeftParen,
    RightParen,
    Colon,
    Comma,
    End,
}

#[derive(Clone, Debug)]
struct MaterialToken {
    kind: MaterialTokenKind,
    text: String,
    line: usize,
    column: usize,
}

struct MaterialLexer<'a> {
    source: &'a str,
    source_path: String,
    position: usize,
    line: usize,
    column: usize,
    diagnostics: Vec<AssetCookDiagnostic>,
}

impl<'a> MaterialLexer<'a> {
    fn new(source: &'a str, source_path: &Path) -> Self {
        Self {
            source,
            source_path: source_path.to_string_lossy().replace('\\', "/"),
            position: 0,
            line: 1,
            column: 1,
            diagnostics: Vec::new(),
        }
    }

    fn next(&mut self) -> MaterialToken {
        self.skip_trivia();
        let line = self.line;
        let column = self.column;
        let Some(ch) = self.peek_char() else {
            return MaterialToken {
                kind: MaterialTokenKind::End,
                text: String::new(),
                line,
                column,
            };
        };
        match ch {
            '{' => return self.single(MaterialTokenKind::LeftBrace, "{"),
            '}' => return self.single(MaterialTokenKind::RightBrace, "}"),
            '(' => return self.single(MaterialTokenKind::LeftParen, "("),
            ')' => return self.single(MaterialTokenKind::RightParen, ")"),
            ':' => return self.single(MaterialTokenKind::Colon, ":"),
            ',' => return self.single(MaterialTokenKind::Comma, ","),
            '"' => return self.string_token(),
            _ => {}
        }
        if ch.is_ascii_digit() || ch == '-' || ch == '+' {
            return self.number_token();
        }
        if ch.is_ascii_alphabetic() || ch == '_' {
            return self.identifier_token();
        }
        self.diagnostics.push(cook_error_at(
            self.source_path.clone(),
            line,
            column,
            format!("unexpected character '{ch}'"),
        ));
        self.advance_char();
        self.next()
    }

    fn single(&mut self, kind: MaterialTokenKind, text: &str) -> MaterialToken {
        let token = MaterialToken {
            kind,
            text: text.to_string(),
            line: self.line,
            column: self.column,
        };
        self.advance_char();
        token
    }

    fn identifier_token(&mut self) -> MaterialToken {
        let line = self.line;
        let column = self.column;
        let mut text = String::new();
        while let Some(ch) = self.peek_char() {
            if ch.is_ascii_alphanumeric() || ch == '_' || ch == '-' || ch == '.' {
                text.push(ch);
                self.advance_char();
            } else {
                break;
            }
        }
        MaterialToken {
            kind: MaterialTokenKind::Identifier,
            text,
            line,
            column,
        }
    }

    fn number_token(&mut self) -> MaterialToken {
        let line = self.line;
        let column = self.column;
        let mut text = String::new();
        if let Some(ch) = self.peek_char() {
            if ch == '-' || ch == '+' {
                text.push(ch);
                self.advance_char();
            }
        }
        while let Some(ch) = self.peek_char() {
            if ch.is_ascii_digit() || ch == '.' || ch == 'e' || ch == 'E' || ch == '-' || ch == '+'
            {
                text.push(ch);
                self.advance_char();
                if (ch == '-' || ch == '+')
                    && text.len() > 1
                    && !text[..text.len() - 1].ends_with('e')
                    && !text[..text.len() - 1].ends_with('E')
                {
                    break;
                }
            } else {
                break;
            }
        }
        MaterialToken {
            kind: MaterialTokenKind::Number,
            text,
            line,
            column,
        }
    }

    fn string_token(&mut self) -> MaterialToken {
        let line = self.line;
        let column = self.column;
        let mut text = String::new();
        self.advance_char();
        while let Some(ch) = self.peek_char() {
            if ch == '"' {
                self.advance_char();
                return MaterialToken {
                    kind: MaterialTokenKind::String,
                    text,
                    line,
                    column,
                };
            }
            if ch == '\\' {
                self.advance_char();
                let Some(escaped) = self.peek_char() else {
                    break;
                };
                text.push(match escaped {
                    '"' | '\\' | '/' => escaped,
                    'n' => '\n',
                    't' => '\t',
                    other => other,
                });
                self.advance_char();
                continue;
            }
            text.push(ch);
            self.advance_char();
        }
        self.diagnostics.push(cook_error_at(
            self.source_path.clone(),
            line,
            column,
            "unterminated string literal",
        ));
        MaterialToken {
            kind: MaterialTokenKind::String,
            text,
            line,
            column,
        }
    }

    fn skip_trivia(&mut self) {
        loop {
            while self.peek_char().is_some_and(|ch| ch.is_whitespace()) {
                self.advance_char();
            }
            if self.source[self.position..].starts_with("//")
                || self.source[self.position..].starts_with('#')
            {
                while self.peek_char().is_some_and(|ch| ch != '\n') {
                    self.advance_char();
                }
                continue;
            }
            break;
        }
    }

    fn peek_char(&self) -> Option<char> {
        self.source[self.position..].chars().next()
    }

    fn advance_char(&mut self) {
        let Some(ch) = self.peek_char() else {
            return;
        };
        self.position += ch.len_utf8();
        if ch == '\n' {
            self.line += 1;
            self.column = 1;
        } else {
            self.column += 1;
        }
    }
}

struct MaterialParser<'a> {
    lexer: MaterialLexer<'a>,
    current: MaterialToken,
}

impl<'a> MaterialParser<'a> {
    fn new(source: &'a str, source_path: &Path) -> Self {
        let mut lexer = MaterialLexer::new(source, source_path);
        let current = lexer.next();
        Self { lexer, current }
    }

    fn parse(mut self, fallback_id: &str, source_path: &Path) -> ParsedMaterialSource {
        let mut parsed = ParsedMaterialSource {
            schema_version: 1,
            id: fallback_id.to_string(),
            name: fallback_id.to_string(),
            shading_model: "LitPBR".to_string(),
            blend_mode: "Opaque".to_string(),
            cull_mode: "Back".to_string(),
            surface_profile: "Auto".to_string(),
            receives_shadows: true,
            ..ParsedMaterialSource::default()
        };
        self.expect_identifier("material");
        if self.current.kind == MaterialTokenKind::Identifier {
            parsed.id = self.current.text.clone();
            parsed.name = parsed.id.clone();
            self.advance();
        } else {
            self.add_error_current("expected material identifier");
        }
        self.expect(
            MaterialTokenKind::LeftBrace,
            "expected '{' after material name",
        );
        while self.current.kind != MaterialTokenKind::RightBrace
            && self.current.kind != MaterialTokenKind::End
        {
            self.parse_top_level(&mut parsed);
        }
        self.expect(
            MaterialTokenKind::RightBrace,
            "expected '}' after material body",
        );
        if self.current.kind != MaterialTokenKind::End {
            self.add_error_current("unexpected tokens after material body");
        }
        parsed.diagnostics = self.lexer.diagnostics;
        validate_parsed_material(&mut parsed, source_path);
        parsed
    }

    fn advance(&mut self) {
        self.current = self.lexer.next();
    }

    fn expect(&mut self, kind: MaterialTokenKind, message: &str) -> bool {
        if self.current.kind == kind {
            self.advance();
            true
        } else {
            self.add_error_current(message);
            false
        }
    }

    fn expect_identifier(&mut self, value: &str) -> bool {
        if self.current.kind == MaterialTokenKind::Identifier && self.current.text == value {
            self.advance();
            true
        } else {
            self.add_error_current(format!("expected '{value}'"));
            false
        }
    }

    fn read_value_text(&mut self) -> String {
        if self.current.kind == MaterialTokenKind::Identifier
            || self.current.kind == MaterialTokenKind::String
            || self.current.kind == MaterialTokenKind::Number
        {
            let out = self.current.text.clone();
            self.advance();
            out
        } else {
            self.add_error_current("expected value");
            String::new()
        }
    }

    fn parse_top_level(&mut self, parsed: &mut ParsedMaterialSource) {
        if self.current.kind != MaterialTokenKind::Identifier {
            self.add_error_current("expected material field");
            self.advance();
            return;
        }
        let field = self.current.clone();
        self.advance();
        match field.text.as_str() {
            "textures" => {
                self.parse_texture_block(parsed);
                return;
            }
            "params" => {
                self.parse_param_block(parsed);
                return;
            }
            "features" => {
                self.parse_feature_block(parsed);
                return;
            }
            "provenance" => {
                parsed.provenance = self.parse_string_map_block("provenance");
                return;
            }
            "authoring" => {
                parsed.authoring = self.parse_string_map_block("authoring");
                return;
            }
            "preview" => {
                parsed.preview = self.parse_string_map_block("preview");
                return;
            }
            "quality_profile" => {
                parsed.quality_profile = self.parse_string_map_block("quality_profile");
                return;
            }
            "layers" => {
                self.parse_layer_block(parsed);
                return;
            }
            _ => {}
        }
        self.expect(
            MaterialTokenKind::Colon,
            "expected ':' after material field",
        );
        let value = self.read_value_text();
        match field.text.as_str() {
            "schema_version" => match value.parse::<u32>() {
                Ok(value) => parsed.schema_version = value,
                Err(_) => self.add_error_at(&field, "invalid schema_version value"),
            },
            "name" => parsed.name = value,
            "shading_model" => {
                if matches!(
                    value.as_str(),
                    "LitPBR" | "lit_pbr" | "Unlit" | "unlit" | "Emissive" | "emissive"
                ) {
                    parsed.shading_model = value;
                } else {
                    self.add_error_at(&field, format!("unknown shading_model '{value}'"));
                }
            }
            "surface_profile" => parsed.surface_profile = value,
            "blend_mode" => {
                if matches!(
                    value.as_str(),
                    "Opaque" | "opaque" | "Masked" | "masked" | "AlphaClip" | "Blend" | "blend"
                ) {
                    parsed.blend_mode = if value == "AlphaClip" {
                        "Masked".to_string()
                    } else {
                        value
                    };
                } else {
                    self.add_error_at(&field, format!("unknown blend_mode '{value}'"));
                }
            }
            "cull_mode" => {
                if matches!(
                    value.as_str(),
                    "Back" | "back" | "Front" | "front" | "None" | "none"
                ) {
                    parsed.cull_mode = value;
                } else {
                    self.add_error_at(&field, format!("unknown cull_mode '{value}'"));
                }
            }
            "receives_shadows" => parsed.receives_shadows = value != "false",
            "receives_decals" | "depth_layer" | "depth_bias" | "slope_depth_bias"
            | "normal_offset" => {}
            _ => self.add_error_at(&field, format!("unknown material field '{}'", field.text)),
        }
    }

    fn parse_texture_block(&mut self, parsed: &mut ParsedMaterialSource) {
        self.expect(MaterialTokenKind::LeftBrace, "expected '{' after textures");
        while self.current.kind != MaterialTokenKind::RightBrace
            && self.current.kind != MaterialTokenKind::End
        {
            if self.current.kind != MaterialTokenKind::Identifier {
                self.add_error_current("expected texture slot name");
                self.advance();
                continue;
            }
            let role_token = self.current.clone();
            self.advance();
            self.expect(MaterialTokenKind::Colon, "expected ':' after texture slot");
            let uri = self.read_value_text();
            if let Some(role) = canonical_material_texture_role(&role_token.text) {
                if parsed.textures.contains_key(&role) {
                    self.add_error_at(&role_token, format!("duplicate texture role '{role}'"));
                } else {
                    parsed.textures.insert(role, uri);
                }
            } else {
                self.add_error_at(
                    &role_token,
                    format!("unsupported texture role '{}'", role_token.text),
                );
            }
        }
        self.expect(MaterialTokenKind::RightBrace, "expected '}' after textures");
    }

    fn parse_param_block(&mut self, parsed: &mut ParsedMaterialSource) {
        self.expect(MaterialTokenKind::LeftBrace, "expected '{' after params");
        while self.current.kind != MaterialTokenKind::RightBrace
            && self.current.kind != MaterialTokenKind::End
        {
            if self.current.kind != MaterialTokenKind::Identifier {
                self.add_error_current("expected parameter name");
                self.advance();
                continue;
            }
            let name = self.current.text.clone();
            self.advance();
            self.expect(
                MaterialTokenKind::Colon,
                "expected ':' after parameter name",
            );
            if self.current.kind != MaterialTokenKind::Number {
                self.add_error_current("expected numeric value");
                self.advance();
                continue;
            }
            match self.current.text.parse::<f32>() {
                Ok(value) => {
                    parsed.params.insert(name, value);
                }
                Err(_) => self.add_error_current("invalid numeric value"),
            }
            self.advance();
        }
        self.expect(MaterialTokenKind::RightBrace, "expected '}' after params");
    }

    fn parse_feature_block(&mut self, parsed: &mut ParsedMaterialSource) {
        self.expect(MaterialTokenKind::LeftBrace, "expected '{' after features");
        while self.current.kind != MaterialTokenKind::RightBrace
            && self.current.kind != MaterialTokenKind::End
        {
            if self.current.kind != MaterialTokenKind::Identifier {
                self.add_error_current("expected feature name");
                self.advance();
                continue;
            }
            let name = self.current.text.clone();
            self.advance();
            self.expect(MaterialTokenKind::Colon, "expected ':' after feature name");
            let value = self.read_value_text();
            match value.as_str() {
                "true" => {
                    parsed.features.insert(name, true);
                }
                "false" => {
                    parsed.features.insert(name, false);
                }
                _ => self.add_error_current("expected boolean value"),
            }
        }
        self.expect(MaterialTokenKind::RightBrace, "expected '}' after features");
    }

    fn parse_string_map_block(&mut self, block_name: &str) -> BTreeMap<String, String> {
        let mut values = BTreeMap::new();
        self.expect(
            MaterialTokenKind::LeftBrace,
            &format!("expected '{{' after {block_name}"),
        );
        while self.current.kind != MaterialTokenKind::RightBrace
            && self.current.kind != MaterialTokenKind::End
        {
            if self.current.kind != MaterialTokenKind::Identifier {
                self.add_error_current(format!("expected {block_name} key"));
                self.advance();
                continue;
            }
            let key = self.current.text.clone();
            self.advance();
            self.expect(
                MaterialTokenKind::Colon,
                &format!("expected ':' after {block_name} key"),
            );
            values.insert(key, self.read_value_text());
        }
        self.expect(
            MaterialTokenKind::RightBrace,
            &format!("expected '}}' after {block_name}"),
        );
        values
    }

    fn parse_layer_block(&mut self, parsed: &mut ParsedMaterialSource) {
        self.expect(MaterialTokenKind::LeftBrace, "expected '{' after layers");
        while self.current.kind != MaterialTokenKind::RightBrace
            && self.current.kind != MaterialTokenKind::End
        {
            if self.current.kind != MaterialTokenKind::Identifier {
                self.add_error_current("expected layer name");
                self.advance();
                continue;
            }
            let mut layer = ParsedMaterialLayer {
                name: self.current.text.clone(),
                ..ParsedMaterialLayer::default()
            };
            self.advance();
            self.expect(MaterialTokenKind::Colon, "expected ':' after layer name");
            if self.current.kind == MaterialTokenKind::Identifier {
                layer.operation = self.current.text.clone();
                layer.raw = layer.operation.clone();
                self.advance();
            } else {
                self.add_error_current("expected layer operation");
            }
            self.expect(
                MaterialTokenKind::LeftParen,
                "expected '(' after layer operation",
            );
            layer.raw.push('(');
            while self.current.kind != MaterialTokenKind::RightParen
                && self.current.kind != MaterialTokenKind::End
            {
                if self.current.kind == MaterialTokenKind::Comma {
                    layer.raw.push(',');
                    self.advance();
                    continue;
                }
                let argument = self.read_value_text();
                if !argument.is_empty() {
                    if !layer.arguments.is_empty() && !layer.raw.ends_with(',') {
                        layer.raw.push(',');
                    }
                    layer.raw.push_str(&argument);
                    layer.arguments.push(argument);
                }
            }
            self.expect(
                MaterialTokenKind::RightParen,
                "expected ')' after layer expression",
            );
            layer.raw.push(')');
            parsed.layers.push(layer);
        }
        self.expect(MaterialTokenKind::RightBrace, "expected '}' after layers");
    }

    fn add_error_current(&mut self, message: impl Into<String>) {
        let token = self.current.clone();
        self.add_error_at(&token, message);
    }

    fn add_error_at(&mut self, token: &MaterialToken, message: impl Into<String>) {
        self.lexer.diagnostics.push(cook_error_at(
            self.lexer.source_path.clone(),
            token.line,
            token.column,
            message,
        ));
    }
}

fn parse_astermat_source(
    source: &str,
    fallback_id: &str,
    source_path: &Path,
) -> ParsedMaterialSource {
    MaterialParser::new(source, source_path).parse(fallback_id, source_path)
}

fn canonical_material_texture_role(role: &str) -> Option<String> {
    match role {
        "albedo" | "base_color" | "baseColor" | "diffuse" => Some("albedo".to_string()),
        "normal" => Some("normal".to_string()),
        "orm" => Some("orm".to_string()),
        "roughness" => Some("roughness".to_string()),
        "metallic" => Some("metallic".to_string()),
        "ao" | "occlusion" | "ambient_occlusion" => Some("ao".to_string()),
        "height" | "displacement" => Some("height".to_string()),
        "emissive" => Some("emissive".to_string()),
        "wetness" => Some("wetness".to_string()),
        "opacity" | "alpha" => Some("opacity".to_string()),
        "mask" | "moss" | "crack" => Some("mask".to_string()),
        _ if role.contains("mask") => Some("mask".to_string()),
        _ => None,
    }
}

fn validate_parsed_material(parsed: &mut ParsedMaterialSource, source_path: &Path) {
    if parsed.id.is_empty() {
        parsed
            .diagnostics
            .push(cook_error("material asset is missing an id"));
    }
    for layer in &parsed.layers {
        if !matches!(
            layer.operation.as_str(),
            "triplanar" | "height_blend" | "slope_blend" | "wetness" | "moss"
        ) {
            parsed.diagnostics.push(cook_error(format!(
                "unsupported layer operation '{}' in layer '{}'",
                layer.operation, layer.name
            )));
        }
    }
    if parsed.features.get("parallax").copied().unwrap_or(false)
        && !parsed.textures.contains_key("height")
    {
        parsed.diagnostics.push(cook_warning(format!(
            "material '{}' enables parallax without a height texture; authoring preview will downgrade it",
            parsed.id
        )));
    }
    if parsed
        .authoring
        .get("mapping_policy")
        .is_some_and(|value| value == "uv")
        && parsed.features.get("triplanar") == Some(&true)
    {
        parsed.diagnostics.push(cook_warning(format!(
            "material '{}' declares uv mapping policy while triplanar is enabled",
            parsed.id
        )));
    }
    if parsed
        .quality_profile
        .get("mobile_drop_parallax")
        .is_some_and(|value| value == "true")
        && parsed.features.get("parallax") == Some(&true)
    {
        parsed.diagnostics.push(cook_warning(format!(
            "material '{}' will drop parallax on mobile quality profiles",
            parsed.id
        )));
    }
    if !parsed.shading_model.eq_ignore_ascii_case("LitPBR") {
        return;
    }
    for role in ["albedo", "normal"] {
        if !parsed.textures.contains_key(role) {
            parsed.diagnostics.push(cook_error(format!(
                "LitPBR material '{}' requires '{}' texture",
                parsed.id, role
            )));
        }
    }
    const SPLIT_ORM_ROLES: [&str; 3] = ["roughness", "metallic", "ao"];
    let has_orm = parsed.textures.contains_key("orm");
    let split_count = SPLIT_ORM_ROLES
        .iter()
        .filter(|role| parsed.textures.contains_key(**role))
        .count();
    if !has_orm && split_count == 0 {
        parsed.diagnostics.push(cook_error(format!(
            "LitPBR material '{}' requires 'orm' texture or complete roughness/metallic/ao sources",
            parsed.id
        )));
    } else if !has_orm && split_count != SPLIT_ORM_ROLES.len() {
        let missing = SPLIT_ORM_ROLES
            .iter()
            .filter(|role| !parsed.textures.contains_key(**role))
            .copied()
            .collect::<Vec<_>>()
            .join(", ");
        parsed.diagnostics.push(cook_error(format!(
            "LitPBR material '{}' has incomplete split ORM sources; missing {}",
            parsed.id, missing
        )));
    }
    let _ = source_path;
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
    if parsed.textures.contains_key("orm") || parsed_requires_split_orm_pack(parsed) {
        mask |= 1 << 2;
    }
    if parsed.textures.contains_key("height") {
        mask |= 1 << 3;
    }
    if parsed.features.get("parallax") == Some(&true) {
        mask |= 1 << 4;
    }
    if parsed.features.get("triplanar") == Some(&true)
        || parsed
            .layers
            .iter()
            .any(|layer| layer.operation == "triplanar")
    {
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
    guid: &str,
    kind: &str,
    source_path: &str,
    platform: &str,
    import_preset: AssetImportPresetRecord,
    platform_profile: AssetPlatformProfileRecord,
) -> AssetDatabaseRecord {
    AssetDatabaseRecord {
        guid: guid.to_string(),
        id: id.to_string(),
        kind: kind.to_string(),
        source: AssetSourceRecord {
            path: source_path.to_string(),
            hash: String::new(),
        },
        source_path: source_path.to_string(),
        import_preset,
        platform_profile,
        import_settings_version: ASSET_IMPORT_SETTINGS_VERSION,
        source_hash: String::new(),
        options_hash: hash_hex_text(&format!("{kind}:{}", ASSET_IMPORT_SETTINGS_VERSION)),
        dependency_edges: Vec::new(),
        dependencies: Vec::new(),
        artifacts: Vec::new(),
        outputs: Vec::new(),
        diagnostics: Vec::new(),
        tool_versions: default_tool_versions(),
        derived_hashes: AssetDerivedHashes::default(),
        fate_report: AssetFateReport::default(),
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
        "fbx" | "usd" | "usda" | "usdc" | "usdz" | "blend" => "scene".to_string(),
        "astermat" => "material".to_string(),
        "astergraph" => "asset_graph".to_string(),
        "png" | "ktx2" | "tga" | "jpg" | "jpeg" => "texture".to_string(),
        _ if !declared_kind.is_empty() => declared_kind.to_string(),
        _ => "unknown".to_string(),
    }
}

fn dcc_source_kind(path: &Path) -> Option<&'static str> {
    match path
        .extension()
        .and_then(|value| value.to_str())
        .unwrap_or("")
        .to_ascii_lowercase()
        .as_str()
    {
        "fbx" => Some("fbx"),
        "usd" | "usda" | "usdc" | "usdz" => Some("usd"),
        "blend" => Some("blend"),
        _ => None,
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

pub fn generate_stable_asset_guid(kind: &str, id: &str) -> String {
    let mut hasher = blake3::Hasher::new();
    hasher.update(b"aster.asset.guid.v2\0");
    hasher.update(kind.as_bytes());
    hasher.update(b"\0");
    hasher.update(id.as_bytes());
    hex_hash(hasher.finalize().as_bytes())
}

pub fn write_missing_asset_meta(project: impl AsRef<Path>) -> Result<usize> {
    let project = project.as_ref();
    let root: Value = serde_json::from_slice(&fs::read(project)?)?;
    let project_root = project.parent().unwrap_or_else(|| Path::new(""));
    let mut written = 0usize;
    let Some(assets) = root.get("assets").and_then(Value::as_array) else {
        return Ok(0);
    };
    for asset in assets {
        if value_str(asset, "guid").is_some() {
            continue;
        }
        let id = value_str(asset, "id").unwrap_or("");
        let declared_kind = value_str(asset, "kind").unwrap_or("");
        let Some(source) = value_str(asset, "path") else {
            continue;
        };
        let source_path = project_root.join(source);
        if read_asset_meta_guid(&source_path)?.is_some() {
            continue;
        }
        let kind = canonical_asset_kind(&source_path, declared_kind);
        let mut meta_path = source_path.clone();
        meta_path.set_extension("astermeta");
        if let Some(parent) = meta_path.parent() {
            fs::create_dir_all(parent)?;
        }
        let meta = serde_json::json!({
            "schema_version": 1,
            "guid": generate_stable_asset_guid(&kind, id),
            "id": id,
            "kind": kind,
        });
        write_json(&meta_path, &meta)?;
        written += 1;
    }
    Ok(written)
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
            vec!["error: jpeg header was present but SOF dimensions were not found".to_string()],
        );
    }
    if bytes.len() >= 4 && bytes.starts_with(&[0x76, 0x2f, 0x31, 0x01]) {
        return (
            "exr".to_string(),
            0,
            0,
            1,
            vec!["error: EXR payload detected; full dataWindow decode is deferred".to_string()],
        );
    }
    (
        "unknown".to_string(),
        0,
        0,
        1,
        vec!["error: unsupported texture header".to_string()],
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
            w: transform_tangent_handedness(matrix, vertex.tangent.w),
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

fn transform_tangent_handedness(matrix: Mat4, handedness: f32) -> f32 {
    if linear_determinant(matrix) < 0.0 {
        -handedness
    } else {
        handedness
    }
}

fn linear_determinant(matrix: Mat4) -> f32 {
    let at = |row: usize, column: usize| matrix.m[column * 4 + row];
    at(0, 0) * (at(1, 1) * at(2, 2) - at(1, 2) * at(2, 1))
        - at(0, 1) * (at(1, 0) * at(2, 2) - at(1, 2) * at(2, 0))
        + at(0, 2) * (at(1, 0) * at(2, 1) - at(1, 1) * at(2, 0))
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

fn write_geometry_audit_chunk(asset: &CompiledSceneAsset) -> Result<Vec<u8>> {
    let report = serde_json::json!({
        "schema_version": 1,
        "kind": "geometry-audit",
        "mesh_count": asset.metadata.mesh_count,
        "collision_mesh_count": asset.metadata.collision_mesh_count,
        "total_vertices": asset.metadata.total_vertices,
        "total_indices": asset.metadata.total_indices,
        "total_collision_triangles": asset.metadata.total_collision_triangles,
        "diagnostics": {
            "input_vertices": asset.metadata.diagnostics.input_vertices,
            "input_indices": asset.metadata.diagnostics.input_indices,
            "output_vertices": asset.metadata.diagnostics.output_vertices,
            "output_indices": asset.metadata.diagnostics.output_indices,
            "degenerate_triangles": asset.metadata.diagnostics.degenerate_triangles,
            "invalid_normals": asset.metadata.diagnostics.invalid_normals,
            "generated_tangents": asset.metadata.diagnostics.generated_tangents,
            "remapped_vertices": asset.metadata.diagnostics.remapped_vertices,
        },
        "meshlet_policy": "deterministic-64-126-audit",
        "lod_policy": "audit-only",
        "tangent_space": "mikk-compatible-contract",
        "lightmap_uv_policy": "audit-only",
        "skeleton_policy": "import-and-validate",
        "animation_policy": "import-clips",
        "morph_policy": "preserve-and-validate"
    });
    Ok(serde_json::to_vec(&report)?)
}

fn write_empty_named_chunk(label: &str) -> Result<Vec<u8>> {
    let report = serde_json::json!({
        "schema_version": 1,
        "label": label,
        "items": []
    });
    Ok(serde_json::to_vec(&report)?)
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

    fn assert_near(actual: f32, expected: f32) {
        assert!(
            (actual - expected).abs() < 0.0001,
            "actual {actual} expected {expected}"
        );
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
        let texture = dir.join("albedo.ktx2");
        write_ktx2_header(&texture, 32, 16, 6, 43);

        let summary = inspect_texture(&texture, "albedo").expect("inspect");
        assert_eq!(summary.kind, TextureImportKind::Albedo);
        assert_eq!(summary.color_space, "srgb");
        assert_eq!(summary.width, 32);
        assert_eq!(summary.height, 16);
        assert_eq!(summary.mip_count, texture_mip_count(32, 16));

        let baked = dir.join("cooked_albedo.ktx2");
        let baked_summary = bake_texture_to_ktx2(&texture, &baked, "albedo").expect("bake");
        assert_eq!(baked_summary.source_hash, summary.source_hash);
        let baked_bytes = fs::read(&baked).expect("baked bytes");
        assert!(baked_bytes.starts_with(&[0xab, b'K', b'T', b'X', b' ', b'2', b'0', 0xbb]));
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

    fn write_ktx2_header(path: &Path, width: u32, height: u32, mip_count: u32, vk_format: u32) {
        let mut ktx2 = Vec::new();
        ktx2.extend_from_slice(&[0xab, b'K', b'T', b'X', b' ', b'2', b'0', 0xbb]);
        ktx2.extend_from_slice(&[b'\r', b'\n', 0x1a, b'\n']);
        ktx2.extend_from_slice(&vk_format.to_le_bytes());
        ktx2.extend_from_slice(&1u32.to_le_bytes());
        ktx2.extend_from_slice(&width.to_le_bytes());
        ktx2.extend_from_slice(&height.to_le_bytes());
        ktx2.extend_from_slice(&0u32.to_le_bytes());
        ktx2.extend_from_slice(&0u32.to_le_bytes());
        ktx2.extend_from_slice(&1u32.to_le_bytes());
        ktx2.extend_from_slice(&mip_count.to_le_bytes());
        ktx2.extend_from_slice(&1u32.to_le_bytes());
        ktx2.extend_from_slice(b"ASTER_TEST_KTX2_PAYLOAD");
        fs::write(path, ktx2).expect("ktx2");
    }

    fn write_material_project(name: &str, missing_texture: bool) -> PathBuf {
        let dir = fixture_dir(name);
        fs::create_dir_all(dir.join("materials")).expect("materials dir");
        fs::create_dir_all(dir.join("textures")).expect("textures dir");
        if !missing_texture {
            write_ktx2_header(&dir.join("textures/wet_albedo.ktx2"), 16, 8, 4, 43);
            write_ktx2_header(&dir.join("textures/wet_normal.ktx2"), 16, 8, 4, 37);
            write_ktx2_header(&dir.join("textures/wet_orm.ktx2"), 16, 8, 4, 37);
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

  provenance {
    source: "procedural-wet-rock"
    generator: "aster-test"
  }

  authoring {
    texel_density: 2.7
    mapping_policy: triplanar
  }

  preview {
    rig: "normalized-three-point"
    exposure: 1.0
  }

  quality_profile {
    mobile_drop_parallax: true
    low_texture_scale: 0.5
  }

  textures {
    albedo: "../textures/wet_albedo.ktx2"
    normal: "../textures/wet_normal.ktx2"
    orm: "../textures/wet_orm.ktx2"
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
  "schema_version": 2,
  "name": "Material Cook Test",
  "assets": [
    {
      "id": "material.wet_rock",
      "guid": "asset-v2-wet-rock-00000000000000000001",
      "kind": "material",
      "path": "materials/wet_rock.astermat",
      "import_preset": "default"
    }
  ]
}
"#,
        )
        .expect("project");
        dir.join("project.asterproj")
    }

    fn write_cave_project(name: &str, blocked_route: bool) -> PathBuf {
        let dir = fixture_dir(name);
        fs::create_dir_all(dir.join("caves")).expect("caves dir");
        let max_segment = if blocked_route { 1.0 } else { 12.0 };
        fs::write(
            dir.join("caves/test.cave"),
            format!(
                r#"{{
  "schema_version": 1,
  "id": "cave.test",
  "name": "Generated Gate Cave",
  "seeds": [{{ "id": "gate", "value": 17 }}],
  "sections": [
    {{
      "id": "entry",
      "archetype": "prefab.cave_segment",
      "ore": {{ "max_nodes": 4 }},
      "fixtures": [
        {{ "id": "left", "max_count": 2 }},
        {{ "id": "right", "max_count": 2 }}
      ]
    }}
  ],
  "placements": [
    {{ "id": "spawn", "kind": "enemy_spawn", "section": "entry", "archetype": "prefab.cave_skitter_spawn" }}
  ],
  "validation": {{
    "walkable_routes": [
      {{ "id": "entry_route", "points": [[0.0, 0.0, 0.0], [0.0, 0.0, -10.0]], "max_segment_length": {max_segment}, "support_tolerance": 0.25 }}
    ],
    "spawn_volumes": [
      {{ "id": "player_spawn", "center": [0.0, 0.0, 0.0], "half_extents": [0.2, 0.2, 0.2] }}
    ],
    "collision_volumes": [
      {{ "id": "wall_contact", "center": [2.0, 0.0, -4.0], "half_extents": [0.2, 0.8, 0.2] }}
    ],
    "probe_agent": {{ "id": "gate_probe", "seed": 17, "step_count": 8, "step_length": 1.25 }},
    "resource_probes": [
      {{ "id": "ore_probe", "kind": "resource", "position": [0.0, 0.0, -4.0], "radius": 4.0, "minimum_count": 2 }}
    ],
    "encounter_probes": [
      {{ "id": "encounter_probe", "kind": "enemy_spawn", "position": [0.0, 0.0, -8.0], "radius": 3.0, "minimum_budget": 0.10, "maximum_budget": 0.50 }}
    ],
    "perceptual_budget": {{ "id": "readability", "minimum_salience": 0.55 }},
    "perceptual_continuity_budget": {{
      "id": "entry_world_reaction",
      "minimum_score": 0.62,
      "required_channels": [
        "spatial_affordance",
        "hazard_readability",
        "material_memory",
        "lighting_atmosphere",
        "event_residue",
        "sensory_feedback",
        "ai_attention",
        "streaming_residency"
      ],
      "reaction_packages": [
        {{
          "id": "coal_mining_reaction",
          "action": "action.mine.coal_ore",
          "minimum_score": 0.68,
          "required_channels": [
            "material_memory",
            "event_residue",
            "sensory_feedback",
            "resource_state",
            "ai_attention",
            "ui_feedback"
          ]
        }}
      ]
    }},
    "perception_ledger": {{
      "id": "entry_sensory_state_graph",
      "minimum_score": 0.78,
      "required_channels": [
        "material_memory",
        "contact_history",
        "lighting_exposure",
        "atmosphere_cell",
        "occlusion_role",
        "gameplay_affordance",
        "wear_continuity",
        "streaming_semantic_lod",
        "audio_visual_cue_budget"
      ],
      "cells": [
        {{
          "id": "entry",
          "minimum_score": 0.78,
          "required_channels": [
            "material_memory",
            "contact_history",
            "lighting_exposure",
            "atmosphere_cell",
            "occlusion_role",
            "gameplay_affordance",
            "wear_continuity",
            "streaming_semantic_lod",
            "audio_visual_cue_budget"
          ]
        }}
      ]
    }},
    "perceptual_runtime": {{
      "id": "entry_perceptual_world_runtime",
      "exposure_horizon_seconds": 47.0,
      "minimum_continuity_score": 0.62,
      "minimum_occlusion_trust": 0.45,
      "minimum_lighting_believability": 0.45,
      "minimum_player_readable_cause": 0.45
    }}
  }}
}}
"#
            ),
        )
        .expect("cave");
        fs::write(
            dir.join("project.asterproj"),
            r#"{
  "schema_version": 2,
  "name": "Cave Gate Cook Test",
  "assets": [
    {
      "id": "cave.test",
      "guid": "asset-v2-cave-gate-0000000000000001",
      "kind": "cave",
      "path": "caves/test.cave",
      "import_preset": "default"
    }
  ]
}
"#,
        )
        .expect("project");
        dir.join("project.asterproj")
    }

    fn write_asset_graph_project(name: &str) -> PathBuf {
        let dir = fixture_dir(name);
        fs::create_dir_all(dir.join("graphs")).expect("graphs dir");
        fs::write(
            dir.join("graphs/wet_rock.astergraph"),
            r#"astergraph asset_graph.wet_rock
schema_version 1
name "Procedural Wet Rock"
material_id material.graph_wet_rock
surface_profile stratified-rock
primitive rock
uv_policy triplanar
tangent_policy validate-or-generate
collision_proxy convex-hull
lod_policy single-lod
base_color 0.19 0.17 0.145
roughness 0.78
metallic 0.0
param wetness 0.52
param macro_variation 0.42
param micro_normal_strength 0.50
param roughness_variation 0.16
param height_shading 0.28
feature triplanar true
feature normal_map true
feature parallax true
preview environment cave-dark
node mesh.surface mesh_primitive role=mesh primitive=rock
node uv.triplanar uv_policy role=uv mapping=triplanar texel_density=2.7
node tangent.validate tangent_validation role=tangent policy=validate-or-generate
node material.assign material_assignment role=material surface_profile=stratified-rock
node mat.noise noise role=base_color scale=3.2 strength=0.30
node mat.cells cellular role=roughness scale=7.0 strength=0.16
node mat.slope slope_mask role=mask threshold=0.42
node mat.cavity cavity_dirt role=ao strength=0.34
node mat.wet wetness_flow role=wetness strength=0.52
node mat.rust rust_spread role=oxidation strength=0.0
node mat.moss moss_growth role=moss strength=0.10
node mat.decal decal_layer role=decal opacity=0.18
node mat.orm orm_baker role=orm policy=runtime-procedural
node mat.normal normal_baker role=normal strength=0.50
node mat.height height_baker role=height strength=0.28
node collision.proxy collision_proxy role=collision shape=convex-hull
node lod.single lod_generator role=lod policy=single-lod
node probe.preview probe_helper role=lighting environment=cave-dark
node prefab.variant prefab_variant role=prefab variant=material-lab
node perception.template world_perceptual_template role=perceptual profile=stratified-rock surface_response=wet-rock history_response=wetness-contact-memory material_half_life_seconds=18 wetness_half_life_seconds=9 semantic_lod=0.74 streaming_cost=0.24 patch_channels=wetness,exposure,material_stability contact_channels=contact_normal,visual_occlusion,traversal_affordance residue_channels=interaction_residue,acoustic_occlusion,player_readable_cause
node export.runtime cook_export role=package target=assetgraphbin
node diagnostic.quality diagnostic role=quality profile=production
edge mat.noise material.assign base_color
edge mat.wet material.assign wetness
edge perception.template export.runtime perceptual_template
"#,
        )
        .expect("astergraph");
        fs::write(
            dir.join("project.asterproj"),
            r#"{
  "schema_version": 2,
  "name": "Asset Graph Cook Test",
  "assets": [
    {
      "id": "asset_graph.wet_rock",
      "guid": "asset-v2-graph-wet-rock-000000000001",
      "kind": "asset_graph",
      "path": "graphs/wet_rock.astergraph",
      "import_preset": "default"
    }
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
        assert_eq!(db.schema_version, ASSET_DATABASE_SCHEMA_VERSION);
        assert_eq!(db.asset_graph.nodes.len(), 1);
        assert_eq!(db.fate_reports.len(), 1);
        assert_eq!(db.records[0].kind, "material");
        assert!(db.records[0].derived_hashes.material_hash.len() > 8);
        assert!(db.records[0]
            .derived_hashes
            .shader_variant_key
            .starts_with("0x"));
        assert!(db.records[0]
            .fate_report
            .render_contract
            .iter()
            .any(|entry| entry.starts_with("shader-variant:")));
        assert!(db.records[0].fate_report.world_ready.accepted);
        assert!(!db.records[0].fate_report.world_ready.report_hash.is_empty());
        assert!(db.records[0]
            .fate_report
            .world_ready
            .readiness_signals
            .iter()
            .any(|signal| signal == "topology"));
        assert!(db.records[0]
            .fate_report
            .world_ready
            .readiness_signals
            .iter()
            .any(|signal| signal == "wetness_propagation"));
        assert!(db.records[0]
            .fate_report
            .world_ready
            .readiness_signals
            .iter()
            .any(|signal| signal == "perceptual_stability"));
        assert!(db.records[0]
            .fate_report
            .render_contract
            .iter()
            .any(|entry| entry.starts_with("world-ready:")));
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
        assert_eq!(material_bin.textures.len(), 3);
        assert_eq!(material_bin.textures[0].runtime_format, "ktx2");
        assert!(material_bin.textures[0].byte_cost > 0);
        assert_eq!(material_bin.textures[0].encoder, "passthrough-ktx2");
        assert!(material_bin.feature_mask & (1 << 0) != 0);
        assert!(material_bin.shader_variant_key != 0);
        assert_eq!(
            material_bin.provenance.get("generator").map(String::as_str),
            Some("aster-test")
        );
        assert_eq!(
            material_bin
                .authoring
                .get("mapping_policy")
                .map(String::as_str),
            Some("triplanar")
        );
        assert_eq!(
            material_bin
                .quality_profile
                .get("mobile_drop_parallax")
                .map(String::as_str),
            Some("true")
        );
        assert_eq!(
            material_bin.derived_hashes.shader_variant_key,
            db.records[0].derived_hashes.shader_variant_key
        );
        assert!(report_asset_database(&db).contains("assets=1"));
        assert!(asset_graph_report_json(&db)
            .expect("graph json")
            .contains("project_fingerprint"));
        assert!(asset_fate_report_json(&db, "material.wet_rock")
            .expect("fate json")
            .contains("material-hash"));
        assert!(asset_database_diff_json(&db, &db)
            .expect("diff json")
            .contains("\"changed\": []"));
        fs::remove_dir_all(project.parent().unwrap()).ok();
    }

    #[test]
    fn cave_cook_emits_world_gate_report_and_rejects_blocked_routes() {
        let project = write_cave_project("cave_world_gate_project", false);
        let output = project.parent().unwrap().join("cooked/desktop");
        let result = cook_project(&project, "desktop", &output).expect("cook cave");
        assert_eq!(result.error_count, 0);
        let db = read_asset_database(&result.database_path).expect("read cave db");
        let record = &db.records[0];
        assert_eq!(record.kind, "cave");
        let report_output = record
            .outputs
            .iter()
            .find(|output| output.role == "world-gate-report")
            .expect("world gate report");
        let report: Value =
            serde_json::from_slice(&fs::read(output.join(&report_output.path)).expect("report"))
                .expect("world gate json");
        let neural_output = record
            .outputs
            .iter()
            .find(|output| output.role == "neural-light-field-report")
            .expect("neural light field report");
        let neural_report: Value = serde_json::from_slice(
            &fs::read(output.join(&neural_output.path)).expect("neural report"),
        )
        .expect("neural light field json");
        assert_eq!(report["kind"], "cave_world_gate_report");
        assert_eq!(report["verdict"], "accepted");
        assert_eq!(
            report["neural_light_field"]["kind"],
            "neural_light_field_report"
        );
        assert_eq!(report["neural_light_field"]["accepted"], true);
        assert_eq!(
            report["neural_light_field"]["model"],
            "aster.deterministic_mlp_feature_grid.v1"
        );
        assert!(
            report["neural_light_field"]["model_hash"]
                .as_str()
                .expect("neural model hash")
                .len()
                >= 16
        );
        assert!(
            report["neural_light_field"]["fixture_count"]
                .as_u64()
                .expect("fixture count")
                > 0
        );
        assert!(
            report["neural_light_field"]["torch_socket_count"]
                .as_u64()
                .expect("torch sockets")
                >= report["neural_light_field"]["fixture_count"]
                    .as_u64()
                    .expect("fixture count")
        );
        assert_eq!(
            report["neural_light_field"]["linked_world_gate_hash"],
            report["probe_trace_hash"]
        );
        assert_eq!(neural_report["kind"], "cave_neural_light_field_artifact");
        assert_eq!(
            neural_report["neural_light_field"]["model_hash"],
            report["neural_light_field"]["model_hash"]
        );
        assert_eq!(
            neural_report["neural_light_field"]["linked_ledger_hash"],
            report["perception_ledger"]["ledger_hash"]
        );
        assert_eq!(report["navigation"]["valid"], true);
        assert_eq!(report["perceptual_budget"]["accepted"], true);
        assert_eq!(report["perception_ledger"]["accepted"], true);
        assert_eq!(report["perception_ledger"]["missing_channel_mask"], 0);
        assert!(
            report["perception_ledger"]["ledger_hash"]
                .as_str()
                .expect("ledger hash")
                .len()
                >= 16
        );
        assert_eq!(report["perception_ledger"]["cell_count"], 1);
        assert_eq!(report["perceptual_runtime"]["accepted"], true);
        assert_eq!(
            report["perceptual_runtime"]["id"],
            "entry_perceptual_world_runtime"
        );
        assert_eq!(
            report["perceptual_runtime"]["exposure_horizon_seconds"],
            47.0
        );
        assert!(
            report["perceptual_runtime"]["perceptual_state_hash"]
                .as_str()
                .expect("perceptual state hash")
                .len()
                >= 16
        );
        assert!(
            report["perceptual_runtime"]["semantic_budget_hash"]
                .as_str()
                .expect("semantic budget hash")
                .len()
                >= 16
        );
        assert!(
            report["perceptual_runtime"]["occlusion_trust"]
                .as_f64()
                .expect("occlusion trust")
                >= 0.45
        );
        assert_eq!(report["perceptual_world_scheduler"]["accepted"], true);
        assert!(
            report["perceptual_world_scheduler"]["scheduler_hash"]
                .as_str()
                .expect("scheduler hash")
                .len()
                >= 16
        );
        assert!(
            report["perceptual_world_scheduler"]["memory_residue"]
                .as_f64()
                .expect("memory residue")
                > 0.0
        );
        assert!(
            report["perceptual_world_scheduler"]["threat_signal"]
                .as_f64()
                .expect("threat signal")
                > 0.0
        );
        assert!(
            report["perceptual_world_scheduler"]["material_age"]
                .as_f64()
                .expect("material age")
                > 0.0
        );
        assert!(
            report["perceptual_world_scheduler"]["streaming_budget"]
                .as_f64()
                .expect("streaming budget")
                > 0.0
        );
        assert!(
            report["decision_impact"]["score"]
                .as_f64()
                .expect("decision impact")
                > 0.0
        );
        assert_eq!(report["belief_contract"]["accepted"], true);
        assert_eq!(report["belief_contract"]["minimum_score"], 0.70);
        assert!(
            report["belief_contract"]["score"]
                .as_f64()
                .expect("belief score")
                >= 0.70
        );
        assert!(
            report["belief_contract"]["belief_contract_hash"]
                .as_str()
                .expect("belief hash")
                .len()
                >= 16
        );
        assert_eq!(
            report["falseness_report"]["kind"],
            "belief_falseness_report"
        );
        assert_eq!(report["falseness_report"]["accepted"], true);
        assert_eq!(
            report["falseness_report"]["world_transition_hash"],
            report["world_transition_hash"]
        );
        assert_eq!(
            report["falseness_report"]["extraction_hash"],
            report["extraction_hash"]
        );
        assert_eq!(
            report["falseness_report"]["perceptual_scheduler_hash"],
            report["perceptual_world_scheduler"]["scheduler_hash"]
        );
        assert_eq!(
            report["falseness_report"]["backend_visual_truth"]["score"],
            1.0
        );
        assert!(report["falseness_report"]["findings"]
            .as_array()
            .expect("belief findings")
            .is_empty());
        assert_eq!(report["perceptual_continuity_budget"]["accepted"], true);
        assert_eq!(
            report["perceptual_continuity_budget"]["missing_channel_mask"],
            0
        );
        assert!(report["perceptual_continuity_budget"]["reaction_packages"]
            .as_array()
            .expect("reaction packages")
            .iter()
            .any(|package| package["id"] == "coal_mining_reaction" && package["accepted"] == true));
        fs::remove_dir_all(project.parent().unwrap()).ok();

        let blocked_project = write_cave_project("cave_world_gate_blocked_project", true);
        let blocked_output = blocked_project.parent().unwrap().join("cooked/desktop");
        let blocked =
            cook_project(&blocked_project, "desktop", &blocked_output).expect("cook blocked cave");
        assert!(blocked.error_count > 0);
        let blocked_db = read_asset_database(&blocked.database_path).expect("read blocked db");
        let blocked_report_output = blocked_db.records[0]
            .outputs
            .iter()
            .find(|output| output.role == "world-gate-report")
            .expect("blocked world gate report");
        let blocked_report: Value = serde_json::from_slice(
            &fs::read(blocked_output.join(&blocked_report_output.path)).expect("blocked report"),
        )
        .expect("blocked report json");
        assert_eq!(blocked_report["verdict"], "quarantined");
        assert_eq!(blocked_report["navigation"]["valid"], false);
        assert_eq!(
            blocked_report["perceptual_continuity_budget"]["accepted"],
            false
        );
        assert_eq!(blocked_report["perception_ledger"]["accepted"], false);
        assert_eq!(blocked_report["perceptual_runtime"]["accepted"], false);
        assert_eq!(
            blocked_report["perceptual_world_scheduler"]["accepted"],
            false
        );
        assert_eq!(blocked_report["belief_contract"]["accepted"], false);
        assert_eq!(blocked_report["falseness_report"]["accepted"], false);
        assert!(blocked_report["falseness_report"]["findings"]
            .as_array()
            .expect("blocked belief findings")
            .iter()
            .any(|finding| {
                finding["kind"] == "contextual_grounding_failure"
                    || finding["kind"] == "lod_transition_visibility"
                    || finding["kind"] == "asset_scale_incoherence"
            }));
        assert_ne!(
            blocked_report["perception_ledger"]["missing_channel_mask"],
            0
        );
        assert_ne!(
            blocked_report["perceptual_continuity_budget"]["missing_channel_mask"],
            0
        );
        fs::remove_dir_all(blocked_project.parent().unwrap()).ok();
    }

    #[test]
    fn cooks_project_database_and_assetgraphbin() {
        let project = write_asset_graph_project("cook_asset_graph");
        let graph_path = project.parent().unwrap().join("graphs/wet_rock.astergraph");
        let inspect = asset_graph_inspect_report_json(&graph_path).expect("inspect graph");
        assert!(inspect.contains("\"runtime_model\": \"runtime-procedural\""));
        let package_output = project.parent().unwrap().join("package");
        let packaged = package_asset_graph(&graph_path, &package_output).expect("package graph");
        assert!(packaged.graph_bin_path.exists());
        assert!(packaged.graph_bin.quality.production_ready);
        assert!(packaged.graph_bin.nodes.len() >= 16);

        let output = project.parent().unwrap().join("cooked/desktop");
        let result = cook_project(&project, "desktop", &output).expect("cook graph");
        assert_eq!(result.error_count, 0);
        assert_eq!(result.database.records.len(), 1);
        let record = &result.database.records[0];
        assert_eq!(record.kind, "asset_graph");
        assert!(record
            .outputs
            .iter()
            .any(|output| output.kind == "assetgraphbin"));
        assert!(record
            .fate_report
            .render_contract
            .iter()
            .any(|entry| entry == "runtime-procedural"));
        assert!(record.derived_hashes.shader_variant_key.starts_with("0x"));
        assert!(result
            .database
            .asset_graph
            .edges
            .iter()
            .any(|edge| edge.role == "wetness"));
        fs::remove_dir_all(project.parent().unwrap()).ok();
    }

    #[test]
    fn asset_graph_accepts_geometry_node_breadth_aliases() {
        let dir = fixture_dir("asset_graph_geometry_node_breadth");
        fs::create_dir_all(&dir).expect("dir");
        let graph = dir.join("geometry_nodes.astergraph");
        fs::write(
            &graph,
            r#"astergraph asset_graph.geometry_nodes
schema_version 1
name "Geometry Node Breadth"
material_id material.geometry_nodes
surface_profile stratified-rock
primitive box
uv_policy packed-uv0
tangent_policy validate-or-generate
collision_proxy bounds
lod_policy single-lod
base_color 0.42 0.44 0.40
roughness 0.66
metallic 0.03
param roughness_variation 0.32
feature normal_map true
preview rig inspection-geometry
node mesh.cube mesh_primitive_cube role=mesh size_x=1.2 size_y=0.8 size_z=0.5
node mesh.grid mesh_primitive_grid role=mesh width=2.0 depth=1.5 columns=3 rows=2
node mesh.sphere mesh_primitive_uv_sphere role=mesh segments=12 rings=6 radius=0.7
node mesh.cylinder mesh_primitive_cylinder role=mesh vertices=16 radius=0.35 depth=1.1
node mesh.cone mesh_primitive_cone role=mesh vertices=16 radius_bottom=0.45 radius_top=0.05 depth=1.0
node curve.line curve_primitive_line role=mesh start_x=-0.5 start_y=0 start_z=0 end_x=0.5 end_y=0 end_z=0 width=0.04
node geom.transform transform_geometry role=operator translate_x=0.25 scale_y=1.2
node geom.join join_geometry role=operator copies=2 spacing=0.75
node geom.separate separate_geometry role=operator
node points.from_mesh mesh_to_points role=points
node points.scatter distribute_points_on_faces role=points count=5 seed=11 radius=0.35
node geom.instance instance_on_points role=operator
node geom.bounds bounding_box role=operator
node geom.triangulate triangulate role=operator
node geom.extrude extrude_mesh role=operator amount=0.03
node geom.merge merge_by_distance role=operator distance=0.0001
node geom.flip flip_faces role=operator
node geom.uv uv_pack_islands role=uv padding=0.03
node material.assign material_assignment role=material
node material.set set_material role=material material=material.geometry_nodes
node material.index set_material_index role=material index=2
node policy.uv uv_policy role=uv mapping=packed-uv0
node policy.tangent tangent_validation role=tangent policy=validate-or-generate
node policy.collision collision_proxy role=collision shape=bounds
node policy.lod lod_generator role=lod policy=single-lod
node descriptor.field field_average role=field
node descriptor.grid sdf_grid_boolean role=grid
node descriptor.volume volume_to_mesh role=volume
node descriptor.raycast raycast role=sample
node descriptor.subdivision subdivision_surface role=mesh
node descriptor.convex convex_hull role=mesh
node descriptor.gizmo gizmo_transform role=tool
node perception.template world_perceptual_template role=perceptual profile=stratified-rock surface_response=geometry history_response=authoring material_half_life_seconds=12 wetness_half_life_seconds=8 semantic_lod=0.7 streaming_cost=0.2 patch_channels=geometry,material_stability contact_channels=contact_normal,visual_occlusion residue_channels=interaction_residue,player_readable_cause
node export.runtime cook_export role=package
node diagnostic.quality diagnostic role=quality
edge mesh.cube geom.transform mesh
edge geom.transform geom.join mesh
edge geom.join points.scatter mesh
edge points.scatter geom.instance points
edge geom.instance geom.bounds mesh
edge geom.bounds geom.triangulate mesh
edge geom.triangulate geom.uv uv
"#,
        )
        .expect("geometry graph");
        let packaged = package_asset_graph(&graph, dir.join("package")).expect("package graph");
        assert!(packaged.graph_bin.quality.production_ready);
        assert!(packaged.graph_bin.material.feature_mask & (1 << 1) != 0);
        assert!(packaged.graph_bin.material.feature_mask & (1 << 15) != 0);
        assert!(packaged
            .graph_bin
            .nodes
            .iter()
            .all(|node| node.capability_status != "unsupported"));
        assert!(packaged.graph_bin.nodes.iter().any(|node| {
            node.kind == "mesh_primitive_cube"
                && node.capability_status == "runtime-procedural-reference"
        }));
        assert!(packaged.graph_bin.nodes.iter().any(|node| {
            node.kind == "sdf_grid_boolean" && node.capability_status == "descriptor-only-reference"
        }));
        assert!(packaged.graph_bin.nodes.iter().any(|node| {
            node.kind == "gizmo_transform" && node.capability_status == "descriptor-only-reference"
        }));
        fs::remove_dir_all(dir).ok();
    }

    #[test]
    fn pipe_asset_graph_reports_surface_realism_nodes() {
        let dir = fixture_dir("pipe_asset_graph_realism");
        fs::create_dir_all(&dir).expect("dir");
        let graph = dir.join("rusted_pipe.astergraph");
        fs::write(
            &graph,
            r#"astergraph asset_graph.pipe_test
schema_version 1
name "Pipe Test"
material_id material.pipe_test
surface_profile corroded-metal
primitive rusted-pipe
uv_policy pipe-triplanar-uv-islands
tangent_policy validate-or-generate
collision_proxy pipe-runtime-bounds
lod_policy lod0-lod1-lod2
base_color 0.18 0.17 0.16
roughness 0.78
metallic 0.82
param wetness 0.24
param macro_variation 0.72
param micro_normal_strength 0.58
param roughness_variation 0.74
param physical_texel_density 1024
param height_normal_coupling 0.92
param roughness_height_coupling 0.74
param macro_frequency_breakup 0.48
param micro_frequency_breakup 0.66
param height_shading 0.46
param pitting_density 1.10
param pitting_depth 0.020
param oxide_layering 0.82
param cavity_grime 0.68
param edge_polish 0.34
param weld_heat_tint 0.46
param axial_scratches 0.62
param wet_streaks 7
param rust_bloom 0.82
param black_scab 0.70
param paint_remnant 0.18
param weld_slag 0.84
param rim_soot 0.76
param attachment_clearance 0.0065
param weld_contact_skirt_width 0.040
param seam_inset_depth 0.004
param rim_normal_feather 0.78
feature triplanar true
feature normal_map true
preview rig pipe-lab-three-quarter
metadata owner aster-headless-foundry
node mesh.pipe pipe_body role=mesh primitive=rusted-pipe
socket mesh.pipe Geometry geometry direction=output role=mesh
zone factory.zone repeat input=factory.recipe output=export.runtime items=mesh.pipe,factory.surface
bundle_item factory.bundle surface_signal string source=factory.signal.rust
bake_target bake.preview export.runtime target=preview artifact=proof frame_start=0 frame_end=0
proof_artifact proof.pipe graph tests/artifacts/asset_foundry_headless/industrial_conduit_graph.png kind=png width=640 height=300 signals=rust,oxide,weld
node factory.recipe factory_recipe role=factory target=rusted-pipe variant=reference-silhouette owner=aster
node factory.stage.source factory_stage role=factory kind=source-geometry order=0
node factory.stage.modifiers factory_stage role=factory kind=modifier-stack order=1
node factory.stage.surface factory_stage role=factory kind=surface-contract order=2
node factory.stage.lod factory_stage role=factory kind=lod-recipe order=3
node factory.stage.physics factory_stage role=factory kind=physics-proxy order=4
node factory.stage.quality factory_stage role=factory kind=quality-gate order=5
node factory.surface surface_contract role=factory signal=corroded_orange_brown_rust texel_density=1024 height_normal_coupling=0.92 roughness_height_coupling=0.74
node factory.physics physics_proxy role=factory shape=pipe-runtime-bounds triangles=48 material=corroded-wet-metal
node factory.lod lod_recipe role=factory levels=3 policy=lod0-lod1-lod2
node factory.signal.rust quality_signal role=factory signal=corroded_orange_brown_rust threshold=0.82
node factory.signal.oxide quality_signal role=factory signal=dark_oxide_cavities threshold=0.68
node factory.signal.rim quality_signal role=factory signal=open_hollow_rims threshold=0.76
node factory.claim.rust visual_brief_claim role=factory signal=corroded_orange_brown_rust
node factory.claim.oxide visual_brief_claim role=factory signal=dark_oxide_cavities
node factory.claim.weld visual_brief_claim role=factory signal=raised_weld_rings
node factory.claim.rim visual_brief_claim role=factory signal=open_hollow_rims
node factory.claim.pitting visual_brief_claim role=factory signal=uneven_pitting
node factory.claim.scratches visual_brief_claim role=factory signal=axial_scratches
node factory.claim.silhouette visual_brief_claim role=factory signal=reference_silhouette
node modifier.bevel bevel_modifier role=modifier
node modifier.soft_rim soft_rim_normals role=modifier
node seam.pipe weld_seam role=geometry
node seam.inset seam_inset role=geometry
node seam.contact contact_skirt role=geometry
node render.depth_bias depth_bias_policy role=render
node material.assign material_assignment role=material
node uv.pipe uv_policy role=uv mapping=pipe-triplanar-uv-islands
node tangent.validate tangent_validation role=tangent policy=validate-or-generate
node mask.rust rust_mask role=material
node mask.rust_bloom rust_bloom role=material
node mask.pits voronoi_pitting role=material
node mask.layer_stack layered_corrosion role=material
node mask.oxide oxide_layer role=material
node mask.black_scab black_scab role=material
node mask.paint paint_remnant role=material
node mask.cavity cavity_occlusion role=material
node mask.edge edge_angle_wear role=material
node mask.wet wet_film role=material
node mask.flow wetness_flow role=material
node mask.rim_soot rim_soot role=material
node shade.weld_scorch weld_scorch role=material
node shade.weld_slag weld_slag role=material
node shade.roughmetal roughness_metalness_split role=material
node normal.scratches axial_scratch role=normal
node normal.weld_displacement weld_bead_displacement role=normal
node normal.pipe normal_height role=material
node collision.proxy collision_proxy role=collision shape=pipe-runtime-bounds
node lod.chain lod_generator role=lod policy=lod0-lod1-lod2
node perception.template world_perceptual_template role=perceptual profile=corroded-metal surface_response=rusted-pipe history_response=corrosion-contact-residue material_half_life_seconds=36 wetness_half_life_seconds=18 semantic_lod=0.84 streaming_cost=0.34 patch_channels=rust,wetness,material_stability contact_channels=weld_contact,visual_occlusion,traversal_affordance residue_channels=rust_residue,acoustic_occlusion,player_readable_cause
node export.runtime cook_export role=package
node diagnostic.quality diagnostic role=quality
edge factory.recipe factory.stage.source factory_order
edge factory.stage.source factory.stage.modifiers factory_order
edge factory.stage.modifiers factory.stage.surface factory_order
edge factory.stage.surface factory.stage.lod factory_order
edge factory.stage.lod factory.stage.physics factory_order
edge factory.stage.physics factory.stage.quality factory_order
edge factory.stage.surface factory.surface surface_contract
edge factory.stage.physics factory.physics physics_proxy
edge factory.stage.lod factory.lod lod_recipe
edge factory.surface factory.signal.rust quality_signal
edge factory.surface factory.signal.oxide quality_signal
edge factory.surface factory.signal.rim quality_signal
edge factory.signal.rust factory.claim.rust visual_brief_claim
edge factory.signal.oxide factory.claim.oxide visual_brief_claim
edge factory.signal.rim factory.claim.rim visual_brief_claim
edge modifier.bevel modifier.soft_rim rim_normal_feather
edge seam.pipe seam.contact contact_skirt
edge seam.contact render.depth_bias attachment_bias
edge mask.pits mask.layer_stack pitting_layer
edge mask.pits mask.rust pitting_seed
edge mask.rust_bloom mask.layer_stack bloom_layer
edge mask.black_scab mask.layer_stack black_scab_layer
edge perception.template export.runtime perceptual_template
"#,
        )
        .expect("pipe graph");
        let packaged = package_asset_graph(&graph, dir.join("package")).expect("package pipe");
        assert!(packaged.graph_bin.quality.production_ready);
        assert!(packaged.graph_bin.material.feature_mask & (1 << 43) != 0);
        assert!(packaged.graph_bin.material.feature_mask & (1 << 44) != 0);
        assert!(packaged.graph_bin.material.feature_mask & (1 << 45) != 0);
        assert!(packaged.graph_bin.material.feature_mask & (1 << 48) != 0);
        assert!(packaged.graph_bin.material.feature_mask & (1 << 51) != 0);
        assert!(packaged.graph_bin.material.feature_mask & (1 << 52) != 0);
        assert!(packaged.graph_bin.material.feature_mask & (1 << 53) != 0);
        assert!(packaged.graph_bin.material.feature_mask & (1 << 54) != 0);
        assert!(packaged.graph_bin.material.feature_mask & (1 << 55) != 0);
        assert!(packaged.graph_bin.material.feature_mask & (1 << 56) != 0);
        for bit in 57..=63 {
            assert!(packaged.graph_bin.material.feature_mask & (1u64 << bit) != 0);
        }
        assert!(packaged
            .graph_bin
            .factory_report
            .stable_recipe_hash
            .starts_with("0x"));
        assert_eq!(
            packaged.graph_bin.metadata.get("owner").map(String::as_str),
            Some("aster-headless-foundry")
        );
        assert_eq!(packaged.graph_bin.sockets.len(), 1);
        assert_eq!(packaged.graph_bin.zones.len(), 1);
        assert_eq!(packaged.graph_bin.bundle_items.len(), 1);
        assert_eq!(packaged.graph_bin.bake_targets.len(), 1);
        assert_eq!(packaged.graph_bin.proof_artifacts.len(), 1);
        assert_eq!(packaged.graph_bin.proof_artifacts[0].width, 640);
        assert!(packaged.graph_bin.factory_report.stage_diagnostics.len() >= 6);
        assert!(packaged
            .graph_bin
            .factory_report
            .surface_signal_coverage
            .iter()
            .any(|signal| signal.signal == "raised_weld_rings" && signal.status == "claimed"));
        assert_eq!(
            packaged
                .graph_bin
                .factory_report
                .collision_proxy_summary
                .get("shape")
                .map(String::as_str),
            Some("pipe-runtime-bounds")
        );
        assert!(packaged
            .graph_bin
            .factory_report
            .visual_brief_claims
            .iter()
            .any(|claim| claim == "reference_silhouette"));
        assert_eq!(
            packaged
                .graph_bin
                .quality
                .surface_stack
                .get("physical_texel_density")
                .map(String::as_str),
            Some("1024.000")
        );
        assert_eq!(
            packaged
                .graph_bin
                .quality
                .presentation_quality
                .get("surface_occlusion")
                .map(String::as_str),
            Some("authored")
        );
        assert!(packaged
            .graph_bin
            .quality
            .visual_proof_expectations
            .iter()
            .any(|value| value == "contact shadows visible"));
        assert_eq!(
            packaged.graph_bin.production_session.quality_gate,
            "production-ready"
        );
        assert!(packaged
            .graph_bin
            .production_session
            .cook_steps
            .iter()
            .any(|step| step == "preview-render"));
        fs::remove_dir_all(dir).ok();
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
        assert!(result
            .database
            .asset_graph
            .edges
            .iter()
            .any(|edge| !edge.present));
        assert!(!result.database.fate_reports[0].production_ready);
        assert!(result.database.records[0].outputs.is_empty());
        assert!(!output.join("materials").exists());
        assert!(!output.join("previews").exists());
        assert!(!output.join("textures").exists());
        fs::remove_dir_all(project.parent().unwrap()).ok();
    }

    #[test]
    fn cook_rejects_source_images_without_encoder() {
        let project = write_material_project("cook_png_without_encoder", false);
        let dir = project.parent().unwrap().to_path_buf();
        write_png_header(&dir.join("textures/wet_albedo.ktx2"), 16, 8);
        let output = dir.join("cooked/desktop");
        let result = cook_project(&project, "desktop", &output).expect("cook");
        assert!(result.error_count > 0);
        assert!(result.database.records[0]
            .diagnostics
            .iter()
            .any(|diagnostic| diagnostic.message.contains("requires KTX2 source")));
        assert!(result.database.records[0].outputs.is_empty());
        assert!(!output.join("materials").exists());
        assert!(!output.join("previews").exists());
        assert!(!output.join("textures").exists());
        fs::remove_dir_all(dir).ok();
    }

    #[test]
    fn cook_rejects_bad_material_roles_and_texture_color_space() {
        let dir = fixture_dir("cook_bad_roles");
        fs::create_dir_all(dir.join("materials")).expect("materials dir");
        fs::create_dir_all(dir.join("textures")).expect("textures dir");
        write_ktx2_header(&dir.join("textures/albedo.ktx2"), 16, 8, 4, 43);
        write_ktx2_header(&dir.join("textures/normal.ktx2"), 16, 8, 4, 43);
        write_ktx2_header(&dir.join("textures/orm.ktx2"), 16, 8, 4, 37);
        fs::write(
            dir.join("materials/bad.astermat"),
            r#"material BadRoles {
  shading_model: LitPBR
  textures {
    albedo: "../textures/albedo.ktx2"
    normal: "../textures/normal.ktx2"
    orm: "../textures/orm.ktx2"
    banana: "../textures/orm.ktx2"
  }
}
"#,
        )
        .expect("material");
        fs::write(
            dir.join("project.asterproj"),
            r#"{
  "schema_version": 2,
  "name": "Bad Roles",
  "assets": [
    {
      "id": "material.bad_roles",
      "guid": "asset-v2-bad-roles-0000000000000000001",
      "kind": "material",
      "path": "materials/bad.astermat",
      "import_preset": "default"
    }
  ]
}
"#,
        )
        .expect("project");
        let result = cook_project(
            &dir.join("project.asterproj"),
            "desktop",
            dir.join("cooked"),
        )
        .expect("cook");
        assert!(result.error_count >= 2);
        let messages = result.database.records[0]
            .diagnostics
            .iter()
            .map(|diagnostic| diagnostic.message.as_str())
            .collect::<Vec<_>>()
            .join("\n");
        assert!(messages.contains("unsupported texture role 'banana'"));
        assert!(messages.contains("expects linear KTX2 VkFormat 37"));
        assert!(result.database.records[0].outputs.is_empty());
        assert!(!dir.join("cooked/materials").exists());
        assert!(!dir.join("cooked/previews").exists());
        assert!(!dir.join("cooked/textures").exists());
        fs::remove_dir_all(dir).ok();
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

    #[test]
    fn gltf_import_math_contract_preserves_matrix_normal_and_mirror_sign() {
        let node = serde_json::json!({
            "matrix": [
                2.0, 0.0, 0.0, 0.0,
                0.0, 4.0, 0.0, 0.0,
                0.0, 0.0, 1.0, 0.0,
                1.0, 2.0, 3.0, 1.0
            ]
        });
        let matrix = node_matrix(&node).expect("node matrix");
        let point = transform_point(
            matrix,
            Vec3 {
                x: 1.0,
                y: 1.0,
                z: 1.0,
            },
        );
        assert_near(point.x, 3.0);
        assert_near(point.y, 6.0);
        assert_near(point.z, 4.0);

        let normal = transform_normal(
            matrix,
            Vec3 {
                x: 1.0,
                y: 1.0,
                z: 0.0,
            },
        );
        assert_near(normal.x, 0.8944272);
        assert_near(normal.y, 0.4472136);
        assert_near(normal.z, 0.0);

        let mirrored = Mat4 {
            m: [
                -1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0,
            ],
        };
        assert_near(linear_determinant(mirrored), -1.0);
        assert_near(transform_tangent_handedness(mirrored, 1.0), -1.0);
        assert_near(transform_tangent_handedness(mirrored, -1.0), 1.0);
    }

    #[test]
    fn reports_foundry_lineage_config_and_session_audits() {
        let project = write_material_project("foundry_reports", false);
        let output = project.parent().unwrap().join("cooked/desktop");
        let result = cook_project(&project, "desktop", &output).expect("cook");
        let foundry = asset_foundry_report_json(&result.database).expect("foundry");
        assert!(foundry.contains("import_recipes"));
        assert!(foundry.contains("production_readiness_reasons"));
        let lineage = cook_lineage_report_json(&result.database).expect("lineage");
        assert!(lineage.contains("project_fingerprint"));
        assert!(lineage.contains("artifact_manifest_hash"));
        assert!(lineage.contains("referentially_transparent_build"));
        assert!(lineage.contains("production_ready_assets"));
        let lineage_diff =
            cook_lineage_diff_json(&result.database, &result.database).expect("lineage diff");
        assert!(lineage_diff.contains("\"changed\": []"));

        let mut stack = ConfigLayerStackRecord::default();
        stack.layers.push(ConfigLayerRecord {
            name: "defaults".to_string(),
            priority: 0,
            values: BTreeMap::from([
                ("render.backend".to_string(), "software".to_string()),
                ("tools.audit".to_string(), "normal".to_string()),
            ]),
        });
        stack.layers.push(ConfigLayerRecord {
            name: "project".to_string(),
            priority: 10,
            values: BTreeMap::from([("tools.audit".to_string(), "strict".to_string())]),
        });
        assert_eq!(stack.resolve().get("tools.audit").unwrap(), "strict");

        let history = project.parent().unwrap().join("history.jsonl");
        fs::write(
            &history,
            r#"{"session_id":"studio","kind":"command","text":"open","detail":"lab","ts":1,"sequence":1}
{"session_id":"assetc","kind":"tool","text":"catalog-audit","detail":"db","ts":2,"sequence":2}
"#,
        )
        .expect("history");
        let audit = session_audit_report_json(&history, Some(160)).expect("session audit");
        assert!(audit.contains("retained_entries"));
        assert!(audit.contains("assetc"));

        let recipe = project.parent().unwrap().join("recipe.json");
        fs::write(
            &recipe,
            r#"{"id":"mesh.recipe","variant_intent_tags":["uv:packed"],"steps":[{"kind":"triangulate"},{"kind":"uv-pack"}]}"#,
        )
        .expect("recipe");
        let mesh_recipe = mesh_recipe_inspect_report_json(&recipe).expect("mesh recipe");
        assert!(mesh_recipe.contains("\"steps\": 2"));
        assert!(mesh_recipe.contains("uv-pack"));
        fs::remove_dir_all(project.parent().unwrap()).ok();
    }
}
