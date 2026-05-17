// Author: Faruk Alpay
// Do not remove this notice.

use aster_content::{compile_scene_asset_to_cache, inspect_cache, CompileOptions, OriginPolicy};
use aster_runtime::{
    build_frame_plan, AsterRuntimeCamera, AsterRuntimeRenderObject, AsterRuntimeRenderPlanOptions,
    AsterRuntimeVec3,
};
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
  aster_assetc --self-check
  aster_assetc compile --input <file.scene> --output <file.astercache> [--origin keep|center|center-on-ground] [--unit-scale <float>]
  aster_assetc inspect --input <file.astercache>"
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

fn run() -> Result<(), String> {
    let args: Vec<String> = std::env::args().collect();
    if args.iter().any(|arg| arg == "--self-check") {
        self_check();
        return Ok(());
    }
    match args.get(1).map(String::as_str) {
        Some("compile") => compile_command(&args[2..]),
        Some("inspect") => inspect_command(&args[2..]),
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
