// Author: Faruk Alpay
// Do not remove this notice.

use std::fs;
use std::io::Write;
use std::path::PathBuf;
use std::process::Command;
use std::sync::atomic::{AtomicU64, Ordering};
use std::time::{SystemTime, UNIX_EPOCH};

static FIXTURE_COUNTER: AtomicU64 = AtomicU64::new(0);

fn append_f32(bytes: &mut Vec<u8>, value: f32) {
    bytes.extend_from_slice(&value.to_le_bytes());
}

fn append_u16(bytes: &mut Vec<u8>, value: u16) {
    bytes.extend_from_slice(&value.to_le_bytes());
}

fn fixture_dir() -> PathBuf {
    let stamp = SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .expect("clock")
        .as_nanos();
    let counter = FIXTURE_COUNTER.fetch_add(1, Ordering::Relaxed);
    std::env::temp_dir().join(format!(
        "aster_assetc_cli_{}_{}_{}",
        std::process::id(),
        stamp,
        counter
    ))
}

fn write_fixture() -> PathBuf {
    let dir = fixture_dir();
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
    let bin = dir.join("asset.bin");
    fs::File::create(&bin)
        .expect("bin")
        .write_all(&bytes)
        .expect("write bin");
    let scene = dir.join("asset.scene");
    fs::write(
        &scene,
        format!(
            r#"{{
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
      "roughnessFactor": 0.62
    }},
    "doubleSided": true,
    "alphaMode": "MASK"
  }}],
  "buffers": [{{ "byteLength": {}, "uri": "asset.bin" }}],
  "bufferViews": [
    {{ "buffer": 0, "byteOffset": 0, "byteLength": 48 }},
    {{ "buffer": 0, "byteOffset": 48, "byteLength": 48 }},
    {{ "buffer": 0, "byteOffset": 96, "byteLength": 32 }},
    {{ "buffer": 0, "byteOffset": 128, "byteLength": 12 }}
  ],
  "accessors": [
    {{ "bufferView": 0, "byteOffset": 0, "componentType": 5126, "count": 4, "type": "VEC3" }},
    {{ "bufferView": 1, "byteOffset": 0, "componentType": 5126, "count": 4, "type": "VEC3" }},
    {{ "bufferView": 2, "byteOffset": 0, "componentType": 5126, "count": 4, "type": "VEC2" }},
    {{ "bufferView": 3, "byteOffset": 0, "componentType": 5123, "count": 6, "type": "SCALAR" }}
  ]
}}"#,
            bytes.len()
        ),
    )
    .expect("write scene");
    scene
}

fn write_png_header(path: &PathBuf, width: u32, height: u32) {
    let mut png = Vec::new();
    png.extend_from_slice(&[0x89, b'P', b'N', b'G', b'\r', b'\n', 0x1a, b'\n']);
    png.extend_from_slice(&13u32.to_be_bytes());
    png.extend_from_slice(b"IHDR");
    png.extend_from_slice(&width.to_be_bytes());
    png.extend_from_slice(&height.to_be_bytes());
    png.extend_from_slice(&[8, 6, 0, 0, 0]);
    fs::write(path, png).expect("png");
}

fn write_ktx2_header(path: &PathBuf, width: u32, height: u32, mip_count: u32, vk_format: u32) {
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
    ktx2.extend_from_slice(b"ASTER_CLI_TEST_KTX2_PAYLOAD");
    fs::write(path, ktx2).expect("ktx2");
}

fn write_material_project() -> PathBuf {
    let dir = fixture_dir();
    fs::create_dir_all(dir.join("materials")).expect("materials");
    fs::create_dir_all(dir.join("textures")).expect("textures");
    write_ktx2_header(&dir.join("textures/albedo.ktx2"), 16, 16, 5, 43);
    write_ktx2_header(&dir.join("textures/normal.ktx2"), 16, 16, 5, 37);
    write_ktx2_header(&dir.join("textures/orm.ktx2"), 16, 16, 5, 37);
    fs::write(
        dir.join("materials/test.astermat"),
        r#"material CliWetRock {
  name: "CLI Wet Rock"
  shading_model: LitPBR
  blend_mode: Opaque
  cull_mode: Back
  provenance {
    generator: "cli-test"
  }
  authoring {
    texel_density: 2.7
    mapping_policy: triplanar
  }
  preview {
    rig: "normalized-three-point"
  }
  quality_profile {
    mobile_drop_parallax: true
  }
  textures {
    albedo: "../textures/albedo.ktx2"
    normal: "../textures/normal.ktx2"
    orm: "../textures/orm.ktx2"
  }
  params {
    base_color_r: 0.25
    base_color_g: 0.24
    base_color_b: 0.22
    roughness: 0.8
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
	  "name": "CLI Project",
	  "assets": [
	    {
	      "id": "material.cli_wet_rock",
	      "guid": "asset-v2-cli-wet-rock-0000000000000001",
	      "kind": "material",
	      "path": "materials/test.astermat",
	      "import_preset": "default"
	    }
	  ]
	}
	"#,
    )
    .expect("project");
    dir.join("project.asterproj")
}

