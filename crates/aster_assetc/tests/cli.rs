// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

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

fn source_root() -> PathBuf {
    PathBuf::from(env!("CARGO_MANIFEST_DIR"))
        .parent()
        .and_then(|path| path.parent())
        .expect("workspace root")
        .to_path_buf()
}

fn pipe_lab_project() -> PathBuf {
    source_root().join("showcases/pipe_lab/pipe_lab.asterproj")
}

fn lumen_project() -> PathBuf {
    source_root().join("projects/lumen_run/lumen_run.asterproj")
}

fn lumen_learning_trace() -> PathBuf {
    source_root().join("projects/lumen_run/lessons/lumen_mining.trace.jsonl")
}

fn industrial_pipe_preview() -> PathBuf {
    source_root().join("assets/screenshots/industrial_pipe.png")
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

fn write_pipe_project_with_broken_material() -> PathBuf {
    let dir = fixture_dir();
    fs::create_dir_all(dir.join("materials")).expect("materials");
    fs::create_dir_all(dir.join("textures")).expect("textures");
    fs::copy(
        source_root().join("showcases/pipe_lab/rusted_pipe.astergraph"),
        dir.join("rusted_pipe.astergraph"),
    )
    .expect("copy pipe graph");
    write_png_header(&dir.join("textures/albedo.png"), 16, 16);
    write_ktx2_header(&dir.join("textures/normal.ktx2"), 16, 16, 5, 37);
    fs::write(
        dir.join("materials/broken.astermat"),
        r#"material BrokenPipeProofMaterial {
  name: "Broken Pipe Proof Material"
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
    .expect("broken material");
    fs::write(
        dir.join("project.asterproj"),
        r#"{
  "schema_version": 2,
  "name": "Broken Pipe Proof Project",
  "assets": [
    {
      "id": "asset_graph.pipe_lab.rusted_pipe",
      "guid": "asset-v2-pipe-proof-rusted-pipe-0001",
      "kind": "asset_graph",
      "path": "rusted_pipe.astergraph",
      "import_preset": "default"
    },
    {
      "id": "material.pipe_proof_broken",
      "guid": "asset-v2-pipe-proof-broken-material-1",
      "kind": "material",
      "path": "materials/broken.astermat",
      "import_preset": "default"
    }
  ]
}
"#,
    )
    .expect("project");
    dir.join("project.asterproj")
}

fn write_agent_tool_repo() -> PathBuf {
    let dir = fixture_dir();
    fs::create_dir_all(dir.join("include/aster")).expect("include dir");
    fs::create_dir_all(dir.join("src/runtime")).expect("src dir");
    fs::create_dir_all(dir.join("projects/demo/materials")).expect("content dir");
    let legacy_header = format!(
        "// Author: Faruk {}\n// Do not remove {} notice.\n",
        "Alpay", "this"
    );
    fs::write(
        dir.join("include/aster/demo.hpp"),
        format!("{legacy_header}\n#pragma once\nclass DemoAgentSurface {{}};\n"),
    )
    .expect("header");
    fs::write(
        dir.join("src/runtime/agent_runtime.cpp"),
        "// SPDX-License-Identifier: Apache-2.0\n// Copyright (c) 2026 Faruk Alpay\n\n#include <aster/demo.hpp>\nvoid aster_agent_runtime_tool_surface() {}\n",
    )
    .expect("runtime");
    fs::write(
        dir.join("projects/demo/materials/test.astermat"),
        format!("{legacy_header}\nmaterial DemoAgentMaterial {{}}\n"),
    )
    .expect("material");
    fs::write(
        dir.join("plan.json"),
        r#"{
  "objective": "audit Aster agent tooling",
  "steps": ["agent-fix-headers", "agent-review"]
}
"#,
    )
    .expect("plan");
    dir
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
    assert!(report_stdout.contains("world_ready=true"));

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
    assert!(fate_stdout.contains("world_ready"));
    assert!(fate_stdout.contains("wetness_propagation"));
    assert!(fate_stdout.contains("perceptual_stability"));

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

    let catalog = Command::new(binary)
        .arg("catalog-inspect")
        .arg("--db")
        .arg(&db)
        .output()
        .expect("run catalog inspect");
    assert!(catalog.status.success());
    let catalog_stdout = String::from_utf8_lossy(&catalog.stdout);
    assert!(catalog_stdout.contains("aster-catalog-"));
    assert!(catalog_stdout.contains("path=Assets/Material"));

    let catalog_audit = Command::new(binary)
        .arg("catalog-audit")
        .arg("--db")
        .arg(&db)
        .output()
        .expect("run catalog audit");
    assert!(catalog_audit.status.success());
    let catalog_audit_stdout = String::from_utf8_lossy(&catalog_audit.stdout);
    assert!(catalog_audit_stdout.contains("import_recipes"));
    assert!(catalog_audit_stdout.contains("production_readiness_reasons"));

    let lineage_report = Command::new(binary)
        .arg("lineage-report")
        .arg("--db")
        .arg(&db)
        .output()
        .expect("run lineage report");
    assert!(lineage_report.status.success());
    let lineage_report_stdout = String::from_utf8_lossy(&lineage_report.stdout);
    assert!(lineage_report_stdout.contains("project_fingerprint"));
    assert!(lineage_report_stdout.contains("artifact_manifest_hash"));
    assert!(lineage_report_stdout.contains("referentially_transparent_build"));
    assert!(lineage_report_stdout.contains("production_ready_assets"));

    let lineage_diff = Command::new(binary)
        .arg("lineage-diff")
        .arg("--before")
        .arg(&db)
        .arg("--after")
        .arg(&db)
        .output()
        .expect("run lineage diff");
    assert!(lineage_diff.status.success());
    let lineage_diff_stdout = String::from_utf8_lossy(&lineage_diff.stdout);
    assert!(lineage_diff_stdout.contains("\"changed\": []"));

    let catalog_file = output_dir.join("aster_catalogs.json");
    let catalog_sync = Command::new(binary)
        .arg("catalog-sync")
        .arg("--db")
        .arg(&db)
        .arg("--output")
        .arg(&catalog_file)
        .output()
        .expect("run catalog sync");
    assert!(
        catalog_sync.status.success(),
        "{}",
        String::from_utf8_lossy(&catalog_sync.stderr)
    );
    let catalog_json = fs::read_to_string(&catalog_file).expect("read catalog store");
    assert!(catalog_json.contains("\"schema_version\""));
    assert!(catalog_json.contains("\"path\": \"Assets/Material\""));
    assert!(catalog_json.contains("\"asset_count\": \"1\""));
    fs::remove_dir_all(project.parent().unwrap()).ok();
}

