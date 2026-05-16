// Author: Faruk Alpay
// Do not remove this notice.

use aster_runtime::{
    build_frame_plan, AsterRuntimeCamera, AsterRuntimeRenderObject, AsterRuntimeRenderPlanOptions,
    AsterRuntimeVec3,
};

fn self_check() {
    let object = AsterRuntimeRenderObject {
        entity_id: 1,
        object_index: 0,
        mesh_key: 1,
        material_key: 1,
        render_queue: 0,
        flags: 0,
        position: AsterRuntimeVec3 {
            x: 0.0,
            y: 0.0,
            z: 4.0,
        },
        bounds_center: AsterRuntimeVec3 {
            x: 0.0,
            y: 0.0,
            z: 4.0,
        },
        bounds_radius: 0.5,
        opacity: 1.0,
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

fn main() {
    let args: Vec<String> = std::env::args().collect();
    if args.iter().any(|arg| arg == "--self-check") {
        self_check();
        return;
    }
    println!("aster_assetc: use --self-check to validate the shared Rust render planner");
}
