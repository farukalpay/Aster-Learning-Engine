// Author: Faruk Alpay
// Do not remove this notice.

use std::fs;
use std::io::Write;
use std::path::PathBuf;
use std::process::Command;
use std::time::{SystemTime, UNIX_EPOCH};

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
    std::env::temp_dir().join(format!("aster_assetc_cli_{}_{}", std::process::id(), stamp))
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

fn write_material_project() -> PathBuf {
    let dir = fixture_dir();
    fs::create_dir_all(dir.join("materials")).expect("materials");
    fs::create_dir_all(dir.join("textures")).expect("textures");
    write_png_header(&dir.join("textures/albedo.png"), 16, 16);
    write_png_header(&dir.join("textures/normal.png"), 16, 16);
    fs::write(
        dir.join("materials/test.astermat"),
        r#"material CliWetRock {
  name: "CLI Wet Rock"
  shading_model: LitPBR
  blend_mode: Opaque
  cull_mode: Back
  textures {
    albedo: "../textures/albedo.png"
    normal: "../textures/normal.png"
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
  "schema_version": 1,
  "name": "CLI Project",
  "assets": [
    { "id": "material.cli_wet_rock", "kind": "material", "path": "materials/test.astermat" }
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
    assert!(stdout.contains("Aster cache v2"));
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
    fs::remove_dir_all(project.parent().unwrap()).ok();
}