fn write_broken_material_project() -> PathBuf {
    let dir = fixture_dir();
    fs::create_dir_all(dir.join("materials")).expect("materials");
    fs::create_dir_all(dir.join("textures")).expect("textures");
    write_png_header(&dir.join("textures/albedo.png"), 16, 16);
    write_ktx2_header(&dir.join("textures/normal.ktx2"), 16, 16, 5, 37);
    fs::write(
        dir.join("materials/test.astermat"),
        r#"material BrokenCliRock {
  name: "Broken CLI Rock"
  shading_model: LitPBR
  blend_mode: Opaque
  cull_mode: Back
  textures {
    albedo: "../textures/albedo.png"
    normal: "../textures/normal.ktx2"
  }
}
"#,
    )
    .expect("material");
    fs::write(
        dir.join("project.asterproj"),
        r#"{
	  "schema_version": 2,
	  "name": "Broken CLI Project",
	  "assets": [
	    {
	      "id": "material.broken_cli_rock",
	      "guid": "asset-v2-broken-cli-rock-000000000001",
	      "kind": "material",
	      "path": "materials/test.astermat",
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
fn compile_and_inspect_runtime_cache() {
    let scene = write_fixture();
    let cache = scene.parent().unwrap().join("asset.astercache");
    let binary = env!("CARGO_BIN_EXE_aster_assetc");
    let compile = Command::new(binary)
        .arg("compile")
        .arg("--input")
        .arg(&scene)
        .arg("--output")
        .arg(&cache)
        .arg("--origin")
        .arg("center-on-ground")
        .output()
        .expect("run compile");
    assert!(
        compile.status.success(),
        "{}",
        String::from_utf8_lossy(&compile.stderr)
    );
    assert!(cache.exists());

    let inspect = Command::new(binary)
        .arg("inspect")
        .arg("--input")
        .arg(&cache)
        .output()
        .expect("run inspect");
    assert!(
        inspect.status.success(),
        "{}",
        String::from_utf8_lossy(&inspect.stderr)
    );
    let stdout = String::from_utf8_lossy(&inspect.stdout);
    assert!(stdout.contains("Aster cache v3"));
    assert!(stdout.contains("materials=2"));
    assert!(stdout.contains("meshes=1"));
    assert!(stdout.contains("collision_meshes=1"));
    fs::remove_dir_all(scene.parent().unwrap()).ok();
}

#[test]
fn cook_and_report_asset_database() {
    let project = write_material_project();
    let output_dir = project.parent().unwrap().join("cooked/desktop");
    let binary = env!("CARGO_BIN_EXE_aster_assetc");
    let cook = Command::new(binary)
        .arg("cook")
        .arg("--project")
        .arg(&project)
        .arg("--platform")
        .arg("desktop")
        .arg("--output")
        .arg(&output_dir)
        .output()
        .expect("run cook");
    assert!(
        cook.status.success(),
        "{}",
        String::from_utf8_lossy(&cook.stderr)
    );
    let stdout = String::from_utf8_lossy(&cook.stdout);
    assert!(stdout.contains("assets=1"));
    let db = output_dir.join("assetdb.asterdb.json");
    assert!(db.exists());
    assert!(output_dir.join("materials/cliwetrock.materialbin").exists());
    assert!(output_dir.join("previews/cliwetrock.preview.ppm").exists());

    let report = Command::new(binary)
        .arg("report")
        .arg("--db")
        .arg(&db)
        .output()
        .expect("run report");
    assert!(
        report.status.success(),
        "{}",
        String::from_utf8_lossy(&report.stderr)
    );
    let report_stdout = String::from_utf8_lossy(&report.stdout);
    assert!(report_stdout.contains("material.cli_wet_rock"));
    assert!(report_stdout.contains("outputs="));

    let graph = Command::new(binary)
        .arg("graph")
        .arg("--db")
        .arg(&db)
        .output()
        .expect("run graph");
    assert!(graph.status.success());
    let graph_stdout = String::from_utf8_lossy(&graph.stdout);
    assert!(graph_stdout.contains("project_fingerprint"));
    assert!(graph_stdout.contains("material.cli_wet_rock"));

    let fate = Command::new(binary)
        .arg("fate")
        .arg("--db")
        .arg(&db)
        .arg("--asset")
        .arg("material.cli_wet_rock")
        .output()
        .expect("run fate");
    assert!(fate.status.success());
    let fate_stdout = String::from_utf8_lossy(&fate.stdout);
    assert!(fate_stdout.contains("shader-variant"));
    assert!(fate_stdout.contains("material-hash"));

    let diff = Command::new(binary)
        .arg("diff")
        .arg("--before")
        .arg(&db)
        .arg("--after")
        .arg(&db)
        .output()
        .expect("run diff");
    assert!(diff.status.success());
    let diff_stdout = String::from_utf8_lossy(&diff.stdout);
    assert!(diff_stdout.contains("\"changed\": []"));
    fs::remove_dir_all(project.parent().unwrap()).ok();
}

#[test]
fn strict_cook_fails_broken_material_and_skips_runtime_outputs() {
    let project = write_broken_material_project();
    let output_dir = project.parent().unwrap().join("cooked/desktop");
    let binary = env!("CARGO_BIN_EXE_aster_assetc");
    let cook = Command::new(binary)
        .arg("cook")
        .arg("--project")
        .arg(&project)
        .arg("--platform")
        .arg("desktop")
        .arg("--output")
        .arg(&output_dir)
        .output()
        .expect("run cook");
    assert!(!cook.status.success());
    let stderr = String::from_utf8_lossy(&cook.stderr);
    assert!(stderr.contains("strict cook failed"));
    let db = output_dir.join("assetdb.asterdb.json");
    assert!(db.exists());
    assert!(!output_dir
        .join("materials/brokenclirock.materialbin")
        .exists());
    assert!(!output_dir
        .join("previews/brokenclirock.preview.ppm")
        .exists());
    assert!(!output_dir.join("textures").exists());

    let report = Command::new(binary)
        .arg("report")
        .arg("--db")
        .arg(&db)
        .output()
        .expect("run report");
    assert!(report.status.success());
    let report_stdout = String::from_utf8_lossy(&report.stdout);
    assert!(report_stdout.contains("errors="));
    assert!(report_stdout.contains("requires 'orm' texture"));
    fs::remove_dir_all(project.parent().unwrap()).ok();
}

#[test]
fn material_inspect_reports_strict_validation() {
    let project = write_broken_material_project();
    let material = project.parent().unwrap().join("materials/test.astermat");
    let binary = env!("CARGO_BIN_EXE_aster_assetc");
    let inspect = Command::new(binary)
        .arg("material-inspect")
        .arg("--input")
        .arg(&material)
        .output()
        .expect("run material inspect");
    assert!(inspect.status.success());
    let stdout = String::from_utf8_lossy(&inspect.stdout);
    assert!(stdout.contains("\"production_ready\": false"));
    assert!(stdout.contains("requires 'orm' texture"));
    fs::remove_dir_all(project.parent().unwrap()).ok();
}

#[test]
fn materialc_packages_single_material_contract() {
    let project = write_material_project();
    let project_root = project.parent().unwrap();
    let material = project_root.join("materials/test.astermat");
    let output_dir = project_root.join("materialc/desktop");
    let binary = env!("CARGO_BIN_EXE_aster_materialc");
    let package = Command::new(binary)
        .arg("package")
        .arg("--input")
        .arg(&material)
        .arg("--asset-root")
        .arg(project_root)
        .arg("--platform")
        .arg("desktop")
        .arg("--output")
        .arg(&output_dir)
        .output()
        .expect("run materialc package");
    assert!(
        package.status.success(),
        "{}",
        String::from_utf8_lossy(&package.stderr)
    );
    let stdout = String::from_utf8_lossy(&package.stdout);
    assert!(stdout.contains("material package"));
    assert!(stdout.contains("runtime_outputs=true"));
    assert!(stdout.contains("bindings="));
    assert!(output_dir.join("materials/cliwetrock.materialbin").exists());
    assert!(output_dir.join("previews/cliwetrock.preview.ppm").exists());
    fs::remove_dir_all(project_root).ok();
}

#[test]
fn texturec_packages_single_texture_contract() {
    let dir = fixture_dir();
    fs::create_dir_all(&dir).expect("texture fixture dir");
    let texture = dir.join("albedo.ktx2");
    write_ktx2_header(&texture, 16, 16, 5, 43);
    let output_dir = dir.join("texturec/desktop");
    let binary = env!("CARGO_BIN_EXE_aster_texturec");
    let package = Command::new(binary)
        .arg("package")
        .arg("--input")
        .arg(&texture)
        .arg("--role")
        .arg("albedo")
        .arg("--output")
        .arg(&output_dir)
        .output()
        .expect("run texturec package");
    assert!(
        package.status.success(),
        "{}",
        String::from_utf8_lossy(&package.stderr)
    );
    let stdout = String::from_utf8_lossy(&package.stdout);
    assert!(stdout.contains("texture package"));
    assert!(stdout.contains("runtime_format=ktx2"));
    assert!(stdout.contains("colorspace=srgb"));
    assert!(output_dir.join("textures/albedo.ktx2").exists());
    assert!(output_dir
        .join("reports/albedo.albedo.report.json")
        .exists());
    fs::remove_dir_all(&dir).ok();
}
