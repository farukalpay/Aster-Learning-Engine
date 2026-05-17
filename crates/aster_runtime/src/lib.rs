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
    pub visibility_class: u32,
    pub position: AsterRuntimeVec3,
    pub visibility_cell: AsterRuntimeVec3,
    pub bounds_center: AsterRuntimeVec3,
    pub bounds_radius: f32,
    pub opacity: f32,
    pub lod_max_distance: f32,
    pub lod_min_projected_radius: f32,
    pub portal_depth: f32,
    pub dynamic_mesh_generation: u64,
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
    pub lod_culled_objects: usize,
    pub visibility_hint_objects: usize,
    pub dynamic_mesh_objects: usize,
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

#[repr(C)]
pub struct AsterRuntimeFramePlanBuffers {
    pub instances: *mut AsterRuntimeDrawInstance,
    pub instance_capacity: usize,
    pub instance_count: usize,
    pub groups: *mut AsterRuntimeDrawGroup,
    pub group_capacity: usize,
    pub group_count: usize,
    pub diagnostics: *mut AsterRuntimeRenderDiagnostics,
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
    pub lod_culled_objects: usize,
    pub visibility_hint_objects: usize,
    pub dynamic_mesh_objects: usize,
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

fn add(lhs: AsterRuntimeVec3, rhs: AsterRuntimeVec3) -> AsterRuntimeVec3 {
    AsterRuntimeVec3 {
        x: lhs.x + rhs.x,
        y: lhs.y + rhs.y,
        z: lhs.z + rhs.z,
    }
}

fn scale(value: AsterRuntimeVec3, scalar: f32) -> AsterRuntimeVec3 {
    AsterRuntimeVec3 {
        x: value.x * scalar,
        y: value.y * scalar,
        z: value.z * scalar,
    }
}

fn cross(lhs: AsterRuntimeVec3, rhs: AsterRuntimeVec3) -> AsterRuntimeVec3 {
    AsterRuntimeVec3 {
        x: lhs.y * rhs.z - lhs.z * rhs.y,
        y: lhs.z * rhs.x - lhs.x * rhs.z,
        z: lhs.x * rhs.y - lhs.y * rhs.x,
    }
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct AsterRuntimeMeshCutMeasure {
    pub centroid: AsterRuntimeVec3,
    pub bounds_min: AsterRuntimeVec3,
    pub bounds_max: AsterRuntimeVec3,
    pub surface_area: f32,
    pub signed_volume: f32,
}

pub fn measure_mesh_cut(
    vertices: &[AsterRuntimeVec3],
    indices: &[u32],
) -> Option<AsterRuntimeMeshCutMeasure> {
    if vertices.is_empty() || indices.len() < 3 || indices.len() % 3 != 0 {
        return None;
    }

    let mut bounds_min = AsterRuntimeVec3 {
        x: f32::INFINITY,
        y: f32::INFINITY,
        z: f32::INFINITY,
    };
    let mut bounds_max = AsterRuntimeVec3 {
        x: f32::NEG_INFINITY,
        y: f32::NEG_INFINITY,
        z: f32::NEG_INFINITY,
    };
    for &vertex in vertices {
        bounds_min.x = bounds_min.x.min(vertex.x);
        bounds_min.y = bounds_min.y.min(vertex.y);
        bounds_min.z = bounds_min.z.min(vertex.z);
        bounds_max.x = bounds_max.x.max(vertex.x);
        bounds_max.y = bounds_max.y.max(vertex.y);
        bounds_max.z = bounds_max.z.max(vertex.z);
    }

    let mut area_centroid = AsterRuntimeVec3::default();
    let mut surface_area = 0.0f32;
    let mut signed_volume = 0.0f32;
    for triangle in indices.chunks_exact(3) {
        let ia = triangle[0] as usize;
        let ib = triangle[1] as usize;
        let ic = triangle[2] as usize;
        if ia >= vertices.len() || ib >= vertices.len() || ic >= vertices.len() {
            return None;
        }
        let a = vertices[ia];
        let b = vertices[ib];
        let c = vertices[ic];
        let area = length_sq(cross(sub(b, a), sub(c, a))).sqrt() * 0.5;
        if area > f32::EPSILON {
            area_centroid = add(area_centroid, scale(add(add(a, b), c), area / 3.0));
            surface_area += area;
        }
        signed_volume += dot(a, cross(b, c)) / 6.0;
    }

    if surface_area <= f32::EPSILON {
        return None;
    }

    Some(AsterRuntimeMeshCutMeasure {
        centroid: scale(area_centroid, 1.0 / surface_area),
        bounds_min,
        bounds_max,
        surface_area,
        signed_volume,
    })
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

fn passes_lod_policy(object: AsterRuntimeRenderObject, camera: AsterRuntimeCamera) -> bool {
    let radius = object.bounds_radius.max(0.0);
    let camera_to_object = sub(object.bounds_center, camera.position);
    let distance_sq = length_sq(camera_to_object);
    if object.lod_max_distance > 0.0 {
        let distance = distance_sq.sqrt();
        if distance - radius > object.lod_max_distance {
            return false;
        }
    }
    if object.lod_min_projected_radius > 0.0 {
        let forward_distance =
            dot(camera_to_object, camera.forward).max(camera.near_plane.max(0.001));
        let focal = 1.0 / (camera.vertical_fov * 0.5).tan().max(0.001);
        let projected_radius = radius * focal / forward_distance;
        if projected_radius < object.lod_min_projected_radius {
            return false;
        }
    }
    true
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
    let mut lod_culled_objects = 0usize;
    let mut visibility_hint_objects = 0usize;
    let mut dynamic_mesh_objects = 0usize;

    for &object in objects {
        if object.visibility_class != 0 {
            visibility_hint_objects += 1;
        }
        if object.dynamic_mesh_generation != 0 {
            dynamic_mesh_objects += 1;
        }
        if !visible_to_camera(object, camera) {
            continue;
        }
        if !passes_lod_policy(object, camera) {
            lod_culled_objects += 1;
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
            lod_culled_objects,
            visibility_hint_objects,
            dynamic_mesh_objects,
        },
        instances,
        groups,
    }
}

fn write_frame_plan(
    objects: *const AsterRuntimeRenderObject,
    object_count: usize,
    camera: &AsterRuntimeCamera,
    options: &AsterRuntimeRenderPlanOptions,
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
    let output = build_frame_plan(object_slice, *camera, *options);

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
                lod_culled_objects: output.summary.lod_culled_objects,
                visibility_hint_objects: output.summary.visibility_hint_objects,
                dynamic_mesh_objects: output.summary.dynamic_mesh_objects,
                rust_plan_seconds: started.elapsed().as_secs_f64(),
            },
        };
    }
    1
}

