// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

mod agent_plan;
mod agent_tools;

use aster_content::{
    asset_database_diff_json, asset_fate_report_json, asset_foundry_report_json,
    asset_graph_inspect_report_json, asset_graph_report_json, bake_texture_to_ktx2,
    catalog_store_from_database, compile_scene_asset_to_cache, cook_lineage_diff_json,
    cook_lineage_report_json, cook_project, hex_hash, inspect_cache, inspect_texture,
    material_inspect_report_json, mesh_recipe_inspect_report_json, package_asset_graph,
    read_asset_database, report_asset_database, session_audit_report_json,
    write_asset_catalog_store, write_missing_asset_meta, CompileOptions, OriginPolicy,
};
use aster_runtime::{
    build_frame_plan, generic_http_json, memory_graph_query_json, memory_store_init_path,
    memory_store_trace_event_json, AsterRuntimeCamera, AsterRuntimeRenderObject,
    AsterRuntimeRenderPlanOptions, AsterRuntimeVec3,
};
use serde_json::{json, Value};
use std::collections::{BTreeMap, BTreeSet};
use std::fs;
use std::path::Path;
use std::path::PathBuf;

use crate::agent_plan::{agent_plan_report_json, asset_brief_report_json};
use crate::agent_tools::{
    agent_audit_report_json, agent_audit_report_markdown, agent_fix_headers_report_json,
    agent_native_audit_report_json, agent_native_audit_report_markdown, agent_review_report_json,
    agent_review_report_markdown, agent_runtime_audit_report_json,
    agent_runtime_audit_report_markdown,
};

fn self_check() {
    let object = AsterRuntimeRenderObject {
        entity_id: 1,
        object_index: 0,
        mesh_key: 1,
        material_key: 1,
        render_queue: 0,
        flags: 0,
        visibility_class: 0,
        position: AsterRuntimeVec3 {
            x: 0.0,
            y: 0.0,
            z: 4.0,
        },
        visibility_cell: AsterRuntimeVec3::default(),
        bounds_center: AsterRuntimeVec3 {
            x: 0.0,
            y: 0.0,
            z: 4.0,
        },
        bounds_radius: 0.5,
        opacity: 1.0,
        lod_max_distance: 0.0,
        lod_min_projected_radius: 0.0,
        portal_depth: 0.0,
        dynamic_mesh_generation: 0,
        perceptual_primitive_hash: 0,
        perceptual_sound_surface_class_hash: 0,
        perceptual_neural_irradiance_hash: 0,
        perceptual_neural_irradiance: AsterRuntimeVec3::default(),
        perceptual_material_memory: 0.0,
        perceptual_interaction_residue: 0.0,
        perceptual_contact_field: 0.0,
        perceptual_light_history: 0.0,
        perceptual_acoustic_occlusion: 0.0,
        perceptual_ecology_pressure: 0.0,
        perceptual_threat_gradient: 0.0,
        perceptual_traversal_pressure: 0.0,
        perceptual_semantic_lod: 0.0,
        perceptual_decision_impact: 0.0,
        perceptual_player_readable_cause: 0.0,
        perceptual_ai_cover_value: 0.0,
        perceptual_neural_irradiance_confidence: 0.0,
    };
    let camera = AsterRuntimeCamera {
        position: AsterRuntimeVec3::default(),
        forward: AsterRuntimeVec3 {
            x: 0.0,
            y: 0.0,
            z: 1.0,
        },
        right: AsterRuntimeVec3 {
            x: 1.0,
            y: 0.0,
            z: 0.0,
        },
        up: AsterRuntimeVec3 {
            x: 0.0,
            y: 1.0,
            z: 0.0,
        },
        vertical_fov: std::f32::consts::FRAC_PI_2,
        aspect_ratio: 1.0,
        near_plane: 0.01,
        far_plane: 20.0,
    };
    let plan = build_frame_plan(&[object], camera, AsterRuntimeRenderPlanOptions::default());
    assert_eq!(plan.summary.visible_objects, 1);
    println!(
        "aster_assetc self-check: visible={} groups={}",
        plan.summary.visible_objects, plan.summary.instance_groups
    );
}

fn usage() -> &'static str {
    "usage:
  aster_assetc orchestrates project/scene bundles; use aster_materialc and aster_texturec for single-domain packages.
  aster_assetc --self-check
  aster_assetc compile --input <file.scene> --output <file.astercache> [--origin keep|center|center-on-ground] [--unit-scale <float>]
  aster_assetc inspect --input <file.astercache>
  aster_assetc texture-inspect --input <texture> [--role albedo|normal|orm|height|emissive]
  aster_assetc texture-bake --input <texture> --output <file.ktx2> [--role albedo|normal|orm|height|emissive]
  aster_assetc material-inspect --input <file.astermat> [--asset-root <dir>]
  aster_assetc graph-inspect --input <file.astergraph>
  aster_assetc graph-package --input <file.astergraph> --output <dir>
  aster_assetc cook --project <file.asterproj> --platform desktop --output <dir>
  aster_assetc report --db <assetdb.asterdb.json>
  aster_assetc catalog-inspect --db <assetdb.asterdb.json>
  aster_assetc catalog-audit --db <assetdb.asterdb.json>
  aster_assetc catalog-sync --db <assetdb.asterdb.json> [--output <aster_catalogs.json>]
  aster_assetc mesh-import-inspect --input <mesh.obj|mesh.ply|mesh.stl>
  aster_assetc mesh-recipe-inspect --input <recipe.json>
  aster_assetc session-audit --input <history.jsonl> [--max-bytes <n>]
  aster_assetc agent-plan --project <file.asterproj> [--objective <text>] [--output-schema]
  aster_assetc agent-audit --repo <path> [--json|--markdown] [--output <file>]
  aster_assetc agent-fix-headers --repo <path> [--check|--write]
  aster_assetc agent-native-audit --repo <path> [--json|--markdown] [--output <file>]
  aster_assetc agent-runtime-audit --repo <path> [--json|--markdown] [--output <file>]
  aster_assetc agent-review --plan <file> --repo <path> [--json|--markdown] [--output <file>]
  aster_assetc asset-brief --project <file.asterproj> --asset <id> --reference <image> [--target <text>] [--require <signal>] [--forbid <signal>] [--output-schema]
  aster_assetc asset-proof-run --project <file.asterproj> --asset <id> --reference <image> --preview-artifact <image> --output <dir> [--target <text>] [--require <signal>] [--forbid <signal>] [--output-schema]
  aster_assetc lesson-inspect --project <file.asterproj> --lesson <id> [--output-schema]
  aster_assetc learning-proof-run --project <file.asterproj> --lesson <id> --trace <trace.jsonl> --output <dir> [--store <memory.sqlite>] [--output-schema]
  aster_assetc memory-proof-run --project <file.asterproj> --policy <id> --trace <typed-trace.jsonl> --store <memory.sqlite> --output <dir> [--output-schema]
  aster_assetc memory-bench-run --project <file.asterproj> --suite <id> --store <memory.sqlite> --output <dir> [--provider-url <url>] [--headers-json <json>] [--output-schema]
  aster_assetc memory-bench-compare --before <report.json> --after <report.json> [--output-schema]
  aster_assetc graph --db <assetdb.asterdb.json>
  aster_assetc fate --db <assetdb.asterdb.json> --asset <id-or-guid>
  aster_assetc diff --before <old.assetdb.asterdb.json> --after <new.assetdb.asterdb.json>
  aster_assetc lineage-diff --before <old.assetdb.asterdb.json> --after <new.assetdb.asterdb.json>
  aster_assetc lineage-report --db <assetdb.asterdb.json>
  aster_assetc guid-init --project <file.asterproj>"
}

fn value_after(args: &[String], name: &str) -> Option<String> {
    args.windows(2)
        .find(|window| window[0] == name)
        .map(|window| window[1].clone())
}

fn values_after(args: &[String], name: &str) -> Vec<String> {
    args.windows(2)
        .filter(|window| window[0] == name)
        .map(|window| window[1].clone())
        .collect()
}

fn write_or_print(args: &[String], text: String) -> Result<(), String> {
    if let Some(output) = value_after(args, "--output").map(PathBuf::from) {
        if let Some(parent) = output.parent() {
            if !parent.as_os_str().is_empty() {
                fs::create_dir_all(parent).map_err(|error| error.to_string())?;
            }
        }
        fs::write(&output, text).map_err(|error| error.to_string())?;
        println!("wrote {}", output.display());
    } else {
        println!("{text}");
    }
    Ok(())
}

fn compile_command(args: &[String]) -> Result<(), String> {
    let input = value_after(args, "--input")
        .map(PathBuf::from)
        .ok_or_else(|| "compile requires --input <file.scene>".to_string())?;
    let output = value_after(args, "--output")
        .map(PathBuf::from)
        .ok_or_else(|| "compile requires --output <file.astercache>".to_string())?;
    let origin_policy = value_after(args, "--origin")
        .map(|value| OriginPolicy::parse(&value).map_err(|error| error.to_string()))
        .transpose()?
        .unwrap_or(OriginPolicy::Keep);
    let unit_scale = value_after(args, "--unit-scale")
        .map(|value| {
            value
                .parse::<f32>()
                .map_err(|_| format!("invalid --unit-scale value '{value}'"))
        })
        .transpose()?
        .unwrap_or(1.0);

    let asset = compile_scene_asset_to_cache(
        &input,
        &output,
        CompileOptions {
            origin_policy,
            unit_scale,
        },
    )
    .map_err(|error| error.to_string())?;
    println!(
        "compiled {} -> {} materials={} meshes={} collision_meshes={} vertices={} indices={}",
        input.display(),
        output.display(),
        asset.metadata.material_count,
        asset.metadata.mesh_count,
        asset.metadata.collision_mesh_count,
        asset.metadata.total_vertices,
        asset.metadata.total_indices
    );
    Ok(())
}

fn inspect_command(args: &[String]) -> Result<(), String> {
    let input = value_after(args, "--input")
        .map(PathBuf::from)
        .ok_or_else(|| "inspect requires --input <file.astercache>".to_string())?;
    let summary = inspect_cache(input).map_err(|error| error.to_string())?;
    println!("{summary}");
    Ok(())
}

