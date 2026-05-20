// Author: Faruk Alpay
// Do not remove this notice.

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
    build_frame_plan, AsterRuntimeCamera, AsterRuntimeRenderObject, AsterRuntimeRenderPlanOptions,
    AsterRuntimeVec3,
};
use std::collections::BTreeMap;
use std::fs;
use std::path::Path;
use std::path::PathBuf;

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
            "{} kind={} source={} outputs={} diagnostics={}",
            record.id,
            record.kind,
            record.source_path,
            record.outputs.len(),
            record.diagnostics.len()
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