fn diagnostics_from_output(
    output: &PlannerOutput,
    started: Instant,
) -> AsterRuntimeRenderDiagnostics {
    AsterRuntimeRenderDiagnostics {
        object_count: output.summary.object_count,
        visible_objects: output.summary.visible_objects,
        culled_objects: output.summary.culled_objects,
        opaque_groups: output.summary.opaque_groups,
        transparent_groups: output.summary.transparent_groups,
        instance_groups: output.summary.instance_groups,
        planned_instances: output.summary.planned_instances,
        lod_culled_objects: output.summary.lod_culled_objects,
        visibility_hint_objects: output.summary.visibility_hint_objects,
        dynamic_mesh_objects: output.summary.dynamic_mesh_objects,
        rust_plan_seconds: started.elapsed().as_secs_f64(),
    }
}

fn object_slice_from_abi<'a>(
    objects: *const AsterRuntimeRenderObject,
    object_count: usize,
) -> Option<&'a [AsterRuntimeRenderObject]> {
    if objects.is_null() && object_count > 0 {
        None
    } else if object_count == 0 {
        Some(&[])
    } else {
        Some(unsafe { slice::from_raw_parts(objects, object_count) })
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
    write_frame_plan(objects, object_count, &camera, &options, out_plan)
}

#[no_mangle]
pub extern "C" fn aster_runtime_build_frame_plan_v2(
    objects: *const AsterRuntimeRenderObject,
    object_count: usize,
    camera: *const AsterRuntimeCamera,
    options: *const AsterRuntimeRenderPlanOptions,
    out_plan: *mut AsterRuntimeFramePlan,
) -> u32 {
    if camera.is_null() || options.is_null() {
        if !out_plan.is_null() {
            unsafe {
                *out_plan = AsterRuntimeFramePlan::default();
            }
        }
        return 0;
    }
    write_frame_plan(
        objects,
        object_count,
        unsafe { &*camera },
        unsafe { &*options },
        out_plan,
    )
}