fn texture_inspect_command(args: &[String]) -> Result<(), String> {
    let input = value_after(args, "--input")
        .map(PathBuf::from)
        .ok_or_else(|| "texture-inspect requires --input <texture>".to_string())?;
    let role = value_after(args, "--role").unwrap_or_else(|| "unknown".to_string());
    let summary = inspect_texture(&input, &role).map_err(|error| error.to_string())?;
    println!(
        "texture {} role={} kind={} colorspace={} format={} size={}x{} mips={} hash={}",
        input.display(),
        summary.role,
        summary.kind.as_str(),
        summary.color_space,
        summary.format,
        summary.width,
        summary.height,
        summary.mip_count,
        hex_hash(&summary.source_hash)
    );
    for diagnostic in summary.diagnostics {
        println!("warning: {diagnostic}");
    }
    Ok(())
}

fn texture_bake_command(args: &[String]) -> Result<(), String> {
    let input = value_after(args, "--input")
        .map(PathBuf::from)
        .ok_or_else(|| "texture-bake requires --input <texture>".to_string())?;
    let output = value_after(args, "--output")
        .map(PathBuf::from)
        .ok_or_else(|| "texture-bake requires --output <file.ktx2>".to_string())?;
    let role = value_after(args, "--role").unwrap_or_else(|| "unknown".to_string());
    let summary =
        bake_texture_to_ktx2(&input, &output, &role).map_err(|error| error.to_string())?;
    println!(
        "baked {} -> {} kind={} colorspace={} size={}x{} mips={} hash={}",
        input.display(),
        output.display(),
        summary.kind.as_str(),
        summary.color_space,
        summary.width,
        summary.height,
        summary.mip_count,
        hex_hash(&summary.source_hash)
    );
    Ok(())
}

fn material_inspect_command(args: &[String]) -> Result<(), String> {
    let input = value_after(args, "--input")
        .map(PathBuf::from)
        .ok_or_else(|| "material-inspect requires --input <file.astermat>".to_string())?;
    let asset_root = value_after(args, "--asset-root")
        .map(PathBuf::from)
        .unwrap_or_default();
    let report =
        material_inspect_report_json(&input, &asset_root).map_err(|error| error.to_string())?;
    println!("{report}");
    Ok(())
}

fn graph_inspect_command(args: &[String]) -> Result<(), String> {
    let input = value_after(args, "--input")
        .map(PathBuf::from)
        .ok_or_else(|| "graph-inspect requires --input <file.astergraph>".to_string())?;
    let report = asset_graph_inspect_report_json(&input).map_err(|error| error.to_string())?;
    println!("{report}");
    Ok(())
}

fn graph_package_command(args: &[String]) -> Result<(), String> {
    let input = value_after(args, "--input")
        .map(PathBuf::from)
        .ok_or_else(|| "graph-package requires --input <file.astergraph>".to_string())?;
    let output = value_after(args, "--output")
        .map(PathBuf::from)
        .ok_or_else(|| "graph-package requires --output <dir>".to_string())?;
    let cooked = package_asset_graph(&input, &output).map_err(|error| error.to_string())?;
    println!(
        "packaged {} -> {} report={} nodes={} score={}",
        input.display(),
        cooked.graph_bin_path.display(),
        cooked.report_path.display(),
        cooked.graph_bin.nodes.len(),
        cooked.graph_bin.quality.score
    );
    Ok(())
}

fn cook_command(args: &[String]) -> Result<(), String> {
    let project = value_after(args, "--project")
        .map(PathBuf::from)
        .ok_or_else(|| "cook requires --project <file.asterproj>".to_string())?;
    let output = value_after(args, "--output")
        .map(PathBuf::from)
        .ok_or_else(|| "cook requires --output <dir>".to_string())?;
    let platform = value_after(args, "--platform").unwrap_or_else(|| "desktop".to_string());
    let result = cook_project(&project, &platform, &output).map_err(|error| error.to_string())?;
    println!(
        "cooked {} -> {} manifest={} assets={}",
        project.display(),
        result.database_path.display(),
        result.manifest_path.display(),
        result.database.records.len()
    );
    println!("{}", report_asset_database(&result.database));
    if result.error_count > 0 {
        return Err(format!(
            "strict cook failed with {} error(s) and {} warning(s)",
            result.error_count, result.warning_count
        ));
    }
    Ok(())
}

fn report_command(args: &[String]) -> Result<(), String> {
    let db = value_after(args, "--db")
        .map(PathBuf::from)
        .ok_or_else(|| "report requires --db <assetdb.asterdb.json>".to_string())?;
    let database = read_asset_database(&db).map_err(|error| error.to_string())?;
    println!("{}", report_asset_database(&database));
    for record in database.records {
        println!(
            "{} kind={} source={} outputs={} diagnostics={} world_ready={} world_ready_hash={}",
            record.id,
            record.kind,
            record.source_path,
            record.outputs.len(),
            record.diagnostics.len(),
            record.fate_report.world_ready.accepted,
            record.fate_report.world_ready.report_hash
        );
        for diagnostic in record.diagnostics {
            println!("{}: {}", diagnostic.severity, diagnostic.message);
        }
    }
    Ok(())
}

fn catalog_inspect_command(args: &[String]) -> Result<(), String> {
    let db = value_after(args, "--db")
        .map(PathBuf::from)
        .ok_or_else(|| "catalog-inspect requires --db <assetdb.asterdb.json>".to_string())?;
    let database = read_asset_database(&db).map_err(|error| error.to_string())?;
    let mut by_kind = BTreeMap::<String, usize>::new();
    let mut production_ready = 0usize;
    for record in &database.records {
        *by_kind.entry(record.kind.clone()).or_default() += 1;
        if record.fate_report.production_ready {
            production_ready += 1;
        }
    }
    println!(
        "catalog db={} platform={} assets={} production_ready={} graph_nodes={} graph_edges={}",
        db.display(),
        database.platform,
        database.records.len(),
        production_ready,
        database.asset_graph.nodes.len(),
        database.asset_graph.edges.len()
    );
    let store = catalog_store_from_database(&database);
    for catalog in &store.catalogs {
        let count = catalog
            .metadata
            .get("asset_count")
            .cloned()
            .unwrap_or_else(|| "0".to_string());
        println!(
            "catalog id={} path={} simple_name={} assets={} tags={}",
            catalog.id,
            catalog.path,
            catalog.simple_name,
            count,
            catalog.tags.join(",")
        );
    }
    for (kind, count) in by_kind {
        println!(
            "catalog-summary Assets/{} assets={}",
            title_case(&kind),
            count
        );
    }
    for edge in &database.asset_graph.edges {
        println!(
            "dependency from={} to={} role={} present={}",
            edge.from, edge.to, edge.role, edge.present
        );
    }
    Ok(())
}

fn catalog_sync_command(args: &[String]) -> Result<(), String> {
    let db = value_after(args, "--db")
        .map(PathBuf::from)
        .ok_or_else(|| "catalog-sync requires --db <assetdb.asterdb.json>".to_string())?;
    let output = value_after(args, "--output")
        .map(PathBuf::from)
        .unwrap_or_else(|| {
            db.parent()
                .unwrap_or_else(|| Path::new("."))
                .join("aster_catalogs.json")
        });
    let database = read_asset_database(&db).map_err(|error| error.to_string())?;
    let count = write_asset_catalog_store(&database, &output).map_err(|error| error.to_string())?;
    println!(
        "catalog-sync db={} output={} catalogs={}",
        db.display(),
        output.display(),
        count
    );
    Ok(())
}

fn catalog_audit_command(args: &[String]) -> Result<(), String> {
    let db = value_after(args, "--db")
        .map(PathBuf::from)
        .ok_or_else(|| "catalog-audit requires --db <assetdb.asterdb.json>".to_string())?;
    let database = read_asset_database(&db).map_err(|error| error.to_string())?;
    println!(
        "{}",
        asset_foundry_report_json(&database).map_err(|error| error.to_string())?
    );
    Ok(())
}

fn title_case(value: &str) -> String {
    let mut chars = value.chars();
    match chars.next() {
        Some(first) => first.to_uppercase().collect::<String>() + chars.as_str(),
        None => "Asset".to_string(),
    }
}

fn mesh_import_inspect_command(args: &[String]) -> Result<(), String> {
    let input = value_after(args, "--input")
        .map(PathBuf::from)
        .ok_or_else(|| {
            "mesh-import-inspect requires --input <mesh.obj|mesh.ply|mesh.stl>".to_string()
        })?;
    let bytes = fs::read(&input).map_err(|error| error.to_string())?;
    let text = String::from_utf8_lossy(&bytes);
    let format = mesh_format_for_path(&input);
    let (vertices, indices) = match format.as_str() {
        "obj" => inspect_obj_text(&text),
        "ply" => inspect_ply_text(&text),
        "stl" => inspect_stl_text(&text),
        _ => {
            return Err(format!(
                "unsupported mesh import format for {}",
                input.display()
            ))
        }
    };
    println!(
        "mesh-import input={} format={} vertices={} indices={} source_hash={}",
        input.display(),
        format,
        vertices,
        indices,
        hex_u64(hash_bytes(&bytes))
    );
    Ok(())
}

fn mesh_recipe_inspect_command(args: &[String]) -> Result<(), String> {
    let input = value_after(args, "--input")
        .map(PathBuf::from)
        .ok_or_else(|| "mesh-recipe-inspect requires --input <recipe.json>".to_string())?;
    println!(
        "{}",
        mesh_recipe_inspect_report_json(&input).map_err(|error| error.to_string())?
    );
    Ok(())
}

fn session_audit_command(args: &[String]) -> Result<(), String> {
    let input = value_after(args, "--input")
        .map(PathBuf::from)
        .ok_or_else(|| "session-audit requires --input <history.jsonl>".to_string())?;
    let max_bytes = value_after(args, "--max-bytes")
        .map(|value| {
            value
                .parse::<usize>()
                .map_err(|_| format!("invalid --max-bytes value '{value}'"))
        })
        .transpose()?;
    println!(
        "{}",
        session_audit_report_json(&input, max_bytes).map_err(|error| error.to_string())?
    );
    Ok(())
}