#[test]
fn audit_session_and_mesh_recipe_commands() {
    let dir = fixture_dir();
    fs::create_dir_all(&dir).expect("audit fixture dir");
    let history = dir.join("history.jsonl");
    fs::write(
        &history,
        r#"{"session_id":"studio","kind":"command","text":"open","detail":"material-lab","ts":1,"sequence":1}
{"session_id":"studio","kind":"command","text":"catalog-audit","detail":"asset-db","ts":2,"sequence":2}
{"session_id":"assetc","kind":"tool","text":"lineage-report","detail":"db","ts":3,"sequence":3}
"#,
    )
    .expect("history");
    let recipe = dir.join("recipe.json");
    fs::write(
        &recipe,
        r#"{
  "id": "mesh.recipe.audit",
  "variant_intent_tags": ["uv:packed", "profile:test"],
  "steps": [
    { "kind": "triangulate" },
    { "kind": "uv-pack" },
    { "kind": "recalculate-normals" }
  ]
}
"#,
    )
    .expect("recipe");
    let binary = env!("CARGO_BIN_EXE_aster_assetc");
    let session = Command::new(binary)
        .arg("session-audit")
        .arg("--input")
        .arg(&history)
        .arg("--max-bytes")
        .arg("1000")
        .output()
        .expect("run session audit");
    assert!(
        session.status.success(),
        "{}",
        String::from_utf8_lossy(&session.stderr)
    );
    let session_stdout = String::from_utf8_lossy(&session.stdout);
    assert!(session_stdout.contains("retained_entries"));
    assert!(session_stdout.contains("studio"));

    let recipe_report = Command::new(binary)
        .arg("mesh-recipe-inspect")
        .arg("--input")
        .arg(&recipe)
        .output()
        .expect("run mesh recipe inspect");
    assert!(recipe_report.status.success());
    let recipe_stdout = String::from_utf8_lossy(&recipe_report.stdout);
    assert!(recipe_stdout.contains("\"steps\": 3"));
    assert!(recipe_stdout.contains("uv-pack"));
    assert!(recipe_stdout.contains("variant_intent_tags"));
    fs::remove_dir_all(&dir).ok();
}