#[no_mangle]
pub extern "C" fn aster_runtime_build_frame_plan_v3(
    objects: *const AsterRuntimeRenderObject,
    object_count: usize,
    camera: *const AsterRuntimeCamera,
    options: *const AsterRuntimeRenderPlanOptions,
    buffers: *mut AsterRuntimeFramePlanBuffers,
) -> u32 {
    let started = Instant::now();
    if camera.is_null() || options.is_null() || buffers.is_null() {
        return 0;
    }
    let Some(object_slice) = object_slice_from_abi(objects, object_count) else {
        return 0;
    };

    let output = build_frame_plan(object_slice, unsafe { *camera }, unsafe { *options });
    let buffers = unsafe { &mut *buffers };
    buffers.instance_count = output.instances.len();
    buffers.group_count = output.groups.len();

    if !buffers.diagnostics.is_null() {
        unsafe {
            *buffers.diagnostics = diagnostics_from_output(&output, started);
        }
    }

    if output.instances.len() > buffers.instance_capacity
        || output.groups.len() > buffers.group_capacity
        || (output.instances.len() > 0 && buffers.instances.is_null())
        || (output.groups.len() > 0 && buffers.groups.is_null())
    {
        return 0;
    }

    if !output.instances.is_empty() {
        unsafe {
            std::ptr::copy_nonoverlapping(
                output.instances.as_ptr(),
                buffers.instances,
                output.instances.len(),
            );
        }
    }
    if !output.groups.is_empty() {
        unsafe {
            std::ptr::copy_nonoverlapping(
                output.groups.as_ptr(),
                buffers.groups,
                output.groups.len(),
            );
        }
    }
    1
}

#[no_mangle]
pub extern "C" fn aster_runtime_free_frame_plan(plan: AsterRuntimeFramePlan) {
    free_frame_plan(plan);
}