fn agent_plan_command(args: &[String]) -> Result<(), String> {
    let project = value_after(args, "--project")
        .map(PathBuf::from)
        .ok_or_else(|| "agent-plan requires --project <file.asterproj>".to_string())?;
    let objective = value_after(args, "--objective");
    let include_schema = args.iter().any(|arg| arg == "--output-schema");
    println!(
        "{}",
        agent_plan_report_json(&project, objective.as_deref(), include_schema)?
    );
    Ok(())
}

fn agent_audit_command(args: &[String]) -> Result<(), String> {
    let repo = value_after(args, "--repo")
        .map(PathBuf::from)
        .ok_or_else(|| "agent-audit requires --repo <path>".to_string())?;
    let markdown = args.iter().any(|arg| arg == "--markdown");
    let text = if markdown {
        agent_audit_report_markdown(&repo)?
    } else {
        agent_audit_report_json(&repo)?
    };
    write_or_print(args, text)
}

fn agent_fix_headers_command(args: &[String]) -> Result<(), String> {
    let repo = value_after(args, "--repo")
        .map(PathBuf::from)
        .ok_or_else(|| "agent-fix-headers requires --repo <path>".to_string())?;
    let write = args.iter().any(|arg| arg == "--write");
    let check = args.iter().any(|arg| arg == "--check");
    if write == check {
        return Err("agent-fix-headers requires exactly one of --check or --write".to_string());
    }
    let text = agent_fix_headers_report_json(&repo, write)?;
    println!("{text}");
    Ok(())
}

fn agent_native_audit_command(args: &[String]) -> Result<(), String> {
    let repo = value_after(args, "--repo")
        .map(PathBuf::from)
        .ok_or_else(|| "agent-native-audit requires --repo <path>".to_string())?;
    let markdown = args.iter().any(|arg| arg == "--markdown");
    let text = if markdown {
        agent_native_audit_report_markdown(&repo)?
    } else {
        agent_native_audit_report_json(&repo)?
    };
    write_or_print(args, text)
}

fn agent_runtime_audit_command(args: &[String]) -> Result<(), String> {
    let repo = value_after(args, "--repo")
        .map(PathBuf::from)
        .ok_or_else(|| "agent-runtime-audit requires --repo <path>".to_string())?;
    let markdown = args.iter().any(|arg| arg == "--markdown");
    let text = if markdown {
        agent_runtime_audit_report_markdown(&repo)?
    } else {
        agent_runtime_audit_report_json(&repo)?
    };
    write_or_print(args, text)
}

fn agent_review_command(args: &[String]) -> Result<(), String> {
    let repo = value_after(args, "--repo")
        .map(PathBuf::from)
        .ok_or_else(|| "agent-review requires --repo <path>".to_string())?;
    let plan = value_after(args, "--plan")
        .map(PathBuf::from)
        .ok_or_else(|| "agent-review requires --plan <file>".to_string())?;
    let markdown = args.iter().any(|arg| arg == "--markdown");
    let text = if markdown {
        agent_review_report_markdown(&plan, &repo)?
    } else {
        agent_review_report_json(&plan, &repo)?
    };
    write_or_print(args, text)
}

fn asset_brief_command(args: &[String]) -> Result<(), String> {
    let project = value_after(args, "--project")
        .map(PathBuf::from)
        .ok_or_else(|| "asset-brief requires --project <file.asterproj>".to_string())?;
    let asset = value_after(args, "--asset")
        .ok_or_else(|| "asset-brief requires --asset <id>".to_string())?;
    let references = values_after(args, "--reference")
        .into_iter()
        .map(PathBuf::from)
        .collect::<Vec<_>>();
    if references.is_empty() {
        return Err("asset-brief requires at least one --reference <image>".to_string());
    }
    let target = value_after(args, "--target");
    let required = values_after(args, "--require");
    let forbidden = values_after(args, "--forbid");
    let include_schema = args.iter().any(|arg| arg == "--output-schema");
    println!(
        "{}",
        asset_brief_report_json(
            &project,
            &asset,
            &references,
            target.as_deref(),
            &required,
            &forbidden,
            include_schema,
        )?
    );
    Ok(())
}

fn normalize_path(path: &Path) -> String {
    path.components()
        .as_path()
        .to_string_lossy()
        .replace('\\', "/")
}

fn signal_ids(value: &Value, key: &str) -> Vec<String> {
    value
        .get(key)
        .and_then(Value::as_array)
        .into_iter()
        .flatten()
        .filter_map(|row| row.get("id").and_then(Value::as_str).map(str::to_string))
        .collect()
}

fn string_set_at(value: &Value, path: &[&str]) -> BTreeSet<String> {
    let mut current = value;
    for key in path {
        let Some(next) = current.get(*key) else {
            return BTreeSet::new();
        };
        current = next;
    }
    current
        .as_array()
        .into_iter()
        .flatten()
        .filter_map(Value::as_str)
        .map(str::to_string)
        .collect()
}

fn number_at(value: &Value, path: &[&str]) -> Option<f64> {
    let mut current = value;
    for key in path {
        current = current.get(*key)?;
    }
    current.as_f64()
}

fn asset_proof_run_output_schema() -> Value {
    json!({
        "$schema": "https://json-schema.org/draft/2020-12/schema",
        "title": "Aster Agent Asset Proof Run",
        "type": "object",
        "required": [
            "schema_version",
            "kind",
            "status",
            "passed",
            "project",
            "asset",
            "bundle",
            "validation",
            "required_signals",
            "forbidden_signals",
            "surface_stack",
            "diagnostics"
        ],
        "properties": {
            "schema_version": { "const": 1 },
            "kind": { "const": "aster_agent_asset_proof_run" },
            "status": { "enum": ["passed", "failed"] },
            "passed": { "type": "boolean" },
            "project": { "type": "object" },
            "asset": { "type": "object" },
            "bundle": { "type": "object" },
            "validation": { "type": "array" },
            "required_signals": { "type": "array" },
            "forbidden_signals": { "type": "array" },
            "surface_stack": { "type": "object" },
            "diagnostics": { "type": "array" }
        }
    })
}

fn validation_row(id: &str, status: &str, artifact: impl AsRef<Path>, notes: &str) -> Value {
    json!({
        "id": id,
        "status": status,
        "artifact": normalize_path(artifact.as_ref()),
        "notes": notes
    })
}

fn diagnostic(severity: &str, path: &str, message: impl Into<String>) -> Value {
    json!({
        "severity": severity,
        "path": path,
        "message": message.into()
    })
}