#[test]
fn agent_plan_reports_batch_contracts() {
    let project = write_material_project();
    fs::write(
        project.parent().unwrap().join("AGENTS.md"),
        "# Agent scope\n\n- Keep material edits in Aster-owned authoring files.\n",
    )
    .expect("agent instructions");
    let binary = env!("CARGO_BIN_EXE_aster_assetc");
    let plan = Command::new(binary)
        .arg("agent-plan")
        .arg("--project")
        .arg(&project)
        .arg("--objective")
        .arg("Let an agent extend the material lab safely.")
        .arg("--output-schema")
        .output()
        .expect("run agent plan");
    assert!(
        plan.status.success(),
        "{}",
        String::from_utf8_lossy(&plan.stderr)
    );
    let stdout = String::from_utf8_lossy(&plan.stdout);
    assert!(stdout.contains("\"kind\": \"aster_agent_plan\""));
    assert!(stdout.contains("batch.content_surface"));
    assert!(stdout.contains("agent.visual_proof_surface"));
    assert!(stdout.contains("\"instruction_stack\""));
    assert!(stdout.contains("Keep material edits in Aster-owned authoring files"));
    assert!(stdout.contains("\"command_policy\""));
    assert!(stdout.contains("aster.deny.git_reset_hard"));
    assert!(stdout.contains("\"handoff_policy\""));
    assert!(stdout.contains("Aster Agent Batch Report"));
    assert!(stdout.contains("\"physics_performance_contract\""));
    assert!(stdout.contains("mesh_triangle_candidate_count"));
    assert!(stdout.contains("Do not add third-party notice files"));
    assert!(stdout.contains("\"header_policy\""));
    assert!(stdout.contains("\"allowed_license_ids\""));
    assert!(stdout.contains("\"dirty_worktree\""));
    fs::remove_dir_all(project.parent().unwrap()).ok();
}

