// Author: Faruk Alpay
// Do not remove this notice.

use std::cmp::Ordering;
use std::slice;
use std::time::Instant;

const ASTER_RENDER_FLAG_FADE_ELIGIBLE: u32 = 1 << 0;
const ASTER_RENDER_QUEUE_TRANSLUCENT: u32 = 2;
const ASTER_RENDER_PASS_OPAQUE: u32 = 0;
const ASTER_RENDER_PASS_TRANSPARENT: u32 = 1;

#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct AsterRuntimeVec3 {
    pub x: f32,
    pub y: f32,
    pub z: f32,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct AsterRuntimeRenderObject {
    pub entity_id: u64,
    pub object_index: u32,
    pub mesh_key: u64,
    pub material_key: u64,
    pub render_queue: u32,
    pub flags: u32,
    pub position: AsterRuntimeVec3,
    pub bounds_center: AsterRuntimeVec3,
    pub bounds_radius: f32,
    pub opacity: f32,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct AsterRuntimeCamera {
    pub position: AsterRuntimeVec3,
    pub forward: AsterRuntimeVec3,
    pub right: AsterRuntimeVec3,
    pub up: AsterRuntimeVec3,
    pub vertical_fov: f32,
    pub aspect_ratio: f32,
    pub near_plane: f32,
    pub far_plane: f32,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct AsterRuntimeLineOfSightFade {
    pub enabled: u32,
    pub camera_position: AsterRuntimeVec3,
    pub target_position: AsterRuntimeVec3,
    pub radius: f32,
    pub min_opacity: f32,
    pub camera_clearance: f32,
    pub target_clearance: f32,
    pub max_object_radius: f32,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct AsterRuntimeRenderPlanOptions {
    pub line_of_sight_fade: AsterRuntimeLineOfSightFade,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct AsterRuntimeDrawInstance {
    pub object_index: u32,
    pub opacity: f32,
    pub sort_distance_sq: f32,
    pub flags: u32,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct AsterRuntimeDrawGroup {
    pub mesh_key: u64,
    pub material_key: u64,
    pub render_queue: u32,
    pub pass: u32,
    pub first_instance: usize,
    pub instance_count: usize,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct AsterRuntimeRenderDiagnostics {
    pub object_count: usize,
    pub visible_objects: usize,
    pub culled_objects: usize,
    pub opaque_groups: usize,
    pub transparent_groups: usize,
    pub instance_groups: usize,
    pub planned_instances: usize,
    pub rust_plan_seconds: f64,
}

#[repr(C)]
#[derive(Debug)]
pub struct AsterRuntimeFramePlan {
    pub instances: *mut AsterRuntimeDrawInstance,
    pub instance_count: usize,
    pub groups: *mut AsterRuntimeDrawGroup,
    pub group_count: usize,
    pub diagnostics: AsterRuntimeRenderDiagnostics,
}

impl Default for AsterRuntimeFramePlan {
    fn default() -> Self {
        Self {
            instances: std::ptr::null_mut(),
            instance_count: 0,
            groups: std::ptr::null_mut(),
            group_count: 0,
            diagnostics: AsterRuntimeRenderDiagnostics::default(),
        }
    }
}

#[derive(Clone, Copy)]
struct PlannedObject {
    object: AsterRuntimeRenderObject,
    opacity: f32,
    sort_distance_sq: f32,
    transparent: bool,
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct DrawKey {
    pub mesh_key: u64,
    pub material_key: u64,
    pub render_queue: u32,
    pub pass: u32,
}

#[derive(Clone, Debug, Default)]
pub struct PlannerSummary {
    pub object_count: usize,
    pub visible_objects: usize,
    pub culled_objects: usize,
    pub opaque_groups: usize,
    pub transparent_groups: usize,
    pub instance_groups: usize,
    pub planned_instances: usize,
}

#[derive(Clone, Debug, Default)]
pub struct PlannerOutput {
    pub instances: Vec<AsterRuntimeDrawInstance>,
    pub groups: Vec<AsterRuntimeDrawGroup>,
    pub summary: PlannerSummary,
}

fn dot(lhs: AsterRuntimeVec3, rhs: AsterRuntimeVec3) -> f32 {
    lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z
}

fn sub(lhs: AsterRuntimeVec3, rhs: AsterRuntimeVec3) -> AsterRuntimeVec3 {
    AsterRuntimeVec3 {
        x: lhs.x - rhs.x,
        y: lhs.y - rhs.y,
        z: lhs.z - rhs.z,
    }
}

fn length_sq(value: AsterRuntimeVec3) -> f32 {
    dot(value, value)
}

fn distance_point_segment_sq(
    point: AsterRuntimeVec3,
    from: AsterRuntimeVec3,
    to: AsterRuntimeVec3,
) -> (f32, f32, f32) {
    let segment = sub(to, from);
    let denom = length_sq(segment);
    if denom <= f32::EPSILON {
        return (length_sq(sub(point, from)), 0.0, 0.0);
    }
    let segment_length = denom.sqrt();
    let t = (dot(sub(point, from), segment) / denom).clamp(0.0, 1.0);
    let closest = AsterRuntimeVec3 {
        x: from.x + segment.x * t,
        y: from.y + segment.y * t,
        z: from.z + segment.z * t,
    };
    (
        length_sq(sub(point, closest)),
        segment_length * t,
        segment_length,
    )
}

fn visible_to_camera(object: AsterRuntimeRenderObject, camera: AsterRuntimeCamera) -> bool {
    let radius = object.bounds_radius.max(0.0);
    let camera_to_object = sub(object.bounds_center, camera.position);
    let distance_sq = length_sq(camera_to_object);
    let far = camera.far_plane.max(camera.near_plane).max(0.0);
    if distance_sq > (far + radius) * (far + radius) {
        return false;
    }

    let forward_distance = dot(camera_to_object, camera.forward);
    if forward_distance < -radius || forward_distance > far + radius {
        return false;
    }

    let plane_distance = forward_distance.max(camera.near_plane.max(0.001));
    let vertical_limit = (camera.vertical_fov * 0.5).tan() * plane_distance + radius;
    let horizontal_limit = vertical_limit * camera.aspect_ratio.max(0.001) + radius;
    dot(camera_to_object, camera.right).abs() <= horizontal_limit
        && dot(camera_to_object, camera.up).abs() <= vertical_limit
}

fn fade_opacity(
    object: AsterRuntimeRenderObject,
    options: AsterRuntimeRenderPlanOptions,
) -> Option<f32> {
    let fade = options.line_of_sight_fade;
    if fade.enabled == 0 || object.flags & ASTER_RENDER_FLAG_FADE_ELIGIBLE == 0 {
        return None;
    }
    let radius = object.bounds_radius.max(0.0);
    if fade.max_object_radius > 0.0 && radius > fade.max_object_radius {
        return None;
    }
    let limit = radius + fade.radius.max(0.0);
    let (distance_sq, along, segment_length) = distance_point_segment_sq(
        object.bounds_center,
        fade.camera_position,
        fade.target_position,
    );
    if segment_length <= 0.001
        || along <= fade.camera_clearance.max(0.0)
        || along >= segment_length - fade.target_clearance.max(0.0)
    {
        return None;
    }
    if distance_sq > limit * limit {
        return None;
    }
    Some(object.opacity.min(fade.min_opacity.clamp(0.0, 1.0)))
}

fn object_key(object: AsterRuntimeRenderObject, pass: u32) -> DrawKey {
    DrawKey {
        mesh_key: object.mesh_key,
        material_key: object.material_key,
        render_queue: object.render_queue,
        pass,
    }
}

fn append_groups(
    planned: &[PlannedObject],
    pass: u32,
    instances: &mut Vec<AsterRuntimeDrawInstance>,
    groups: &mut Vec<AsterRuntimeDrawGroup>,
) -> usize {
    let first_group = groups.len();
    let mut cursor = 0;
    while cursor < planned.len() {
        let key = object_key(planned[cursor].object, pass);
        let first_instance = instances.len();
        let mut count = 0;
        while cursor + count < planned.len()
            && object_key(planned[cursor + count].object, pass) == key
        {
            let item = planned[cursor + count];
            instances.push(AsterRuntimeDrawInstance {
                object_index: item.object.object_index,
                opacity: item.opacity,
                sort_distance_sq: item.sort_distance_sq,
                flags: if item.opacity < item.object.opacity {
                    ASTER_RENDER_FLAG_FADE_ELIGIBLE
                } else {
                    0
                },
            });
            count += 1;
        }
        groups.push(AsterRuntimeDrawGroup {
            mesh_key: key.mesh_key,
            material_key: key.material_key,
            render_queue: key.render_queue,
            pass: key.pass,
            first_instance,
            instance_count: count,
        });
        cursor += count;
    }
    groups.len() - first_group
}

pub fn build_frame_plan(
    objects: &[AsterRuntimeRenderObject],
    camera: AsterRuntimeCamera,
    options: AsterRuntimeRenderPlanOptions,
) -> PlannerOutput {
    let mut opaque = Vec::new();
    let mut transparent = Vec::new();

    for &object in objects {
        if !visible_to_camera(object, camera) {
            continue;
        }
        let camera_delta = sub(object.bounds_center, camera.position);
        let sort_distance_sq = length_sq(camera_delta);
        let fade_opacity = fade_opacity(object, options);
        let transparent_object =
            object.render_queue == ASTER_RENDER_QUEUE_TRANSLUCENT || fade_opacity.is_some();
        let planned = PlannedObject {
            object,
            opacity: fade_opacity.unwrap_or(object.opacity),
            sort_distance_sq,
            transparent: transparent_object,
        };
        if planned.transparent {
            transparent.push(planned);
        } else {
            opaque.push(planned);
        }
    }

    opaque.sort_by(|lhs, rhs| {
        object_key(lhs.object, ASTER_RENDER_PASS_OPAQUE)
            .mesh_key
            .cmp(&object_key(rhs.object, ASTER_RENDER_PASS_OPAQUE).mesh_key)
            .then_with(|| lhs.object.material_key.cmp(&rhs.object.material_key))
            .then_with(|| lhs.object.render_queue.cmp(&rhs.object.render_queue))
            .then_with(|| lhs.object.object_index.cmp(&rhs.object.object_index))
    });
    transparent.sort_by(|lhs, rhs| {
        rhs.sort_distance_sq
            .partial_cmp(&lhs.sort_distance_sq)
            .unwrap_or(Ordering::Equal)
            .then_with(|| lhs.object.object_index.cmp(&rhs.object.object_index))
    });

    let visible_objects = opaque.len() + transparent.len();
    let mut instances = Vec::with_capacity(visible_objects);
    let mut groups = Vec::new();
    let opaque_groups = append_groups(
        &opaque,
        ASTER_RENDER_PASS_OPAQUE,
        &mut instances,
        &mut groups,
    );
    let transparent_groups = append_groups(
        &transparent,
        ASTER_RENDER_PASS_TRANSPARENT,
        &mut instances,
        &mut groups,
    );

    PlannerOutput {
        summary: PlannerSummary {
            object_count: objects.len(),
            visible_objects,
            culled_objects: objects.len().saturating_sub(visible_objects),
            opaque_groups,
            transparent_groups,
            instance_groups: groups.len(),
            planned_instances: instances.len(),
        },
        instances,
        groups,
    }
}

#[no_mangle]
pub extern "C" fn aster_runtime_build_frame_plan(
    objects: *const AsterRuntimeRenderObject,
    object_count: usize,
    camera: AsterRuntimeCamera,
    options: AsterRuntimeRenderPlanOptions,
    out_plan: *mut AsterRuntimeFramePlan,
) -> u32 {
    let started = Instant::now();
    if out_plan.is_null() {
        return 0;
    }
    if objects.is_null() && object_count > 0 {
        unsafe {
            *out_plan = AsterRuntimeFramePlan::default();
        }
        return 0;
    }
    let object_slice = if object_count == 0 {
        &[]
    } else {
        unsafe { slice::from_raw_parts(objects, object_count) }
    };
    let output = build_frame_plan(object_slice, camera, options);

    let mut instances = output.instances.into_boxed_slice();
    let mut groups = output.groups.into_boxed_slice();
    let instance_count = instances.len();
    let group_count = groups.len();
    let instance_ptr = instances.as_mut_ptr();
    let group_ptr = groups.as_mut_ptr();
    std::mem::forget(instances);
    std::mem::forget(groups);

    unsafe {
        *out_plan = AsterRuntimeFramePlan {
            instances: instance_ptr,
            instance_count,
            groups: group_ptr,
            group_count,
            diagnostics: AsterRuntimeRenderDiagnostics {
                object_count: output.summary.object_count,
                visible_objects: output.summary.visible_objects,
                culled_objects: output.summary.culled_objects,
                opaque_groups: output.summary.opaque_groups,
                transparent_groups: output.summary.transparent_groups,
                instance_groups: output.summary.instance_groups,
                planned_instances: output.summary.planned_instances,
                rust_plan_seconds: started.elapsed().as_secs_f64(),
            },
        };
    }
    1
}

#[no_mangle]
pub extern "C" fn aster_runtime_free_frame_plan(plan: AsterRuntimeFramePlan) {
    if !plan.instances.is_null() && plan.instance_count > 0 {
        unsafe {
            drop(Vec::from_raw_parts(
                plan.instances,
                plan.instance_count,
                plan.instance_count,
            ));
        }
    }
    if !plan.groups.is_null() && plan.group_count > 0 {
        unsafe {
            drop(Vec::from_raw_parts(
                plan.groups,
                plan.group_count,
                plan.group_count,
            ));
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn object(
        index: u32,
        x: f32,
        mesh: u64,
        material: u64,
        queue: u32,
    ) -> AsterRuntimeRenderObject {
        AsterRuntimeRenderObject {
            entity_id: index as u64 + 1,
            object_index: index,
            mesh_key: mesh,
            material_key: material,
            render_queue: queue,
            flags: ASTER_RENDER_FLAG_FADE_ELIGIBLE,
            position: AsterRuntimeVec3 { x, y: 0.0, z: 6.0 },
            bounds_center: AsterRuntimeVec3 { x, y: 0.0, z: 6.0 },
            bounds_radius: 0.5,
            opacity: 1.0,
        }
    }

    fn camera() -> AsterRuntimeCamera {
        AsterRuntimeCamera {
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
        }
    }

    #[test]
    fn groups_opaque_objects_by_draw_key() {
        let objects = [
            object(0, 0.0, 2, 7, 0),
            object(1, 0.3, 2, 7, 0),
            object(2, 0.6, 3, 7, 0),
        ];
        let output = build_frame_plan(&objects, camera(), AsterRuntimeRenderPlanOptions::default());
        assert_eq!(output.summary.visible_objects, 3);
        assert_eq!(output.summary.opaque_groups, 2);
        assert_eq!(output.groups[0].instance_count, 2);
        assert_eq!(output.groups[1].instance_count, 1);
    }

    #[test]
    fn culls_objects_outside_frustum() {
        let objects = [object(0, 0.0, 2, 7, 0), object(1, 100.0, 2, 7, 0)];
        let output = build_frame_plan(&objects, camera(), AsterRuntimeRenderPlanOptions::default());
        assert_eq!(output.summary.visible_objects, 1);
        assert_eq!(output.summary.culled_objects, 1);
    }

    #[test]
    fn transparent_objects_sort_back_to_front() {
        let near = object(0, 0.0, 2, 7, ASTER_RENDER_QUEUE_TRANSLUCENT);
        let mut far = object(1, 0.0, 2, 7, ASTER_RENDER_QUEUE_TRANSLUCENT);
        far.position.z = 10.0;
        far.bounds_center.z = 10.0;
        let output = build_frame_plan(
            &[near, far],
            camera(),
            AsterRuntimeRenderPlanOptions::default(),
        );
        assert_eq!(output.instances[0].object_index, 1);
        assert_eq!(output.instances[1].object_index, 0);
    }
}