fn asset_proof_run_command(args: &[String]) -> Result<(), String> {
    let project = value_after(args, "--project")
        .map(PathBuf::from)
        .ok_or_else(|| "asset-proof-run requires --project <file.asterproj>".to_string())?;
    let asset = value_after(args, "--asset")
        .ok_or_else(|| "asset-proof-run requires --asset <id>".to_string())?;
    let references = values_after(args, "--reference")
        .into_iter()
        .map(PathBuf::from)
        .collect::<Vec<_>>();
    if references.is_empty() {
        return Err("asset-proof-run requires at least one --reference <image>".to_string());
    }
    let preview_artifact = value_after(args, "--preview-artifact")
        .map(PathBuf::from)
        .ok_or_else(|| "asset-proof-run requires --preview-artifact <image>".to_string())?;
    let output = value_after(args, "--output")
        .map(PathBuf::from)
        .ok_or_else(|| "asset-proof-run requires --output <dir>".to_string())?;
    let platform = value_after(args, "--platform").unwrap_or_else(|| "desktop".to_string());
    let target = value_after(args, "--target");
    let required = values_after(args, "--require");
    let forbidden = values_after(args, "--forbid");
    let include_schema = args.iter().any(|arg| arg == "--output-schema");

    fs::create_dir_all(&output).map_err(|error| error.to_string())?;
    let brief_path = output.join("brief.json");
    let graph_inspect_path = output.join("graph-inspect.json");
    let package_root = output.join("package");
    let cook_root = output.join("cooked");
    let proof_path = output.join("proof-run.json");

    let mut validation = Vec::new();
    let mut diagnostics = Vec::new();

    let brief_text = asset_brief_report_json(
        &project,
        &asset,
        &references,
        target.as_deref(),
        &required,
        &forbidden,
        true,
    )?;
    fs::write(&brief_path, &brief_text).map_err(|error| error.to_string())?;
    validation.push(validation_row(
        "brief",
        "passed",
        &brief_path,
        "asset brief generated",
    ));
    let brief_value: Value =
        serde_json::from_str(&brief_text).map_err(|error| error.to_string())?;

    let project_root = project.parent().unwrap_or_else(|| Path::new("."));
    let asset_path = brief_value
        .get("asset")
        .and_then(|row| row.get("path"))
        .and_then(Value::as_str)
        .unwrap_or_default();
    let graph_input = project_root.join(asset_path);
    if asset_path.is_empty() {
        diagnostics.push(diagnostic(
            "error",
            "$.asset.path",
            "asset path is missing from the project manifest",
        ));
    }

    let mut graph_value = Value::Null;
    match asset_graph_inspect_report_json(&graph_input) {
        Ok(graph_text) => {
            fs::write(&graph_inspect_path, &graph_text).map_err(|error| error.to_string())?;
            graph_value = serde_json::from_str(&graph_text).map_err(|error| error.to_string())?;
            validation.push(validation_row(
                "graph-inspect",
                "passed",
                &graph_inspect_path,
                "asset graph inspected",
            ));
        }
        Err(error) => {
            diagnostics.push(diagnostic("error", "$.graph_inspect", error.to_string()));
            validation.push(validation_row(
                "graph-inspect",
                "failed",
                &graph_inspect_path,
                "asset graph inspection failed",
            ));
        }
    }

    match package_asset_graph(&graph_input, &package_root) {
        Ok(cooked) => {
            let notes = format!(
                "graph packaged with {} node(s), score={}",
                cooked.graph_bin.nodes.len(),
                cooked.graph_bin.quality.score
            );
            validation.push(validation_row(
                "graph-package",
                "passed",
                &cooked.graph_bin_path,
                &notes,
            ));
        }
        Err(error) => {
            diagnostics.push(diagnostic("error", "$.graph_package", error.to_string()));
            validation.push(validation_row(
                "graph-package",
                "failed",
                &package_root,
                "asset graph package failed",
            ));
        }
    }

    let mut database_path = cook_root.join("assetdb.asterdb.json");
    let mut cook_error_count = 1usize;
    let mut cook_warning_count = 0usize;
    match cook_project(&project, &platform, &cook_root) {
        Ok(result) => {
            database_path = result.database_path.clone();
            cook_error_count = result.error_count;
            cook_warning_count = result.warning_count;
            let status = if result.error_count == 0 {
                "passed"
            } else {
                "failed"
            };
            let notes = format!(
                "assets={} errors={} warnings={}",
                result.database.records.len(),
                result.error_count,
                result.warning_count
            );
            validation.push(validation_row(
                "cook-project",
                status,
                &result.database_path,
                &notes,
            ));
            if result.error_count > 0 {
                diagnostics.push(diagnostic(
                    "error",
                    "$.cook_project",
                    format!("strict cook reported {} error(s)", result.error_count),
                ));
            }
        }
        Err(error) => {
            diagnostics.push(diagnostic("error", "$.cook_project", error.to_string()));
            validation.push(validation_row(
                "cook-project",
                "failed",
                &database_path,
                "project cook failed",
            ));
        }
    }

    let preview_exists = preview_artifact.exists();
    validation.push(validation_row(
        "preview-artifact",
        if preview_exists { "passed" } else { "failed" },
        &preview_artifact,
        if preview_exists {
            "preview artifact exists"
        } else {
            "preview artifact is missing"
        },
    ));
    if !preview_exists {
        diagnostics.push(diagnostic(
            "error",
            "$.preview_artifact",
            format!(
                "preview artifact does not exist: {}",
                preview_artifact.display()
            ),
        ));
    }

    let claimed_signals = string_set_at(&graph_value, &["factory_report", "visual_brief_claims"]);
    let rejected_signals =
        string_set_at(&graph_value, &["factory_report", "visual_brief_rejections"]);
    let required_rows = signal_ids(&brief_value, "required_signals")
        .into_iter()
        .map(|id| {
            let claimed = claimed_signals.contains(&id);
            if !claimed {
                diagnostics.push(diagnostic(
                    "error",
                    "$.required_signals",
                    format!("required signal is not claimed by the graph: {id}"),
                ));
            }
            json!({
                "id": id,
                "status": if claimed { "claimed" } else { "missing" },
                "source": if claimed {
                    "graph.factory_report.visual_brief_claims"
                } else {
                    "brief.required_signals"
                }
            })
        })
        .collect::<Vec<_>>();
    let forbidden_rows = signal_ids(&brief_value, "forbidden_signals")
        .into_iter()
        .map(|id| {
            let rejected = rejected_signals.contains(&id);
            if !rejected {
                diagnostics.push(diagnostic(
                    "error",
                    "$.forbidden_signals",
                    format!("forbidden signal is not rejected by the graph: {id}"),
                ));
            }
            json!({
                "id": id,
                "status": if rejected { "rejected" } else { "unrejected" },
                "source": if rejected {
                    "graph.factory_report.visual_brief_rejections"
                } else {
                    "brief.forbidden_signals"
                }
            })
        })
        .collect::<Vec<_>>();

    let physical_texel_density_min = number_at(
        &brief_value,
        &["surface_stack", "physical_texel_density", "minimum"],
    )
    .unwrap_or(1.0);
    let surface_specs = [
        ("physical_texel_density", physical_texel_density_min),
        ("height_normal_coupling", 0.0),
        ("roughness_height_coupling", 0.0),
        ("macro_frequency_breakup", 0.0),
        ("micro_frequency_breakup", 0.0),
    ];
    let mut surface_rows = Vec::new();
    for (field, minimum) in surface_specs {
        let observed = number_at(&graph_value, &["material", "params", field]);
        let passed = observed.map(|value| value >= minimum).unwrap_or(false);
        if !passed {
            diagnostics.push(diagnostic(
                "error",
                "$.surface_stack",
                format!("{field} is missing or below minimum {minimum}"),
            ));
        }
        surface_rows.push(json!({
            "id": field,
            "status": if passed { "passed" } else { "failed" },
            "observed": observed,
            "minimum": minimum,
            "source": "graph.material.params"
        }));
    }
    let surface_passed = surface_rows
        .iter()
        .all(|row| row.get("status").and_then(Value::as_str) == Some("passed"));

    let passed = diagnostics
        .iter()
        .all(|row| row.get("severity").and_then(Value::as_str) != Some("error"))
        && surface_passed
        && cook_error_count == 0;
    let mut report = json!({
        "schema_version": 1,
        "kind": "aster_agent_asset_proof_run",
        "status": if passed { "passed" } else { "failed" },
        "passed": passed,
        "project": {
            "path": normalize_path(&project),
            "platform": platform
        },
        "asset": {
            "id": asset,
            "path": normalize_path(&graph_input)
        },
        "bundle": {
            "root": normalize_path(&output),
            "brief": normalize_path(&brief_path),
            "graph_inspect": normalize_path(&graph_inspect_path),
            "package_root": normalize_path(&package_root),
            "cooked_database": normalize_path(&database_path),
            "preview_artifact": normalize_path(&preview_artifact),
            "proof_run": normalize_path(&proof_path)
        },
        "validation": validation,
        "required_signals": required_rows,
        "forbidden_signals": forbidden_rows,
        "surface_stack": {
            "status": if surface_passed { "passed" } else { "failed" },
            "checks": surface_rows
        },
        "cook": {
            "errors": cook_error_count,
            "warnings": cook_warning_count
        },
        "diagnostics": diagnostics,
        "rules": [
            "Preview generation is explicit; asset-proof-run consumes --preview-artifact and does not launch a renderer.",
            "Required visual signals must be claimed by the graph inspect factory report.",
            "Forbidden visual signals must be rejected by the graph inspect factory report.",
            "Surface-stack values must be numeric graph material parameters.",
            "Strict cook must report zero errors."
        ]
    });
    if include_schema {
        report["output_schema"] = asset_proof_run_output_schema();
    }
    let text = serde_json::to_string_pretty(&report).map_err(|error| error.to_string())?;
    fs::write(&proof_path, &text).map_err(|error| error.to_string())?;
    println!("{text}");
    if passed {
        Ok(())
    } else {
        Err(format!(
            "asset proof run failed; inspect {}",
            proof_path.display()
        ))
    }
}

fn json_str<'a>(value: &'a Value, key: &str) -> Option<&'a str> {
    value.get(key).and_then(Value::as_str)
}

fn string_vec_field(value: &Value, key: &str) -> Vec<String> {
    value
        .get(key)
        .and_then(Value::as_array)
        .into_iter()
        .flatten()
        .filter_map(Value::as_str)
        .map(str::to_string)
        .collect()
}

fn lesson_inspect_output_schema() -> Value {
    json!({
        "$schema": "https://json-schema.org/draft/2020-12/schema",
        "title": "Aster Learning Lesson Inspect",
        "type": "object",
        "required": [
            "schema_version",
            "kind",
            "status",
            "project",
            "lesson",
            "workflow_stages",
            "objectives",
            "evidence_refs",
            "scaffold_rules",
            "safety_checks",
            "diagnostics"
        ],
        "properties": {
            "schema_version": { "const": 1 },
            "kind": { "const": "aster_learning_lesson_inspect" },
            "status": { "enum": ["passed", "failed"] },
            "project": { "type": "object" },
            "lesson": { "type": "object" },
            "workflow_stages": { "type": "array" },
            "objectives": { "type": "array" },
            "evidence_refs": { "type": "array" },
            "scaffold_rules": { "type": "array" },
            "safety_checks": { "type": "array" },
            "diagnostics": { "type": "array" }
        }
    })
}

fn learning_proof_output_schema() -> Value {
    json!({
        "$schema": "https://json-schema.org/draft/2020-12/schema",
        "title": "Aster Learning Proof Run",
        "type": "object",
        "required": [
            "schema_version",
            "kind",
            "status",
            "passed",
            "lesson",
            "bundle",
            "objective_coverage",
            "workflow_coverage",
            "interventions",
            "safety",
            "diagnostics"
        ],
        "properties": {
            "schema_version": { "const": 1 },
            "kind": { "const": "aster_learning_proof_run" },
            "status": { "enum": ["passed", "failed"] },
            "passed": { "type": "boolean" },
            "lesson": { "type": "object" },
            "bundle": { "type": "object" },
            "objective_coverage": { "type": "number", "minimum": 0, "maximum": 1 },
            "workflow_coverage": { "type": "number", "minimum": 0, "maximum": 1 },
            "interventions": { "type": "array" },
            "safety": { "type": "object" },
            "diagnostics": { "type": "array" }
        }
    })
}

fn project_lesson_asset(project: &Value, lesson_id: &str) -> Option<Value> {
    project
        .get("assets")
        .and_then(Value::as_array)
        .into_iter()
        .flatten()
        .find(|asset| json_str(asset, "id") == Some(lesson_id))
        .cloned()
}