#[test]
fn agent_maintenance_commands_audit_review_and_fix_headers() {
    let repo = write_agent_tool_repo();
    let binary = env!("CARGO_BIN_EXE_aster_assetc");

    let check = Command::new(binary)
        .arg("agent-fix-headers")
        .arg("--repo")
        .arg(&repo)
        .arg("--check")
        .output()
        .expect("run header check");
    assert!(!check.status.success());
    let stderr = String::from_utf8_lossy(&check.stderr);
    assert!(stderr.contains("legacy-author-notice"));

    let write = Command::new(binary)
        .arg("agent-fix-headers")
        .arg("--repo")
        .arg(&repo)
        .arg("--write")
        .output()
        .expect("run header write");
    assert!(
        write.status.success(),
        "{}",
        String::from_utf8_lossy(&write.stderr)
    );
    let write_stdout = String::from_utf8_lossy(&write.stdout);
    assert!(write_stdout.contains("include/aster/demo.hpp"));
    assert!(write_stdout.contains("projects/demo/materials/test.astermat"));

    let fixed_header = fs::read_to_string(repo.join("include/aster/demo.hpp")).expect("header");
    assert!(fixed_header.starts_with("// SPDX-License-Identifier: Apache-2.0"));
    assert!(!fixed_header.contains("Do not remove"));
    let fixed_material =
        fs::read_to_string(repo.join("projects/demo/materials/test.astermat")).expect("material");
    assert!(fixed_material.starts_with("// SPDX-License-Identifier: LicenseRef-Aster-Content"));

    let clean_check = Command::new(binary)
        .arg("agent-fix-headers")
        .arg("--repo")
        .arg(&repo)
        .arg("--check")
        .output()
        .expect("run clean header check");
    assert!(
        clean_check.status.success(),
        "{}",
        String::from_utf8_lossy(&clean_check.stderr)
    );
    assert!(String::from_utf8_lossy(&clean_check.stdout).contains("\"error_count\": 0"));

    let audit = Command::new(binary)
        .arg("agent-audit")
        .arg("--repo")
        .arg(&repo)
        .arg("--json")
        .output()
        .expect("run agent audit");
    assert!(audit.status.success());
    let audit_stdout = String::from_utf8_lossy(&audit.stdout);
    assert!(audit_stdout.contains("\"kind\": \"aster_agent_audit\""));
    assert!(audit_stdout.contains("\"header_errors\": 0"));
    assert!(audit_stdout.contains("LicenseRef-Aster-Content"));

    let native_report = repo.join("native.md");
    let native = Command::new(binary)
        .arg("agent-native-audit")
        .arg("--repo")
        .arg(&repo)
        .arg("--markdown")
        .arg("--output")
        .arg(&native_report)
        .output()
        .expect("run native audit");
    assert!(native.status.success());
    assert!(fs::read_to_string(&native_report)
        .expect("native report")
        .contains("Aster Native Audit"));

    let runtime = Command::new(binary)
        .arg("agent-runtime-audit")
        .arg("--repo")
        .arg(&repo)
        .arg("--json")
        .output()
        .expect("run runtime audit");
    assert!(runtime.status.success());
    assert!(String::from_utf8_lossy(&runtime.stdout).contains("agent-contract"));

    let review = Command::new(binary)
        .arg("agent-review")
        .arg("--plan")
        .arg(repo.join("plan.json"))
        .arg("--repo")
        .arg(&repo)
        .arg("--json")
        .output()
        .expect("run agent review");
    assert!(review.status.success());
    let review_stdout = String::from_utf8_lossy(&review.stdout);
    assert!(review_stdout.contains("\"kind\": \"aster_agent_review\""));
    assert!(review_stdout.contains("\"readiness\": \"ready\""));

    fs::remove_dir_all(&repo).ok();
}

#[test]
fn asset_brief_reports_reference_quality_gate() {
    let project = write_material_project();
    let reference = project.parent().unwrap().join("reference.png");
    write_png_header(&reference, 320, 180);
    let binary = env!("CARGO_BIN_EXE_aster_assetc");
    let brief = Command::new(binary)
        .arg("asset-brief")
        .arg("--project")
        .arg(&project)
        .arg("--asset")
        .arg("asset_graph.pipe_lab.rusted_pipe")
        .arg("--reference")
        .arg(&reference)
        .arg("--output-schema")
        .output()
        .expect("run asset brief");
    assert!(
        brief.status.success(),
        "{}",
        String::from_utf8_lossy(&brief.stderr)
    );
    let stdout = String::from_utf8_lossy(&brief.stdout);
    assert!(stdout.contains("\"kind\": \"aster_agent_asset_brief\""));
    assert!(stdout.contains("corroded_orange_brown_rust"));
    assert!(stdout.contains("smooth_black_pipe"));
    assert!(stdout.contains("presentation_quality"));
    assert!(stdout.contains("surface_occlusion"));
    assert!(stdout.contains("physical_texel_density"));
    assert!(stdout.contains("height_normal_coupling"));
    assert!(stdout.contains("Aster Agent Asset Iteration Report"));
    assert!(stdout.contains("A passed build is not enough"));
    fs::remove_dir_all(project.parent().unwrap()).ok();
}