fn free_frame_plan(plan: AsterRuntimeFramePlan) {
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

#[no_mangle]
pub extern "C" fn aster_runtime_free_frame_plan_v2(plan: *mut AsterRuntimeFramePlan) {
    if plan.is_null() {
        return;
    }
    let owned = unsafe { std::ptr::read(plan) };
    unsafe {
        *plan = AsterRuntimeFramePlan::default();
    }
    free_frame_plan(owned);
}

#[no_mangle]
pub extern "C" fn aster_runtime_measure_mesh_cut(
    vertices: *const AsterRuntimeVec3,
    vertex_count: usize,
    indices: *const u32,
    index_count: usize,
    out_measure: *mut AsterRuntimeMeshCutMeasure,
) -> u32 {
    if out_measure.is_null()
        || (vertices.is_null() && vertex_count > 0)
        || (indices.is_null() && index_count > 0)
    {
        return 0;
    }

    let vertex_slice = if vertex_count == 0 {
        &[]
    } else {
        unsafe { slice::from_raw_parts(vertices, vertex_count) }
    };
    let index_slice = if index_count == 0 {
        &[]
    } else {
        unsafe { slice::from_raw_parts(indices, index_count) }
    };
    let Some(measure) = measure_mesh_cut(vertex_slice, index_slice) else {
        unsafe {
            *out_measure = AsterRuntimeMeshCutMeasure::default();
        }
        return 0;
    };

    unsafe {
        *out_measure = measure;
    }
    1
}

pub mod software_raster {
    #[derive(Clone, Copy, Debug, Default)]
    pub struct RasterVertex {
        pub x: f32,
        pub y: f32,
        pub z: f32,
        pub rgba: [f32; 4],
    }

    #[derive(Clone, Debug)]
    pub struct RasterImage {
        pub width: usize,
        pub height: usize,
        pub color: Vec<[f32; 4]>,
        pub depth: Vec<f32>,
    }

    impl RasterImage {
        pub fn new(width: usize, height: usize) -> Self {
            let pixels = width.saturating_mul(height);
            Self {
                width,
                height,
                color: vec![[0.0, 0.0, 0.0, 0.0]; pixels],
                depth: vec![f32::INFINITY; pixels],
            }
        }

        pub fn clear(&mut self, rgba: [f32; 4]) {
            self.color.fill(rgba);
            self.depth.fill(f32::INFINITY);
        }

        pub fn pixel(&self, x: usize, y: usize) -> Option<[f32; 4]> {
            if x >= self.width || y >= self.height {
                return None;
            }
            self.color.get(y * self.width + x).copied()
        }
    }

    fn edge(a: RasterVertex, b: RasterVertex, x: f32, y: f32) -> f32 {
        (x - a.x) * (b.y - a.y) - (y - a.y) * (b.x - a.x)
    }

    fn lerp4(a: [f32; 4], b: [f32; 4], c: [f32; 4], wa: f32, wb: f32, wc: f32) -> [f32; 4] {
        [
            a[0] * wa + b[0] * wb + c[0] * wc,
            a[1] * wa + b[1] * wb + c[1] * wc,
            a[2] * wa + b[2] * wb + c[2] * wc,
            a[3] * wa + b[3] * wb + c[3] * wc,
        ]
    }

    pub fn rasterize_triangle(
        target: &mut RasterImage,
        a: RasterVertex,
        b: RasterVertex,
        c: RasterVertex,
    ) -> usize {
        if target.width == 0 || target.height == 0 {
            return 0;
        }
        let area = edge(a, b, c.x, c.y);
        if area.abs() <= f32::EPSILON {
            return 0;
        }
        let min_x = a.x.min(b.x).min(c.x).floor().max(0.0) as usize;
        let min_y = a.y.min(b.y).min(c.y).floor().max(0.0) as usize;
        let max_x =
            a.x.max(b.x)
                .max(c.x)
                .ceil()
                .min((target.width.saturating_sub(1)) as f32) as usize;
        let max_y =
            a.y.max(b.y)
                .max(c.y)
                .ceil()
                .min((target.height.saturating_sub(1)) as f32) as usize;
        let inv_area = 1.0 / area;
        let mut written = 0usize;
        for y in min_y..=max_y {
            for x in min_x..=max_x {
                let px = x as f32 + 0.5;
                let py = y as f32 + 0.5;
                let wa = edge(b, c, px, py) * inv_area;
                let wb = edge(c, a, px, py) * inv_area;
                let wc = edge(a, b, px, py) * inv_area;
                if wa < -0.00001 || wb < -0.00001 || wc < -0.00001 {
                    continue;
                }
                let depth = a.z * wa + b.z * wb + c.z * wc;
                let index = y * target.width + x;
                if depth >= target.depth[index] {
                    continue;
                }
                target.depth[index] = depth;
                target.color[index] = lerp4(a.rgba, b.rgba, c.rgba, wa, wb, wc);
                written += 1;
            }
        }
        written
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
            visibility_class: 0,
            position: AsterRuntimeVec3 { x, y: 0.0, z: 6.0 },
            visibility_cell: AsterRuntimeVec3::default(),
            bounds_center: AsterRuntimeVec3 { x, y: 0.0, z: 6.0 },
            bounds_radius: 0.5,
            opacity: 1.0,
            lod_max_distance: 0.0,
            lod_min_projected_radius: 0.0,
            portal_depth: 0.0,
            dynamic_mesh_generation: 0,
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
    #[cfg(target_pointer_width = "64")]
    fn abi_layout_matches_cxx_runtime_contract() {
        assert_eq!(std::mem::size_of::<AsterRuntimeVec3>(), 12);
        assert_eq!(std::mem::align_of::<AsterRuntimeVec3>(), 4);
        assert_eq!(std::mem::size_of::<AsterRuntimeRenderObject>(), 112);
        assert_eq!(std::mem::align_of::<AsterRuntimeRenderObject>(), 8);
        assert_eq!(std::mem::size_of::<AsterRuntimeCamera>(), 64);
        assert_eq!(std::mem::size_of::<AsterRuntimeLineOfSightFade>(), 48);
        assert_eq!(std::mem::size_of::<AsterRuntimeRenderPlanOptions>(), 48);
        assert_eq!(std::mem::size_of::<AsterRuntimeDrawInstance>(), 16);
        assert_eq!(std::mem::size_of::<AsterRuntimeDrawGroup>(), 40);
        assert_eq!(std::mem::size_of::<AsterRuntimeRenderDiagnostics>(), 88);
        assert_eq!(std::mem::size_of::<AsterRuntimeFramePlanBuffers>(), 56);
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
    fn reports_lod_visibility_and_dynamic_mesh_diagnostics() {
        let mut visible_dynamic = object(0, 0.0, 42, 7, 0);
        visible_dynamic.visibility_class = 3;
        visible_dynamic.dynamic_mesh_generation = 9;

        let mut lod_culled = object(1, 0.0, 43, 7, 0);
        lod_culled.lod_max_distance = 1.0;

        let output = build_frame_plan(
            &[visible_dynamic, lod_culled],
            camera(),
            AsterRuntimeRenderPlanOptions::default(),
        );
        assert_eq!(output.summary.visible_objects, 1);
        assert_eq!(output.summary.culled_objects, 1);
        assert_eq!(output.summary.lod_culled_objects, 1);
        assert_eq!(output.summary.visibility_hint_objects, 1);
        assert_eq!(output.summary.dynamic_mesh_objects, 1);
    }

    #[test]
    fn measures_cut_mesh_surface_and_bounds() {
        let vertices = [
            AsterRuntimeVec3 {
                x: 0.0,
                y: 0.0,
                z: 0.0,
            },
            AsterRuntimeVec3 {
                x: 1.0,
                y: 0.0,
                z: 0.0,
            },
            AsterRuntimeVec3 {
                x: 0.0,
                y: 1.0,
                z: 0.0,
            },
        ];
        let indices = [0u32, 1, 2];
        let measure = measure_mesh_cut(&vertices, &indices).expect("valid triangle");
        assert!((measure.surface_area - 0.5).abs() < 0.0001);
        assert!((measure.centroid.x - 1.0 / 3.0).abs() < 0.0001);
        assert!((measure.centroid.y - 1.0 / 3.0).abs() < 0.0001);
        assert_eq!(measure.bounds_max.x, 1.0);
        assert_eq!(measure.bounds_max.y, 1.0);
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

    #[test]
    fn software_raster_depth_tests_triangles() {
        let mut image = software_raster::RasterImage::new(8, 8);
        let far = [
            software_raster::RasterVertex {
                x: 1.0,
                y: 1.0,
                z: 0.8,
                rgba: [0.0, 0.0, 1.0, 1.0],
            },
            software_raster::RasterVertex {
                x: 6.0,
                y: 1.0,
                z: 0.8,
                rgba: [0.0, 0.0, 1.0, 1.0],
            },
            software_raster::RasterVertex {
                x: 1.0,
                y: 6.0,
                z: 0.8,
                rgba: [0.0, 0.0, 1.0, 1.0],
            },
        ];
        let near = [
            software_raster::RasterVertex {
                z: 0.2,
                rgba: [1.0, 0.0, 0.0, 1.0],
                ..far[0]
            },
            software_raster::RasterVertex {
                z: 0.2,
                rgba: [1.0, 0.0, 0.0, 1.0],
                ..far[1]
            },
            software_raster::RasterVertex {
                z: 0.2,
                rgba: [1.0, 0.0, 0.0, 1.0],
                ..far[2]
            },
        ];
        assert!(software_raster::rasterize_triangle(&mut image, far[0], far[1], far[2]) > 0);
        assert!(software_raster::rasterize_triangle(&mut image, near[0], near[1], near[2]) > 0);
        let pixel = image.pixel(2, 2).expect("covered pixel");
        assert!(pixel[0] > 0.9);
        assert!(pixel[2] < 0.1);
    }
}