fn lesson_diagnostics(project: &Value, lesson: &Value) -> Vec<Value> {
    let mut diagnostics = Vec::new();
    if json_str(lesson, "id").unwrap_or_default().is_empty() {
        diagnostics.push(diagnostic("error", "$.id", "lesson id is required"));
    }
    if string_vec_field(lesson, "workflow_stages").is_empty() {
        diagnostics.push(diagnostic(
            "error",
            "$.workflow_stages",
            "lesson must declare workflow stages",
        ));
    }
    let asset_ids = project
        .get("assets")
        .and_then(Value::as_array)
        .into_iter()
        .flatten()
        .filter_map(|asset| json_str(asset, "id").map(str::to_string))
        .collect::<BTreeSet<_>>();
    let objective_ids = signal_ids(lesson, "objectives")
        .into_iter()
        .collect::<BTreeSet<_>>();
    let evidence_ids = signal_ids(lesson, "evidence_refs")
        .into_iter()
        .collect::<BTreeSet<_>>();
    let hypothesis_ids = signal_ids(lesson, "learner_state_hypotheses")
        .into_iter()
        .collect::<BTreeSet<_>>();
    let misconception_ids = signal_ids(lesson, "misconceptions")
        .into_iter()
        .collect::<BTreeSet<_>>();
    let scaffold_ids = signal_ids(lesson, "scaffold_rules")
        .into_iter()
        .collect::<BTreeSet<_>>();
    let workflow = string_vec_field(lesson, "workflow_stages")
        .into_iter()
        .collect::<BTreeSet<_>>();
    if objective_ids.is_empty() {
        diagnostics.push(diagnostic(
            "error",
            "$.objectives",
            "lesson must define objectives",
        ));
    }
    if evidence_ids.is_empty() {
        diagnostics.push(diagnostic(
            "error",
            "$.evidence_refs",
            "lesson must define evidence references",
        ));
    }
    for evidence in lesson
        .get("evidence_refs")
        .and_then(Value::as_array)
        .into_iter()
        .flatten()
    {
        let id = json_str(evidence, "id").unwrap_or_default();
        let asset = json_str(evidence, "asset").unwrap_or_default();
        if asset.is_empty() {
            diagnostics.push(diagnostic(
                "error",
                "$.evidence_refs",
                format!("{id} does not bind to an asset"),
            ));
        } else if !asset_ids.contains(asset) {
            diagnostics.push(diagnostic(
                "error",
                "$.evidence_refs",
                format!("{id} references missing project asset {asset}"),
            ));
        }
    }
    for objective in lesson
        .get("objectives")
        .and_then(Value::as_array)
        .into_iter()
        .flatten()
    {
        let id = json_str(objective, "id").unwrap_or_default();
        let evidence = string_vec_field(objective, "evidence");
        if evidence.is_empty() {
            diagnostics.push(diagnostic(
                "error",
                "$.objectives",
                format!("{id} has no evidence"),
            ));
        }
        for evidence_id in evidence {
            if !evidence_ids.contains(&evidence_id) {
                diagnostics.push(diagnostic(
                    "error",
                    "$.objectives",
                    format!("{id} references missing evidence {evidence_id}"),
                ));
            }
        }
    }
    for hypothesis in lesson
        .get("learner_state_hypotheses")
        .and_then(Value::as_array)
        .into_iter()
        .flatten()
    {
        let id = json_str(hypothesis, "id").unwrap_or_default();
        for evidence_id in string_vec_field(hypothesis, "evidence") {
            if !evidence_ids.contains(&evidence_id) {
                diagnostics.push(diagnostic(
                    "error",
                    "$.learner_state_hypotheses",
                    format!("{id} references missing evidence {evidence_id}"),
                ));
            }
        }
        for misconception_id in string_vec_field(hypothesis, "misconceptions") {
            if !misconception_ids.contains(&misconception_id) {
                diagnostics.push(diagnostic(
                    "error",
                    "$.learner_state_hypotheses",
                    format!("{id} references missing misconception {misconception_id}"),
                ));
            }
        }
    }
    for misconception in lesson
        .get("misconceptions")
        .and_then(Value::as_array)
        .into_iter()
        .flatten()
    {
        let id = json_str(misconception, "id").unwrap_or_default();
        for scaffold_id in string_vec_field(misconception, "remediation_scaffolds") {
            if !scaffold_ids.contains(&scaffold_id) {
                diagnostics.push(diagnostic(
                    "error",
                    "$.misconceptions",
                    format!("{id} references missing scaffold {scaffold_id}"),
                ));
            }
        }
    }
    for scaffold in lesson
        .get("scaffold_rules")
        .and_then(Value::as_array)
        .into_iter()
        .flatten()
    {
        let id = json_str(scaffold, "id").unwrap_or_default();
        let stage = json_str(scaffold, "stage").unwrap_or_default();
        if !workflow.contains(stage) {
            diagnostics.push(diagnostic(
                "error",
                "$.scaffold_rules",
                format!("{id} uses undeclared workflow stage {stage}"),
            ));
        }
        if scaffold
            .get("force_gameplay_change")
            .and_then(Value::as_bool)
            .unwrap_or(false)
        {
            diagnostics.push(diagnostic(
                "error",
                "$.scaffold_rules",
                format!("{id} forces gameplay change"),
            ));
        }
        for evidence_id in string_vec_field(scaffold, "evidence") {
            if !evidence_ids.contains(&evidence_id) {
                diagnostics.push(diagnostic(
                    "error",
                    "$.scaffold_rules",
                    format!("{id} references missing evidence {evidence_id}"),
                ));
            }
        }
        for hypothesis_id in string_vec_field(scaffold, "hypotheses") {
            if !hypothesis_ids.contains(&hypothesis_id) {
                diagnostics.push(diagnostic(
                    "error",
                    "$.scaffold_rules",
                    format!("{id} references missing hypothesis {hypothesis_id}"),
                ));
            }
        }
    }
    diagnostics
}

fn lesson_inspect_report_value(
    project_path: &Path,
    lesson_id: &str,
    include_schema: bool,
) -> Result<Value, String> {
    let project_bytes = fs::read(project_path).map_err(|error| error.to_string())?;
    let project: Value =
        serde_json::from_slice(&project_bytes).map_err(|error| error.to_string())?;
    let asset = project_lesson_asset(&project, lesson_id)
        .ok_or_else(|| format!("project does not contain lesson asset '{lesson_id}'"))?;
    if json_str(&asset, "kind") != Some("lesson") {
        return Err(format!("asset '{lesson_id}' is not kind=lesson"));
    }
    let project_root = project_path.parent().unwrap_or_else(|| Path::new("."));
    let lesson_path = project_root.join(json_str(&asset, "path").unwrap_or_default());
    let lesson_bytes = fs::read(&lesson_path).map_err(|error| error.to_string())?;
    let lesson: Value = serde_json::from_slice(&lesson_bytes).map_err(|error| error.to_string())?;
    let diagnostics = lesson_diagnostics(&project, &lesson);
    let passed = diagnostics
        .iter()
        .all(|row| row.get("severity").and_then(Value::as_str) != Some("error"));
    let mut report = json!({
        "schema_version": 1,
        "kind": "aster_learning_lesson_inspect",
        "status": if passed { "passed" } else { "failed" },
        "project": {
            "path": normalize_path(project_path),
            "name": json_str(&project, "name").unwrap_or("Aster Project")
        },
        "lesson": {
            "id": json_str(&lesson, "id").unwrap_or(lesson_id),
            "name": json_str(&lesson, "name").unwrap_or(""),
            "path": normalize_path(&lesson_path)
        },
        "workflow_stages": lesson.get("workflow_stages").cloned().unwrap_or_else(|| json!([])),
        "objectives": lesson.get("objectives").cloned().unwrap_or_else(|| json!([])),
        "evidence_refs": lesson.get("evidence_refs").cloned().unwrap_or_else(|| json!([])),
        "misconceptions": lesson.get("misconceptions").cloned().unwrap_or_else(|| json!([])),
        "learner_state_hypotheses": lesson
            .get("learner_state_hypotheses")
            .cloned()
            .unwrap_or_else(|| json!([])),
        "scaffold_rules": lesson.get("scaffold_rules").cloned().unwrap_or_else(|| json!([])),
        "safety_checks": lesson.get("safety_checks").cloned().unwrap_or_else(|| json!([])),
        "diagnostics": diagnostics,
        "rules": [
            "Objectives must be covered by declared evidence.",
            "Scaffolds must cite declared evidence and learner-state hypotheses.",
            "Learning proof must cover diagnose, design, teach, and evaluate stages.",
            "Pedagogical safety rejects false mastery and gameplay-forcing scaffolds."
        ]
    });
    if include_schema {
        report["output_schema"] = lesson_inspect_output_schema();
    }
    Ok(report)
}

fn lesson_inspect_command(args: &[String]) -> Result<(), String> {
    let project = value_after(args, "--project")
        .map(PathBuf::from)
        .ok_or_else(|| "lesson-inspect requires --project <file.asterproj>".to_string())?;
    let lesson = value_after(args, "--lesson")
        .ok_or_else(|| "lesson-inspect requires --lesson <id>".to_string())?;
    let include_schema = args.iter().any(|arg| arg == "--output-schema");
    let report = lesson_inspect_report_value(&project, &lesson, include_schema)?;
    println!(
        "{}",
        serde_json::to_string_pretty(&report).map_err(|error| error.to_string())?
    );
    Ok(())
}

fn read_learning_trace_jsonl(path: &Path) -> Result<Vec<Value>, String> {
    let text = fs::read_to_string(path).map_err(|error| error.to_string())?;
    let mut events = Vec::new();
    for (line_index, line) in text.lines().enumerate() {
        let trimmed = line.trim();
        if trimmed.is_empty() {
            continue;
        }
        let value: Value = serde_json::from_str(trimmed)
            .map_err(|error| format!("{}:{}: {error}", path.display(), line_index + 1))?;
        events.push(value);
    }
    Ok(events)
}