#[test]
fn asset_proof_run_writes_passing_pipe_bundle() {
    let output_dir = fixture_dir();
    let binary = env!("CARGO_BIN_EXE_aster_assetc");
    let proof = Command::new(binary)
        .arg("asset-proof-run")
        .arg("--project")
        .arg(pipe_lab_project())
        .arg("--asset")
        .arg("asset_graph.pipe_lab.rusted_pipe")
        .arg("--reference")
        .arg(industrial_pipe_preview())
        .arg("--preview-artifact")
        .arg(industrial_pipe_preview())
        .arg("--output")
        .arg(&output_dir)
        .arg("--output-schema")
        .output()
        .expect("run asset proof");
    assert!(
        proof.status.success(),
        "{}\n{}",
        String::from_utf8_lossy(&proof.stdout),
        String::from_utf8_lossy(&proof.stderr)
    );
    let stdout = String::from_utf8_lossy(&proof.stdout);
    assert!(stdout.contains("\"kind\": \"aster_agent_asset_proof_run\""));
    assert!(stdout.contains("\"status\": \"passed\""));
    assert!(stdout.contains("missing_weld_rings"));
    assert!(output_dir.join("brief.json").exists());
    assert!(output_dir.join("graph-inspect.json").exists());
    assert!(output_dir
        .join("package/asset_graphs/rusted_pipe.assetgraphbin")
        .exists());
    assert!(output_dir.join("cooked/assetdb.asterdb.json").exists());
    assert!(output_dir.join("proof-run.json").exists());
    let proof_json = fs::read_to_string(output_dir.join("proof-run.json")).expect("proof json");
    assert!(proof_json.contains("\"passed\": true"));
    assert!(proof_json.contains("physical_texel_density"));
    fs::remove_dir_all(&output_dir).ok();
}

#[test]
fn asset_proof_run_reports_structured_failures() {
    let binary = env!("CARGO_BIN_EXE_aster_assetc");
    let run = |output_dir: &PathBuf, extra: &[&str], preview: PathBuf| {
        let mut command = Command::new(binary);
        command
            .arg("asset-proof-run")
            .arg("--project")
            .arg(pipe_lab_project())
            .arg("--asset")
            .arg("asset_graph.pipe_lab.rusted_pipe")
            .arg("--reference")
            .arg(industrial_pipe_preview())
            .arg("--preview-artifact")
            .arg(preview)
            .arg("--output")
            .arg(output_dir);
        for arg in extra {
            command.arg(arg);
        }
        command.output().expect("run failing asset proof")
    };

    let missing_preview_dir = fixture_dir();
    let missing_preview = run(
        &missing_preview_dir,
        &[],
        missing_preview_dir.join("missing-preview.png"),
    );
    assert!(!missing_preview.status.success());
    let missing_preview_json =
        fs::read_to_string(missing_preview_dir.join("proof-run.json")).expect("missing proof json");
    assert!(missing_preview_json.contains("\"preview-artifact\""));
    assert!(missing_preview_json.contains("\"status\": \"failed\""));
    fs::remove_dir_all(&missing_preview_dir).ok();

    let missing_required_dir = fixture_dir();
    let missing_required = run(
        &missing_required_dir,
        &["--require", "absent_surface_signal"],
        industrial_pipe_preview(),
    );
    assert!(!missing_required.status.success());
    let missing_required_json = fs::read_to_string(missing_required_dir.join("proof-run.json"))
        .expect("missing required proof json");
    assert!(missing_required_json.contains("absent_surface_signal"));
    assert!(missing_required_json.contains("\"status\": \"missing\""));
    fs::remove_dir_all(&missing_required_dir).ok();

    let unrejected_dir = fixture_dir();
    let unrejected = run(
        &unrejected_dir,
        &["--forbid", "forbidden_unrejected_signal"],
        industrial_pipe_preview(),
    );
    assert!(!unrejected.status.success());
    let unrejected_json =
        fs::read_to_string(unrejected_dir.join("proof-run.json")).expect("unrejected proof json");
    assert!(unrejected_json.contains("forbidden_unrejected_signal"));
    assert!(unrejected_json.contains("\"status\": \"unrejected\""));
    fs::remove_dir_all(&unrejected_dir).ok();

    let broken_project = write_pipe_project_with_broken_material();
    let broken_dir = fixture_dir();
    let broken = Command::new(binary)
        .arg("asset-proof-run")
        .arg("--project")
        .arg(&broken_project)
        .arg("--asset")
        .arg("asset_graph.pipe_lab.rusted_pipe")
        .arg("--reference")
        .arg(industrial_pipe_preview())
        .arg("--preview-artifact")
        .arg(industrial_pipe_preview())
        .arg("--output")
        .arg(&broken_dir)
        .output()
        .expect("run broken proof");
    assert!(!broken.status.success());
    let broken_json = fs::read_to_string(broken_dir.join("proof-run.json")).expect("broken json");
    assert!(broken_json.contains("\"cook-project\""));
    assert!(broken_json.contains("strict cook reported"));
    fs::remove_dir_all(broken_project.parent().unwrap()).ok();
    fs::remove_dir_all(&broken_dir).ok();
}