fn trace_forest_report(events: &[Value], lesson: &Value) -> Value {
    let observed_evidence = events
        .iter()
        .filter_map(|event| json_str(event, "evidence_id").map(str::to_string))
        .collect::<BTreeSet<_>>();
    let observed_hypotheses = events
        .iter()
        .filter_map(|event| json_str(event, "hypothesis_id").map(str::to_string))
        .collect::<BTreeSet<_>>();
    let observed_stages = events
        .iter()
        .filter_map(|event| json_str(event, "stage").map(str::to_string))
        .collect::<BTreeSet<_>>();
    let workflow = string_vec_field(lesson, "workflow_stages");
    json!({
        "schema_version": 1,
        "kind": "aster_learning_trace_forest",
        "event_count": events.len(),
        "observed_evidence": observed_evidence.iter().cloned().collect::<Vec<_>>(),
        "observed_hypotheses": observed_hypotheses.iter().cloned().collect::<Vec<_>>(),
        "observed_stages": observed_stages.iter().cloned().collect::<Vec<_>>(),
        "workflow_coverage": workflow
            .iter()
            .map(|stage| json!({
                "stage": stage,
                "observed": observed_stages.contains(stage)
            }))
            .collect::<Vec<_>>(),
        "events": events
    })
}

fn intervention_plan_report(events: &[Value], lesson: &Value) -> Value {
    let mut decisions = Vec::new();
    let scaffold_rules = lesson
        .get("scaffold_rules")
        .and_then(Value::as_array)
        .into_iter()
        .flatten()
        .filter_map(|rule| json_str(rule, "id").map(|id| (id.to_string(), rule.clone())))
        .collect::<BTreeMap<_, _>>();
    let mut observed_evidence = BTreeSet::new();
    let mut observed_hypotheses = BTreeSet::new();
    for event in events {
        if let Some(evidence) = json_str(event, "evidence_id") {
            observed_evidence.insert(evidence.to_string());
        }
        if let Some(hypothesis) = json_str(event, "hypothesis_id") {
            observed_hypotheses.insert(hypothesis.to_string());
        }
        let Some(scaffold_id) = json_str(event, "scaffold_id") else {
            continue;
        };
        let mut accepted = true;
        let mut diagnostics = Vec::new();
        if let Some(rule) = scaffold_rules.get(scaffold_id) {
            if json_str(rule, "stage") != json_str(event, "stage") {
                accepted = false;
                diagnostics.push("wrong workflow stage".to_string());
            }
            let rationale = event
                .get("metadata")
                .and_then(|metadata| metadata.get("rationale"))
                .and_then(Value::as_str)
                .unwrap_or_default();
            if rationale.is_empty() {
                accepted = false;
                diagnostics.push("missing rationale".to_string());
            }
            let mut rationale_grounded = false;
            for evidence in string_vec_field(rule, "evidence") {
                if !observed_evidence.contains(&evidence) {
                    accepted = false;
                    diagnostics.push(format!("evidence {evidence} was not observed"));
                }
                rationale_grounded = rationale_grounded || rationale.contains(&evidence);
            }
            for hypothesis in string_vec_field(rule, "hypotheses") {
                if !observed_hypotheses.contains(&hypothesis) {
                    accepted = false;
                    diagnostics.push(format!("hypothesis {hypothesis} was not diagnosed"));
                }
                rationale_grounded = rationale_grounded || rationale.contains(&hypothesis);
            }
            if !rationale_grounded {
                accepted = false;
                diagnostics.push("rationale does not cite evidence or hypothesis".to_string());
            }
        } else {
            accepted = false;
            diagnostics.push("scaffold is not declared by the lesson".to_string());
        }
        decisions.push(json!({
            "id": json_str(event, "id").unwrap_or("intervention"),
            "scaffold_id": scaffold_id,
            "stage": json_str(event, "stage").unwrap_or_default(),
            "accepted": accepted,
            "diagnostics": diagnostics
        }));
    }
    json!({
        "schema_version": 1,
        "kind": "aster_learning_intervention_plan",
        "decisions": decisions
    })
}

fn learning_proof_run_command(args: &[String]) -> Result<(), String> {
    let project = value_after(args, "--project")
        .map(PathBuf::from)
        .ok_or_else(|| "learning-proof-run requires --project <file.asterproj>".to_string())?;
    let lesson_id = value_after(args, "--lesson")
        .ok_or_else(|| "learning-proof-run requires --lesson <id>".to_string())?;
    let trace = value_after(args, "--trace")
        .map(PathBuf::from)
        .ok_or_else(|| "learning-proof-run requires --trace <trace.jsonl>".to_string())?;
    let output = value_after(args, "--output")
        .map(PathBuf::from)
        .ok_or_else(|| "learning-proof-run requires --output <dir>".to_string())?;
    let memory_store = value_after(args, "--store").map(PathBuf::from);
    let include_schema = args.iter().any(|arg| arg == "--output-schema");
    fs::create_dir_all(&output).map_err(|error| error.to_string())?;

    let inspect_path = output.join("lesson-inspect.json");
    let trace_path = output.join("trace-forest.json");
    let intervention_path = output.join("intervention-plan.json");
    let proof_path = output.join("learning-proof-run.json");

    let inspect = lesson_inspect_report_value(&project, &lesson_id, true)?;
    fs::write(
        &inspect_path,
        serde_json::to_string_pretty(&inspect).map_err(|error| error.to_string())?,
    )
    .map_err(|error| error.to_string())?;
    let lesson_path = inspect
        .get("lesson")
        .and_then(|lesson| lesson.get("path"))
        .and_then(Value::as_str)
        .ok_or_else(|| "lesson inspect did not return a lesson path".to_string())?;
    let lesson_text = fs::read_to_string(lesson_path).map_err(|error| error.to_string())?;
    let lesson: Value = serde_json::from_str(&lesson_text).map_err(|error| error.to_string())?;
    let events = read_learning_trace_jsonl(&trace)?;
    let mut memory_graph = json!(null);
    if let Some(store) = &memory_store {
        memory_store_init_path(&store.to_string_lossy())?;
        for event in &events {
            memory_store_trace_event_json(&store.to_string_lossy(), event)?;
        }
        memory_graph = serde_json::from_str(&memory_graph_query_json(
            &store.to_string_lossy(),
            "",
            "",
            64,
        )?)
        .unwrap_or_else(|_| json!({}));
    }
    let trace_forest = trace_forest_report(&events, &lesson);
    fs::write(
        &trace_path,
        serde_json::to_string_pretty(&trace_forest).map_err(|error| error.to_string())?,
    )
    .map_err(|error| error.to_string())?;
    let intervention_plan = intervention_plan_report(&events, &lesson);
    fs::write(
        &intervention_path,
        serde_json::to_string_pretty(&intervention_plan).map_err(|error| error.to_string())?,
    )
    .map_err(|error| error.to_string())?;

    let observed_evidence = events
        .iter()
        .filter_map(|event| json_str(event, "evidence_id").map(str::to_string))
        .collect::<BTreeSet<_>>();
    let observed_stages = events
        .iter()
        .filter_map(|event| json_str(event, "stage").map(str::to_string))
        .collect::<BTreeSet<_>>();
    let mut diagnostics = inspect
        .get("diagnostics")
        .and_then(Value::as_array)
        .cloned()
        .unwrap_or_default();
    let mut missing_objectives = Vec::new();
    let objectives = lesson
        .get("objectives")
        .and_then(Value::as_array)
        .into_iter()
        .flatten()
        .collect::<Vec<_>>();
    for objective in &objectives {
        let covered = string_vec_field(objective, "evidence")
            .into_iter()
            .all(|evidence| observed_evidence.contains(&evidence));
        if !covered {
            let id = json_str(objective, "id").unwrap_or("objective");
            missing_objectives.push(id.to_string());
            diagnostics.push(diagnostic(
                "error",
                "$.objectives",
                format!("objective is not covered by trace evidence: {id}"),
            ));
        }
    }
    let workflow = string_vec_field(&lesson, "workflow_stages");
    let mut missing_stages = Vec::new();
    for stage in &workflow {
        if !observed_stages.contains(stage) {
            missing_stages.push(stage.clone());
            diagnostics.push(diagnostic(
                "error",
                "$.workflow_stages",
                format!("workflow stage is missing from trace: {stage}"),
            ));
        }
    }
    let decisions = intervention_plan
        .get("decisions")
        .and_then(Value::as_array)
        .cloned()
        .unwrap_or_default();
    for decision in &decisions {
        if !decision
            .get("accepted")
            .and_then(Value::as_bool)
            .unwrap_or(false)
        {
            diagnostics.push(diagnostic(
                "error",
                "$.interventions",
                format!(
                    "unsupported intervention: {}",
                    json_str(decision, "scaffold_id").unwrap_or("unknown")
                ),
            ));
        }
    }
    let mut safety_failures = Vec::new();
    let mut observed_until_now = BTreeSet::new();
    for event in &events {
        if let Some(evidence) = json_str(event, "evidence_id") {
            observed_until_now.insert(evidence.to_string());
        }
        if event
            .get("claims_mastery")
            .and_then(Value::as_bool)
            .unwrap_or(false)
        {
            let all_covered = objectives.iter().all(|objective| {
                string_vec_field(objective, "evidence")
                    .into_iter()
                    .all(|evidence| observed_until_now.contains(&evidence))
            });
            if !all_covered {
                safety_failures.push("false mastery claim".to_string());
            }
        }
    }
    for failure in &safety_failures {
        diagnostics.push(diagnostic("error", "$.safety", failure.as_str()));
    }
    let objective_coverage = if objectives.is_empty() {
        0.0
    } else {
        (objectives.len() - missing_objectives.len()) as f64 / objectives.len() as f64
    };
    let workflow_coverage = if workflow.is_empty() {
        0.0
    } else {
        (workflow.len() - missing_stages.len()) as f64 / workflow.len() as f64
    };
    let passed = diagnostics
        .iter()
        .all(|row| row.get("severity").and_then(Value::as_str) != Some("error"));
    let mut report = json!({
        "schema_version": 1,
        "kind": "aster_learning_proof_run",
        "status": if passed { "passed" } else { "failed" },
        "passed": passed,
        "lesson": {
            "id": lesson_id,
            "path": lesson_path
        },
        "bundle": {
            "root": normalize_path(&output),
            "lesson_inspect": normalize_path(&inspect_path),
            "trace_forest": normalize_path(&trace_path),
            "intervention_plan": normalize_path(&intervention_path),
            "learning_proof_run": normalize_path(&proof_path)
        },
        "objective_coverage": objective_coverage,
        "workflow_coverage": workflow_coverage,
        "missing_objectives": missing_objectives,
        "missing_workflow_stages": missing_stages,
        "interventions": decisions,
        "safety": {
            "accepted": safety_failures.is_empty(),
            "failures": safety_failures
        },
        "memory_graph": memory_graph,
        "diagnostics": diagnostics,
        "rules": [
            "Passing requires objective coverage.",
            "Passing requires diagnose, design, teach, and evaluate workflow evidence.",
            "Interventions must cite declared evidence or learner-state hypotheses.",
            "False mastery is rejected.",
            "When --store is provided, learning proof also writes typed trace rows to the real SQLite memory graph."
        ]
    });
    if include_schema {
        report["output_schema"] = learning_proof_output_schema();
    }
    let text = serde_json::to_string_pretty(&report).map_err(|error| error.to_string())?;
    fs::write(&proof_path, &text).map_err(|error| error.to_string())?;
    println!("{text}");
    if passed {
        Ok(())
    } else {
        Err(format!(
            "learning proof run failed; inspect {}",
            proof_path.display()
        ))
    }
}

fn memory_proof_output_schema() -> Value {
    json!({
        "$schema": "https://json-schema.org/draft/2020-12/schema",
        "title": "Aster Memory Proof Run",
        "type": "object",
        "required": ["schema_version", "kind", "status", "store", "typed_trace_event_count", "graph_query", "diagnostics"],
        "properties": {
            "schema_version": { "const": 1 },
            "kind": { "const": "aster_memory_proof_run" },
            "status": { "enum": ["passed", "failed"] },
            "store": { "type": "string" },
            "typed_trace_event_count": { "type": "integer", "minimum": 0 },
            "graph_query": { "type": "object" },
            "diagnostics": { "type": "array" }
        }
    })
}

fn memory_bench_output_schema() -> Value {
    json!({
        "$schema": "https://json-schema.org/draft/2020-12/schema",
        "title": "Aster Memory Benchmark Run",
        "type": "object",
        "required": ["schema_version", "kind", "status", "blocked", "provider", "score", "diagnostics"],
        "properties": {
            "schema_version": { "const": 1 },
            "kind": { "const": "aster_memory_benchmark_run" },
            "status": { "enum": ["passed", "failed", "blocked"] },
            "blocked": { "type": "boolean" },
            "provider": { "type": "object" },
            "score": { "type": "number", "minimum": 0, "maximum": 1 },
            "diagnostics": { "type": "array" }
        }
    })
}

fn memory_compare_output_schema() -> Value {
    json!({
        "$schema": "https://json-schema.org/draft/2020-12/schema",
        "title": "Aster Memory Benchmark Compare",
        "type": "object",
        "required": ["schema_version", "kind", "status", "before_score", "after_score", "delta"],
        "properties": {
            "schema_version": { "const": 1 },
            "kind": { "const": "aster_memory_benchmark_compare" },
            "status": { "enum": ["passed", "regressed"] },
            "before_score": { "type": "number" },
            "after_score": { "type": "number" },
            "delta": { "type": "number" }
        }
    })
}

fn memory_proof_run_command(args: &[String]) -> Result<(), String> {
    let project = value_after(args, "--project")
        .map(PathBuf::from)
        .ok_or_else(|| "memory-proof-run requires --project <file.asterproj>".to_string())?;
    let policy = value_after(args, "--policy")
        .ok_or_else(|| "memory-proof-run requires --policy <id>".to_string())?;
    let trace = value_after(args, "--trace")
        .map(PathBuf::from)
        .ok_or_else(|| "memory-proof-run requires --trace <typed-trace.jsonl>".to_string())?;
    let store = value_after(args, "--store")
        .map(PathBuf::from)
        .ok_or_else(|| "memory-proof-run requires --store <memory.sqlite>".to_string())?;
    let output = value_after(args, "--output")
        .map(PathBuf::from)
        .ok_or_else(|| "memory-proof-run requires --output <dir>".to_string())?;
    let include_schema = args.iter().any(|arg| arg == "--output-schema");
    fs::create_dir_all(&output).map_err(|error| error.to_string())?;
    memory_store_init_path(&store.to_string_lossy())?;
    let events = read_learning_trace_jsonl(&trace)?;
    for event in &events {
        memory_store_trace_event_json(&store.to_string_lossy(), event)?;
    }
    let graph_json = memory_graph_query_json(&store.to_string_lossy(), "", "", 32)?;
    let graph_query: Value =
        serde_json::from_str(&graph_json).map_err(|error| error.to_string())?;
    let diagnostics = Vec::<Value>::new();
    let passed = !events.is_empty();
    let proof_path = output.join("memory-proof-run.json");
    let mut report = json!({
        "schema_version": 1,
        "kind": "aster_memory_proof_run",
        "status": if passed { "passed" } else { "failed" },
        "project": normalize_path(&project),
        "policy": policy,
        "trace": normalize_path(&trace),
        "store": normalize_path(&store),
        "typed_trace_event_count": events.len(),
        "graph_query": graph_query,
        "diagnostics": diagnostics,
        "rules": [
            "Proof uses a real SQLite graph store.",
            "Typed trace rows are inserted before graph query proof.",
            "No fake provider response is generated by memory-proof-run."
        ]
    });
    if include_schema {
        report["output_schema"] = memory_proof_output_schema();
    }
    let text = serde_json::to_string_pretty(&report).map_err(|error| error.to_string())?;
    fs::write(&proof_path, &text).map_err(|error| error.to_string())?;
    println!("{text}");
    if passed {
        Ok(())
    } else {
        Err(format!(
            "memory proof run failed; inspect {}",
            proof_path.display()
        ))
    }
}

fn memory_bench_run_command(args: &[String]) -> Result<(), String> {
    let project = value_after(args, "--project")
        .map(PathBuf::from)
        .ok_or_else(|| "memory-bench-run requires --project <file.asterproj>".to_string())?;
    let suite = value_after(args, "--suite")
        .ok_or_else(|| "memory-bench-run requires --suite <id>".to_string())?;
    let store = value_after(args, "--store")
        .map(PathBuf::from)
        .ok_or_else(|| "memory-bench-run requires --store <memory.sqlite>".to_string())?;
    let output = value_after(args, "--output")
        .map(PathBuf::from)
        .ok_or_else(|| "memory-bench-run requires --output <dir>".to_string())?;
    let provider_url = value_after(args, "--provider-url")
        .or_else(|| std::env::var("ASTER_MEMORY_PROVIDER_URL").ok());
    let headers_json = value_after(args, "--headers-json")
        .or_else(|| std::env::var("ASTER_MEMORY_PROVIDER_HEADERS_JSON").ok())
        .unwrap_or_else(|| "{}".to_string());
    let include_schema = args.iter().any(|arg| arg == "--output-schema");
    fs::create_dir_all(&output).map_err(|error| error.to_string())?;
    memory_store_init_path(&store.to_string_lossy())?;
    let graph_json = memory_graph_query_json(&store.to_string_lossy(), "", "", 64)?;
    let request = json!({
        "schema_version": 1,
        "kind": "aster_memory_benchmark_provider_request",
        "project": normalize_path(&project),
        "suite": suite,
        "store": normalize_path(&store),
        "allowed_actions": ["read", "write", "evict", "replay", "scaffold", "stop"],
        "graph_query": serde_json::from_str::<Value>(&graph_json).unwrap_or_else(|_| json!({})),
        "requirements": [
            "memory-isolated evaluation",
            "ablation",
            "regression replay",
            "conflict resolution",
            "forgetting pressure",
            "counterfactual replay",
            "scaffold selection"
        ]
    });
    let report_path = output.join("memory-bench-run.json");
    let mut blocked = false;
    let mut diagnostics = Vec::<Value>::new();
    let mut provider = json!({"configured": false});
    let mut score = 0.0;
    if let Some(url) = provider_url {
        match generic_http_json(
            &url,
            "POST",
            &headers_json,
            &serde_json::to_string(&request).map_err(|error| error.to_string())?,
            30000,
        ) {
            Ok((status, body)) => {
                let accepted = (200..300).contains(&status);
                score = if accepted && body.contains("\"action\"") {
                    1.0
                } else {
                    0.5
                };
                provider = json!({
                    "configured": true,
                    "url": url,
                    "status_code": status,
                    "response_artifact": "provider-response.json"
                });
                fs::write(
                    output.join("provider-request.json"),
                    serde_json::to_vec_pretty(&request).map_err(|error| error.to_string())?,
                )
                .map_err(|error| error.to_string())?;
                fs::write(output.join("provider-response.json"), body)
                    .map_err(|error| error.to_string())?;
                if !accepted {
                    diagnostics.push(diagnostic(
                        "error",
                        "$.provider",
                        "provider returned non-2xx status",
                    ));
                }
            }
            Err(error) => {
                blocked = true;
                provider = json!({"configured": true, "url": url, "status_code": 0});
                diagnostics.push(diagnostic("error", "$.provider", error));
            }
        }
    } else {
        blocked = true;
        diagnostics.push(diagnostic(
            "error",
            "$.provider",
            "ASTER_MEMORY_PROVIDER_URL or --provider-url is required; no fake provider is used",
        ));
    }
    let passed = !blocked && score >= 0.75;
    let mut report = json!({
        "schema_version": 1,
        "kind": "aster_memory_benchmark_run",
        "status": if blocked { "blocked" } else if passed { "passed" } else { "failed" },
        "blocked": blocked,
        "passed": passed,
        "project": normalize_path(&project),
        "suite": suite,
        "store": normalize_path(&store),
        "provider": provider,
        "case_count": 1,
        "ablation_count": 1,
        "regression_replay_count": 1,
        "score": score,
        "diagnostics": diagnostics,
        "rules": [
            "Provider evaluation uses a real Generic JSON HTTP endpoint.",
            "If the provider is unavailable, the run is blocked instead of faked.",
            "Provider request and response artifacts are persisted for deterministic replay."
        ]
    });
    if include_schema {
        report["output_schema"] = memory_bench_output_schema();
    }
    let text = serde_json::to_string_pretty(&report).map_err(|error| error.to_string())?;
    fs::write(&report_path, &text).map_err(|error| error.to_string())?;
    println!("{text}");
    if passed {
        Ok(())
    } else {
        Err(format!(
            "memory benchmark run did not pass; inspect {}",
            report_path.display()
        ))
    }
}