#[test]
fn lesson_inspect_reports_lumen_learning_contract() {
    let binary = env!("CARGO_BIN_EXE_aster_assetc");
    let inspect = Command::new(binary)
        .arg("lesson-inspect")
        .arg("--project")
        .arg(lumen_project())
        .arg("--lesson")
        .arg("lesson.lumen_mining")
        .arg("--output-schema")
        .output()
        .expect("run lesson inspect");
    assert!(
        inspect.status.success(),
        "{}",
        String::from_utf8_lossy(&inspect.stderr)
    );
    let stdout = String::from_utf8_lossy(&inspect.stdout);
    assert!(stdout.contains("\"kind\": \"aster_learning_lesson_inspect\""));
    assert!(stdout.contains("\"status\": \"passed\""));
    assert!(stdout.contains("hypothesis.tool_affordance_gap"));
    assert!(stdout.contains("scaffold.pickaxe_prompt"));
    assert!(stdout.contains("Aster Learning Lesson Inspect"));
}

#[test]
fn learning_proof_run_writes_lumen_bundle_and_failures() {
    let output_dir = fixture_dir();
    let binary = env!("CARGO_BIN_EXE_aster_assetc");
    let proof = Command::new(binary)
        .arg("learning-proof-run")
        .arg("--project")
        .arg(lumen_project())
        .arg("--lesson")
        .arg("lesson.lumen_mining")
        .arg("--trace")
        .arg(lumen_learning_trace())
        .arg("--output")
        .arg(&output_dir)
        .arg("--output-schema")
        .output()
        .expect("run learning proof");
    assert!(
        proof.status.success(),
        "{}\n{}",
        String::from_utf8_lossy(&proof.stdout),
        String::from_utf8_lossy(&proof.stderr)
    );
    let stdout = String::from_utf8_lossy(&proof.stdout);
    assert!(stdout.contains("\"kind\": \"aster_learning_proof_run\""));
    assert!(stdout.contains("\"status\": \"passed\""));
    assert!(stdout.contains("\"objective_coverage\": 1.0"));
    assert!(output_dir.join("lesson-inspect.json").exists());
    assert!(output_dir.join("trace-forest.json").exists());
    assert!(output_dir.join("intervention-plan.json").exists());
    assert!(output_dir.join("learning-proof-run.json").exists());

    let bad_trace = output_dir.join("bad.trace.jsonl");
    fs::write(
        &bad_trace,
        r#"{"id":"t1","timestamp":1,"stage":"diagnose","event":"mining_attempt_without_tool","evidence_id":"evidence.mine_attempt","hypothesis_id":"hypothesis.tool_affordance_gap","misconception_id":"mine_without_tool"}
{"id":"t2","timestamp":2,"stage":"design","event":"select_scaffold","scaffold_id":"scaffold.pickaxe_prompt","metadata":{"rationale":"generic encouragement"}}
{"id":"t3","timestamp":3,"stage":"teach","event":"inventory_transfer","evidence_id":"evidence.pickaxe_pickup","objective_id":"objective.pickup_tool"}
"#,
    )
    .expect("bad trace");
    let bad_dir = fixture_dir();
    let bad = Command::new(binary)
        .arg("learning-proof-run")
        .arg("--project")
        .arg(lumen_project())
        .arg("--lesson")
        .arg("lesson.lumen_mining")
        .arg("--trace")
        .arg(&bad_trace)
        .arg("--output")
        .arg(&bad_dir)
        .output()
        .expect("run failing learning proof");
    assert!(!bad.status.success());
    let bad_json =
        fs::read_to_string(bad_dir.join("learning-proof-run.json")).expect("bad proof json");
    assert!(bad_json.contains("\"status\": \"failed\""));
    assert!(bad_json.contains("unsupported intervention"));
    assert!(bad_json.contains("objective is not covered"));
    fs::remove_dir_all(&output_dir).ok();
    fs::remove_dir_all(&bad_dir).ok();
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