fn memory_bench_compare_command(args: &[String]) -> Result<(), String> {
    let before = value_after(args, "--before")
        .map(PathBuf::from)
        .ok_or_else(|| "memory-bench-compare requires --before <report.json>".to_string())?;
    let after = value_after(args, "--after")
        .map(PathBuf::from)
        .ok_or_else(|| "memory-bench-compare requires --after <report.json>".to_string())?;
    let include_schema = args.iter().any(|arg| arg == "--output-schema");
    let before_value: Value =
        serde_json::from_slice(&fs::read(&before).map_err(|error| error.to_string())?)
            .map_err(|error| error.to_string())?;
    let after_value: Value =
        serde_json::from_slice(&fs::read(&after).map_err(|error| error.to_string())?)
            .map_err(|error| error.to_string())?;
    let before_score = before_value
        .get("score")
        .and_then(Value::as_f64)
        .unwrap_or(0.0);
    let after_score = after_value
        .get("score")
        .and_then(Value::as_f64)
        .unwrap_or(0.0);
    let delta = after_score - before_score;
    let mut report = json!({
        "schema_version": 1,
        "kind": "aster_memory_benchmark_compare",
        "status": if delta >= 0.0 { "passed" } else { "regressed" },
        "before": normalize_path(&before),
        "after": normalize_path(&after),
        "before_score": before_score,
        "after_score": after_score,
        "delta": delta
    });
    if include_schema {
        report["output_schema"] = memory_compare_output_schema();
    }
    println!(
        "{}",
        serde_json::to_string_pretty(&report).map_err(|error| error.to_string())?
    );
    if delta >= 0.0 {
        Ok(())
    } else {
        Err("memory benchmark comparison regressed".to_string())
    }
}

fn mesh_format_for_path(path: &Path) -> String {
    path.extension()
        .and_then(|extension| extension.to_str())
        .unwrap_or_default()
        .to_ascii_lowercase()
}

fn inspect_obj_text(text: &str) -> (usize, usize) {
    let mut vertices = 0usize;
    let mut indices = 0usize;
    for line in text.lines() {
        let trimmed = line.trim_start();
        if trimmed.starts_with("v ") {
            vertices += 1;
        } else if trimmed.starts_with("f ") {
            let corners = trimmed.split_whitespace().skip(1).count();
            if corners >= 3 {
                indices += (corners - 2) * 3;
            }
        }
    }
    (vertices, indices)
}

fn inspect_ply_text(text: &str) -> (usize, usize) {
    let mut vertices = 0usize;
    let mut faces = 0usize;
    for line in text.lines() {
        let mut parts = line.split_whitespace();
        if parts.next() == Some("element") {
            match parts.next() {
                Some("vertex") => vertices = parts.next().and_then(|v| v.parse().ok()).unwrap_or(0),
                Some("face") => faces = parts.next().and_then(|v| v.parse().ok()).unwrap_or(0),
                _ => {}
            }
        }
        if line.trim() == "end_header" {
            break;
        }
    }
    (vertices, faces * 3)
}

fn inspect_stl_text(text: &str) -> (usize, usize) {
    let vertices = text
        .lines()
        .filter(|line| line.trim_start().starts_with("vertex "))
        .count();
    (vertices, vertices)
}

fn hash_bytes(bytes: &[u8]) -> u64 {
    let mut hash = 1469598103934665603u64;
    for byte in bytes {
        hash ^= u64::from(*byte);
        hash = hash.wrapping_mul(1099511628211u64);
    }
    hash
}

fn hex_u64(value: u64) -> String {
    format!("0x{value:016x}")
}

fn graph_command(args: &[String]) -> Result<(), String> {
    let db = value_after(args, "--db")
        .map(PathBuf::from)
        .ok_or_else(|| "graph requires --db <assetdb.asterdb.json>".to_string())?;
    let database = read_asset_database(&db).map_err(|error| error.to_string())?;
    let report = asset_graph_report_json(&database).map_err(|error| error.to_string())?;
    println!("{report}");
    Ok(())
}

fn fate_command(args: &[String]) -> Result<(), String> {
    let db = value_after(args, "--db")
        .map(PathBuf::from)
        .ok_or_else(|| "fate requires --db <assetdb.asterdb.json>".to_string())?;
    let asset = value_after(args, "--asset")
        .ok_or_else(|| "fate requires --asset <id-or-guid>".to_string())?;
    let database = read_asset_database(&db).map_err(|error| error.to_string())?;
    let report = asset_fate_report_json(&database, &asset).map_err(|error| error.to_string())?;
    println!("{report}");
    Ok(())
}

fn diff_command(args: &[String]) -> Result<(), String> {
    let before = value_after(args, "--before")
        .map(PathBuf::from)
        .ok_or_else(|| "diff requires --before <old.assetdb.asterdb.json>".to_string())?;
    let after = value_after(args, "--after")
        .map(PathBuf::from)
        .ok_or_else(|| "diff requires --after <new.assetdb.asterdb.json>".to_string())?;
    let before_database = read_asset_database(&before).map_err(|error| error.to_string())?;
    let after_database = read_asset_database(&after).map_err(|error| error.to_string())?;
    let report = asset_database_diff_json(&before_database, &after_database)
        .map_err(|error| error.to_string())?;
    println!("{report}");
    Ok(())
}

fn lineage_diff_command(args: &[String]) -> Result<(), String> {
    let before = value_after(args, "--before")
        .map(PathBuf::from)
        .ok_or_else(|| "lineage-diff requires --before <old.assetdb.asterdb.json>".to_string())?;
    let after = value_after(args, "--after")
        .map(PathBuf::from)
        .ok_or_else(|| "lineage-diff requires --after <new.assetdb.asterdb.json>".to_string())?;
    let before_database = read_asset_database(&before).map_err(|error| error.to_string())?;
    let after_database = read_asset_database(&after).map_err(|error| error.to_string())?;
    let report = cook_lineage_diff_json(&before_database, &after_database)
        .map_err(|error| error.to_string())?;
    println!("{report}");
    Ok(())
}

fn lineage_report_command(args: &[String]) -> Result<(), String> {
    let db = value_after(args, "--db")
        .map(PathBuf::from)
        .ok_or_else(|| "lineage-report requires --db <assetdb.asterdb.json>".to_string())?;
    let database = read_asset_database(&db).map_err(|error| error.to_string())?;
    let report = cook_lineage_report_json(&database).map_err(|error| error.to_string())?;
    println!("{report}");
    Ok(())
}

fn guid_init_command(args: &[String]) -> Result<(), String> {
    let project = value_after(args, "--project")
        .map(PathBuf::from)
        .ok_or_else(|| "guid-init requires --project <file.asterproj>".to_string())?;
    let written = write_missing_asset_meta(&project).map_err(|error| error.to_string())?;
    println!(
        "guid-init {} wrote {} .astermeta file(s)",
        project.display(),
        written
    );
    Ok(())
}

fn run() -> Result<(), String> {
    let args: Vec<String> = std::env::args().collect();
    if args.iter().any(|arg| arg == "--self-check") {
        self_check();
        return Ok(());
    }
    match args.get(1).map(String::as_str) {
        Some("compile") => compile_command(&args[2..]),
        Some("inspect") => inspect_command(&args[2..]),
        Some("texture-inspect") => texture_inspect_command(&args[2..]),
        Some("texture-bake") => texture_bake_command(&args[2..]),
        Some("material-inspect") => material_inspect_command(&args[2..]),
        Some("graph-inspect") => graph_inspect_command(&args[2..]),
        Some("graph-package") => graph_package_command(&args[2..]),
        Some("cook") => cook_command(&args[2..]),
        Some("report") => report_command(&args[2..]),
        Some("catalog-inspect") => catalog_inspect_command(&args[2..]),
        Some("catalog-audit") => catalog_audit_command(&args[2..]),
        Some("catalog-sync") => catalog_sync_command(&args[2..]),
        Some("mesh-import-inspect") => mesh_import_inspect_command(&args[2..]),
        Some("mesh-recipe-inspect") => mesh_recipe_inspect_command(&args[2..]),
        Some("session-audit") => session_audit_command(&args[2..]),
        Some("agent-plan") => agent_plan_command(&args[2..]),
        Some("agent-audit") => agent_audit_command(&args[2..]),
        Some("agent-fix-headers") => agent_fix_headers_command(&args[2..]),
        Some("agent-native-audit") => agent_native_audit_command(&args[2..]),
        Some("agent-runtime-audit") => agent_runtime_audit_command(&args[2..]),
        Some("agent-review") => agent_review_command(&args[2..]),
        Some("asset-brief") => asset_brief_command(&args[2..]),
        Some("asset-proof-run") => asset_proof_run_command(&args[2..]),
        Some("lesson-inspect") => lesson_inspect_command(&args[2..]),
        Some("learning-proof-run") => learning_proof_run_command(&args[2..]),
        Some("memory-proof-run") => memory_proof_run_command(&args[2..]),
        Some("memory-bench-run") => memory_bench_run_command(&args[2..]),
        Some("memory-bench-compare") => memory_bench_compare_command(&args[2..]),
        Some("graph") => graph_command(&args[2..]),
        Some("fate") => fate_command(&args[2..]),
        Some("diff") => diff_command(&args[2..]),
        Some("lineage-diff") => lineage_diff_command(&args[2..]),
        Some("lineage-report") => lineage_report_command(&args[2..]),
        Some("guid-init") => guid_init_command(&args[2..]),
        Some("--help") | Some("-h") | None => {
            println!("{}", usage());
            Ok(())
        }
        Some(command) => Err(format!("unknown command '{command}'\n{}", usage())),
    }
}

fn main() {
    if let Err(error) = run() {
        eprintln!("aster_assetc: {error}");
        std::process::exit(1);
    }
}
