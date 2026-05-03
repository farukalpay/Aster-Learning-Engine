#ifndef ASSETDOC_H_INCLUDED__
#define ASSETDOC_H_INCLUDED__

#include <stddef.h>
#include <stdint.h> /* For uint8_t, uint32_t */

#ifdef __cplusplus
extern "C" {
#endif

typedef size_t assetdoc_size;
typedef long long int assetdoc_ssize;
typedef float assetdoc_float;
typedef int assetdoc_int;
typedef unsigned int assetdoc_uint;
typedef int assetdoc_bool;

typedef enum assetdoc_file_type
{
	assetdoc_file_type_invalid,
	assetdoc_file_type_scene_doc,
	assetdoc_file_type_glb,
	assetdoc_file_type_max_enum
} assetdoc_file_type;

typedef enum assetdoc_result
{
	assetdoc_result_success,
	assetdoc_result_data_too_short,
	assetdoc_result_unknown_format,
	assetdoc_result_invalid_json,
	assetdoc_result_invalid_scene_doc,
	assetdoc_result_invalid_options,
	assetdoc_result_file_not_found,
	assetdoc_result_io_error,
	assetdoc_result_out_of_memory,
	assetdoc_result_legacy_scene_doc,
    assetdoc_result_max_enum
} assetdoc_result;

typedef struct assetdoc_memory_options
{
	void* (*alloc_func)(void* user, assetdoc_size size);
	void (*free_func) (void* user, void* ptr);
	void* user_data;
} assetdoc_memory_options;

typedef struct assetdoc_file_options
{
	assetdoc_result(*read)(const struct assetdoc_memory_options* memory_options, const struct assetdoc_file_options* file_options, const char* path, assetdoc_size* size, void** data);
	void (*release)(const struct assetdoc_memory_options* memory_options, const struct assetdoc_file_options* file_options, void* data, assetdoc_size size);
	void* user_data;
} assetdoc_file_options;

typedef struct assetdoc_options
{
	assetdoc_file_type type; /* invalid == auto detect */
	assetdoc_size json_token_count; /* 0 == auto */
	assetdoc_memory_options memory;
	assetdoc_file_options file;
} assetdoc_options;

typedef enum assetdoc_buffer_view_type
{
	assetdoc_buffer_view_type_invalid,
	assetdoc_buffer_view_type_indices,
	assetdoc_buffer_view_type_vertices,
	assetdoc_buffer_view_type_max_enum
} assetdoc_buffer_view_type;

typedef enum assetdoc_attribute_type
{
	assetdoc_attribute_type_invalid,
	assetdoc_attribute_type_position,
	assetdoc_attribute_type_normal,
	assetdoc_attribute_type_tangent,
	assetdoc_attribute_type_texcoord,
	assetdoc_attribute_type_color,
	assetdoc_attribute_type_joints,
	assetdoc_attribute_type_weights,
	assetdoc_attribute_type_custom,
	assetdoc_attribute_type_max_enum
} assetdoc_attribute_type;

typedef enum assetdoc_component_type
{
	assetdoc_component_type_invalid,
	assetdoc_component_type_r_8, /* BYTE */
	assetdoc_component_type_r_8u, /* UNSIGNED_BYTE */
	assetdoc_component_type_r_16, /* SHORT */
	assetdoc_component_type_r_16u, /* UNSIGNED_SHORT */
	assetdoc_component_type_r_32u, /* UNSIGNED_INT */
	assetdoc_component_type_r_32f, /* FLOAT */
    assetdoc_component_type_max_enum
} assetdoc_component_type;

typedef enum assetdoc_type
{
	assetdoc_type_invalid,
	assetdoc_type_scalar,
	assetdoc_type_vec2,
	assetdoc_type_vec3,
	assetdoc_type_vec4,
	assetdoc_type_mat2,
	assetdoc_type_mat3,
	assetdoc_type_mat4,
	assetdoc_type_max_enum
} assetdoc_type;

typedef enum assetdoc_primitive_type
{
	assetdoc_primitive_type_invalid,
	assetdoc_primitive_type_points,
	assetdoc_primitive_type_lines,
	assetdoc_primitive_type_line_loop,
	assetdoc_primitive_type_line_strip,
	assetdoc_primitive_type_triangles,
	assetdoc_primitive_type_triangle_strip,
	assetdoc_primitive_type_triangle_fan,
	assetdoc_primitive_type_max_enum
} assetdoc_primitive_type;

typedef enum assetdoc_alpha_mode
{
	assetdoc_alpha_mode_opaque,
	assetdoc_alpha_mode_mask,
	assetdoc_alpha_mode_blend,
	assetdoc_alpha_mode_max_enum
} assetdoc_alpha_mode;

typedef enum assetdoc_animation_path_type {
	assetdoc_animation_path_type_invalid,
	assetdoc_animation_path_type_translation,
	assetdoc_animation_path_type_rotation,
	assetdoc_animation_path_type_scale,
	assetdoc_animation_path_type_weights,
	assetdoc_animation_path_type_max_enum
} assetdoc_animation_path_type;

typedef enum assetdoc_interpolation_type {
	assetdoc_interpolation_type_linear,
	assetdoc_interpolation_type_step,
	assetdoc_interpolation_type_cubic_spline,
	assetdoc_interpolation_type_max_enum
} assetdoc_interpolation_type;

typedef enum assetdoc_camera_type {
	assetdoc_camera_type_invalid,
	assetdoc_camera_type_perspective,
	assetdoc_camera_type_orthographic,
	assetdoc_camera_type_max_enum
} assetdoc_camera_type;

typedef enum assetdoc_light_type {
	assetdoc_light_type_invalid,
	assetdoc_light_type_directional,
	assetdoc_light_type_point,
	assetdoc_light_type_spot,
	assetdoc_light_type_max_enum
} assetdoc_light_type;

typedef enum assetdoc_data_free_method {
	assetdoc_data_free_method_none,
	assetdoc_data_free_method_file_release,
	assetdoc_data_free_method_memory_free,
	assetdoc_data_free_method_max_enum
} assetdoc_data_free_method;

typedef struct assetdoc_extras {
	assetdoc_size start_offset; /* this field is deprecated and will be removed in the future; use data instead */
	assetdoc_size end_offset; /* this field is deprecated and will be removed in the future; use data instead */

	char* data;
} assetdoc_extras;

typedef struct assetdoc_extension {
	char* name;
	char* data;
} assetdoc_extension;

typedef struct assetdoc_buffer
{
	char* name;
	assetdoc_size size;
	char* uri;
	void* data; /* loaded by assetdoc_load_buffers */
	assetdoc_data_free_method data_free_method;
	assetdoc_extras extras;
	assetdoc_size extensions_count;
	assetdoc_extension* extensions;
} assetdoc_buffer;

typedef enum assetdoc_meshwork_compression_mode {
	assetdoc_meshwork_compression_mode_invalid,
	assetdoc_meshwork_compression_mode_attributes,
	assetdoc_meshwork_compression_mode_triangles,
	assetdoc_meshwork_compression_mode_indices,
	assetdoc_meshwork_compression_mode_max_enum
} assetdoc_meshwork_compression_mode;

typedef enum assetdoc_meshwork_compression_filter {
	assetdoc_meshwork_compression_filter_none,
	assetdoc_meshwork_compression_filter_octahedral,
	assetdoc_meshwork_compression_filter_quaternion,
	assetdoc_meshwork_compression_filter_exponential,
	assetdoc_meshwork_compression_filter_color,
	assetdoc_meshwork_compression_filter_max_enum
} assetdoc_meshwork_compression_filter;

typedef struct assetdoc_meshwork_compression
{
	assetdoc_buffer* buffer;
	assetdoc_size offset;
	assetdoc_size size;
	assetdoc_size stride;
	assetdoc_size count;
	assetdoc_meshwork_compression_mode mode;
	assetdoc_meshwork_compression_filter filter;
	assetdoc_bool is_khr;
} assetdoc_meshwork_compression;

typedef struct assetdoc_buffer_view
{
	char *name;
	assetdoc_buffer* buffer;
	assetdoc_size offset;
	assetdoc_size size;
	assetdoc_size stride; /* 0 == automatically determined by accessor */
	assetdoc_buffer_view_type type;
	void* data; /* overrides buffer->data if present, filled by extensions */
	assetdoc_bool has_meshwork_compression;
	assetdoc_meshwork_compression meshwork_compression;
	assetdoc_extras extras;
	assetdoc_size extensions_count;
	assetdoc_extension* extensions;
} assetdoc_buffer_view;

typedef struct assetdoc_accessor_sparse
{
	assetdoc_size count;
	assetdoc_buffer_view* indices_buffer_view;
	assetdoc_size indices_byte_offset;
	assetdoc_component_type indices_component_type;
	assetdoc_buffer_view* values_buffer_view;
	assetdoc_size values_byte_offset;
} assetdoc_accessor_sparse;

typedef struct assetdoc_accessor
{
	char* name;
	assetdoc_component_type component_type;
	assetdoc_bool normalized;
	assetdoc_type type;
	assetdoc_size offset;
	assetdoc_size count;
	assetdoc_size stride;
	assetdoc_buffer_view* buffer_view;
	assetdoc_bool has_min;
	assetdoc_float min[16];
	assetdoc_bool has_max;
	assetdoc_float max[16];
	assetdoc_bool is_sparse;
	assetdoc_accessor_sparse sparse;
	assetdoc_extras extras;
	assetdoc_size extensions_count;
	assetdoc_extension* extensions;
} assetdoc_accessor;

typedef struct assetdoc_attribute
{
	char* name;
	assetdoc_attribute_type type;
	assetdoc_int index;
	assetdoc_accessor* data;
} assetdoc_attribute;

typedef struct assetdoc_image
{
	char* name;
	char* uri;
	assetdoc_buffer_view* buffer_view;
	char* mime_type;
	assetdoc_extras extras;
	assetdoc_size extensions_count;
	assetdoc_extension* extensions;
} assetdoc_image;

typedef enum assetdoc_filter_type {
    assetdoc_filter_type_undefined = 0,
    assetdoc_filter_type_nearest = 9728,
    assetdoc_filter_type_linear = 9729,
    assetdoc_filter_type_nearest_mipmap_nearest = 9984,
    assetdoc_filter_type_linear_mipmap_nearest = 9985,
    assetdoc_filter_type_nearest_mipmap_linear = 9986,
    assetdoc_filter_type_linear_mipmap_linear = 9987
} assetdoc_filter_type;

typedef enum assetdoc_wrap_mode {
    assetdoc_wrap_mode_clamp_to_edge = 33071,
    assetdoc_wrap_mode_mirrored_repeat = 33648,
    assetdoc_wrap_mode_repeat = 10497
} assetdoc_wrap_mode;

typedef struct assetdoc_sampler
{
	char* name;
	assetdoc_filter_type mag_filter;
	assetdoc_filter_type min_filter;
	assetdoc_wrap_mode wrap_s;
	assetdoc_wrap_mode wrap_t;
	assetdoc_extras extras;
	assetdoc_size extensions_count;
	assetdoc_extension* extensions;
} assetdoc_sampler;

typedef struct assetdoc_texture
{
	char* name;
	assetdoc_image* image;
	assetdoc_sampler* sampler;
	assetdoc_bool has_basisu;
	assetdoc_image* basisu_image;
	assetdoc_bool has_webp;
	assetdoc_image* webp_image;
	assetdoc_extras extras;
	assetdoc_size extensions_count;
	assetdoc_extension* extensions;
} assetdoc_texture;

typedef struct assetdoc_texture_transform
{
	assetdoc_float offset[2];
	assetdoc_float rotation;
	assetdoc_float scale[2];
	assetdoc_bool has_texcoord;
	assetdoc_int texcoord;
} assetdoc_texture_transform;

typedef struct assetdoc_texture_view
{
	assetdoc_texture* texture;
	assetdoc_int texcoord;
	assetdoc_float scale; /* equivalent to strength for occlusion_texture */
	assetdoc_bool has_transform;
	assetdoc_texture_transform transform;
} assetdoc_texture_view;

typedef struct assetdoc_pbr_metallic_roughness
{
	assetdoc_texture_view base_color_texture;
	assetdoc_texture_view metallic_roughness_texture;

	assetdoc_float base_color_factor[4];
	assetdoc_float metallic_factor;
	assetdoc_float roughness_factor;
} assetdoc_pbr_metallic_roughness;

typedef struct assetdoc_pbr_specular_glossiness
{
	assetdoc_texture_view diffuse_texture;
	assetdoc_texture_view specular_glossiness_texture;

	assetdoc_float diffuse_factor[4];
	assetdoc_float specular_factor[3];
	assetdoc_float glossiness_factor;
} assetdoc_pbr_specular_glossiness;

typedef struct assetdoc_clearcoat
{
	assetdoc_texture_view clearcoat_texture;
	assetdoc_texture_view clearcoat_roughness_texture;
	assetdoc_texture_view clearcoat_normal_texture;

	assetdoc_float clearcoat_factor;
	assetdoc_float clearcoat_roughness_factor;
} assetdoc_clearcoat;

typedef struct assetdoc_transmission
{
	assetdoc_texture_view transmission_texture;
	assetdoc_float transmission_factor;
} assetdoc_transmission;

typedef struct assetdoc_ior
{
	assetdoc_float ior;
} assetdoc_ior;

typedef struct assetdoc_specular
{
	assetdoc_texture_view specular_texture;
	assetdoc_texture_view specular_color_texture;
	assetdoc_float specular_color_factor[3];
	assetdoc_float specular_factor;
} assetdoc_specular;

typedef struct assetdoc_volume
{
	assetdoc_texture_view thickness_texture;
	assetdoc_float thickness_factor;
	assetdoc_float attenuation_color[3];
	assetdoc_float attenuation_distance;
} assetdoc_volume;

typedef struct assetdoc_sheen
{
	assetdoc_texture_view sheen_color_texture;
	assetdoc_float sheen_color_factor[3];
	assetdoc_texture_view sheen_roughness_texture;
	assetdoc_float sheen_roughness_factor;
} assetdoc_sheen;

typedef struct assetdoc_emissive_strength
{
	assetdoc_float emissive_strength;
} assetdoc_emissive_strength;

typedef struct assetdoc_iridescence
{
	assetdoc_float iridescence_factor;
	assetdoc_texture_view iridescence_texture;
	assetdoc_float iridescence_ior;
	assetdoc_float iridescence_thickness_min;
	assetdoc_float iridescence_thickness_max;
	assetdoc_texture_view iridescence_thickness_texture;
} assetdoc_iridescence;

typedef struct assetdoc_diffuse_transmission
{
	assetdoc_texture_view diffuse_transmission_texture;
	assetdoc_float diffuse_transmission_factor;
	assetdoc_float diffuse_transmission_color_factor[3];
	assetdoc_texture_view diffuse_transmission_color_texture;
} assetdoc_diffuse_transmission;

typedef struct assetdoc_anisotropy
{
	assetdoc_float anisotropy_strength;
	assetdoc_float anisotropy_rotation;
	assetdoc_texture_view anisotropy_texture;
} assetdoc_anisotropy;

typedef struct assetdoc_dispersion
{
	assetdoc_float dispersion;
} assetdoc_dispersion;

typedef struct assetdoc_material
{
	char* name;
	assetdoc_bool has_pbr_metallic_roughness;
	assetdoc_bool has_pbr_specular_glossiness;
	assetdoc_bool has_clearcoat;
	assetdoc_bool has_transmission;
	assetdoc_bool has_volume;
	assetdoc_bool has_ior;
	assetdoc_bool has_specular;
	assetdoc_bool has_sheen;
	assetdoc_bool has_emissive_strength;
	assetdoc_bool has_iridescence;
	assetdoc_bool has_diffuse_transmission;
	assetdoc_bool has_anisotropy;
	assetdoc_bool has_dispersion;
	assetdoc_pbr_metallic_roughness pbr_metallic_roughness;
	assetdoc_pbr_specular_glossiness pbr_specular_glossiness;
	assetdoc_clearcoat clearcoat;
	assetdoc_ior ior;
	assetdoc_specular specular;
	assetdoc_sheen sheen;
	assetdoc_transmission transmission;
	assetdoc_volume volume;
	assetdoc_emissive_strength emissive_strength;
	assetdoc_iridescence iridescence;
	assetdoc_diffuse_transmission diffuse_transmission;
	assetdoc_anisotropy anisotropy;
	assetdoc_dispersion dispersion;
	assetdoc_texture_view normal_texture;
	assetdoc_texture_view occlusion_texture;
	assetdoc_texture_view emissive_texture;
	assetdoc_float emissive_factor[3];
	assetdoc_alpha_mode alpha_mode;
	assetdoc_float alpha_cutoff;
	assetdoc_bool double_sided;
	assetdoc_bool unlit;
	assetdoc_extras extras;
	assetdoc_size extensions_count;
	assetdoc_extension* extensions;
} assetdoc_material;

typedef struct assetdoc_material_mapping
{
	assetdoc_size variant;
	assetdoc_material* material;
	assetdoc_extras extras;
} assetdoc_material_mapping;

typedef struct assetdoc_morph_target {
	assetdoc_attribute* attributes;
	assetdoc_size attributes_count;
} assetdoc_morph_target;

typedef struct assetdoc_draco_mesh_compression {
	assetdoc_buffer_view* buffer_view;
	assetdoc_attribute* attributes;
	assetdoc_size attributes_count;
} assetdoc_draco_mesh_compression;

typedef struct assetdoc_mesh_gpu_instancing {
	assetdoc_attribute* attributes;
	assetdoc_size attributes_count;
} assetdoc_mesh_gpu_instancing;

typedef struct assetdoc_primitive {
	assetdoc_primitive_type type;
	assetdoc_accessor* indices;
	assetdoc_material* material;
	assetdoc_attribute* attributes;
	assetdoc_size attributes_count;
	assetdoc_morph_target* targets;
	assetdoc_size targets_count;
	assetdoc_extras extras;
	assetdoc_bool has_draco_mesh_compression;
	assetdoc_draco_mesh_compression draco_mesh_compression;
	assetdoc_material_mapping* mappings;
	assetdoc_size mappings_count;
	assetdoc_size extensions_count;
	assetdoc_extension* extensions;
} assetdoc_primitive;

typedef struct assetdoc_mesh {
	char* name;
	assetdoc_primitive* primitives;
	assetdoc_size primitives_count;
	assetdoc_float* weights;
	assetdoc_size weights_count;
	char** target_names;
	assetdoc_size target_names_count;
	assetdoc_extras extras;
	assetdoc_size extensions_count;
	assetdoc_extension* extensions;
} assetdoc_mesh;

typedef struct assetdoc_node assetdoc_node;

typedef struct assetdoc_skin {
	char* name;
	assetdoc_node** joints;
	assetdoc_size joints_count;
	assetdoc_node* skeleton;
	assetdoc_accessor* inverse_bind_matrices;
	assetdoc_extras extras;
	assetdoc_size extensions_count;
	assetdoc_extension* extensions;
} assetdoc_skin;

typedef struct assetdoc_camera_perspective {
	assetdoc_bool has_aspect_ratio;
	assetdoc_float aspect_ratio;
	assetdoc_float yfov;
	assetdoc_bool has_zfar;
	assetdoc_float zfar;
	assetdoc_float znear;
	assetdoc_extras extras;
} assetdoc_camera_perspective;

typedef struct assetdoc_camera_orthographic {
	assetdoc_float xmag;
	assetdoc_float ymag;
	assetdoc_float zfar;
	assetdoc_float znear;
	assetdoc_extras extras;
} assetdoc_camera_orthographic;

typedef struct assetdoc_camera {
	char* name;
	assetdoc_camera_type type;
	union {
		assetdoc_camera_perspective perspective;
		assetdoc_camera_orthographic orthographic;
	} data;
	assetdoc_extras extras;
	assetdoc_size extensions_count;
	assetdoc_extension* extensions;
} assetdoc_camera;

typedef struct assetdoc_light {
	char* name;
	assetdoc_float color[3];
	assetdoc_float intensity;
	assetdoc_light_type type;
	assetdoc_float range;
	assetdoc_float spot_inner_cone_angle;
	assetdoc_float spot_outer_cone_angle;
	assetdoc_extras extras;
} assetdoc_light;

struct assetdoc_node {
	char* name;
	assetdoc_node* parent;
	assetdoc_node** children;
	assetdoc_size children_count;
	assetdoc_skin* skin;
	assetdoc_mesh* mesh;
	assetdoc_camera* camera;
	assetdoc_light* light;
	assetdoc_float* weights;
	assetdoc_size weights_count;
	assetdoc_bool has_translation;
	assetdoc_bool has_rotation;
	assetdoc_bool has_scale;
	assetdoc_bool has_matrix;
	assetdoc_float translation[3];
	assetdoc_float rotation[4];
	assetdoc_float scale[3];
	assetdoc_float matrix[16];
	assetdoc_extras extras;
	assetdoc_bool has_mesh_gpu_instancing;
	assetdoc_mesh_gpu_instancing mesh_gpu_instancing;
	assetdoc_size extensions_count;
	assetdoc_extension* extensions;
};

typedef struct assetdoc_scene {
	char* name;
	assetdoc_node** nodes;
	assetdoc_size nodes_count;
	assetdoc_extras extras;
	assetdoc_size extensions_count;
	assetdoc_extension* extensions;
} assetdoc_scene;

typedef struct assetdoc_animation_sampler {
	assetdoc_accessor* input;
	assetdoc_accessor* output;
	assetdoc_interpolation_type interpolation;
	assetdoc_extras extras;
	assetdoc_size extensions_count;
	assetdoc_extension* extensions;
} assetdoc_animation_sampler;

typedef struct assetdoc_animation_channel {
	assetdoc_animation_sampler* sampler;
	assetdoc_node* target_node;
	assetdoc_animation_path_type target_path;
	assetdoc_extras extras;
	assetdoc_size extensions_count;
	assetdoc_extension* extensions;
} assetdoc_animation_channel;

typedef struct assetdoc_animation {
	char* name;
	assetdoc_animation_sampler* samplers;
	assetdoc_size samplers_count;
	assetdoc_animation_channel* channels;
	assetdoc_size channels_count;
	assetdoc_extras extras;
	assetdoc_size extensions_count;
	assetdoc_extension* extensions;
} assetdoc_animation;

typedef struct assetdoc_material_variant
{
	char* name;
	assetdoc_extras extras;
} assetdoc_material_variant;

typedef struct assetdoc_asset {
	char* copyright;
	char* generator;
	char* version;
	char* min_version;
	assetdoc_extras extras;
	assetdoc_size extensions_count;
	assetdoc_extension* extensions;
} assetdoc_asset;

typedef struct assetdoc_data
{
	assetdoc_file_type file_type;
	void* file_data;
	assetdoc_size file_size;

	assetdoc_asset asset;

	assetdoc_mesh* meshes;
	assetdoc_size meshes_count;

	assetdoc_material* materials;
	assetdoc_size materials_count;

	assetdoc_accessor* accessors;
	assetdoc_size accessors_count;

	assetdoc_buffer_view* buffer_views;
	assetdoc_size buffer_views_count;

	assetdoc_buffer* buffers;
	assetdoc_size buffers_count;

	assetdoc_image* images;
	assetdoc_size images_count;

	assetdoc_texture* textures;
	assetdoc_size textures_count;

	assetdoc_sampler* samplers;
	assetdoc_size samplers_count;

	assetdoc_skin* skins;
	assetdoc_size skins_count;

	assetdoc_camera* cameras;
	assetdoc_size cameras_count;

	assetdoc_light* lights;
	assetdoc_size lights_count;

	assetdoc_node* nodes;
	assetdoc_size nodes_count;

	assetdoc_scene* scenes;
	assetdoc_size scenes_count;

	assetdoc_scene* scene;

	assetdoc_animation* animations;
	assetdoc_size animations_count;

	assetdoc_material_variant* variants;
	assetdoc_size variants_count;

	assetdoc_extras extras;

	assetdoc_size data_extensions_count;
	assetdoc_extension* data_extensions;

	char** extensions_used;
	assetdoc_size extensions_used_count;

	char** extensions_required;
	assetdoc_size extensions_required_count;

	const char* json;
	assetdoc_size json_size;

	const void* bin;
	assetdoc_size bin_size;

	assetdoc_memory_options memory;
	assetdoc_file_options file;
} assetdoc_data;

assetdoc_result assetdoc_parse(
		const assetdoc_options* options,
		const void* data,
		assetdoc_size size,
		assetdoc_data** out_data);

assetdoc_result assetdoc_parse_file(
		const assetdoc_options* options,
		const char* path,
		assetdoc_data** out_data);

assetdoc_result assetdoc_load_buffers(
		const assetdoc_options* options,
		assetdoc_data* data,
		const char* scene_doc_path);

assetdoc_result assetdoc_load_buffer_base64(const assetdoc_options* options, assetdoc_size size, const char* base64, void** out_data);

assetdoc_size assetdoc_decode_string(char* string);
assetdoc_size assetdoc_decode_uri(char* uri);

assetdoc_result assetdoc_validate(assetdoc_data* data);

void assetdoc_free(assetdoc_data* data);

void assetdoc_node_transform_local(const assetdoc_node* node, assetdoc_float* out_matrix);
void assetdoc_node_transform_world(const assetdoc_node* node, assetdoc_float* out_matrix);

const uint8_t* assetdoc_buffer_view_data(const assetdoc_buffer_view* view);

const assetdoc_accessor* assetdoc_find_accessor(const assetdoc_primitive* prim, assetdoc_attribute_type type, assetdoc_int index);

assetdoc_bool assetdoc_accessor_read_float(const assetdoc_accessor* accessor, assetdoc_size index, assetdoc_float* out, assetdoc_size element_size);
assetdoc_bool assetdoc_accessor_read_uint(const assetdoc_accessor* accessor, assetdoc_size index, assetdoc_uint* out, assetdoc_size element_size);
assetdoc_size assetdoc_accessor_read_index(const assetdoc_accessor* accessor, assetdoc_size index);

assetdoc_size assetdoc_num_components(assetdoc_type type);
assetdoc_size assetdoc_component_size(assetdoc_component_type component_type);
assetdoc_size assetdoc_calc_size(assetdoc_type type, assetdoc_component_type component_type);

assetdoc_size assetdoc_accessor_unpack_floats(const assetdoc_accessor* accessor, assetdoc_float* out, assetdoc_size float_count);
assetdoc_size assetdoc_accessor_unpack_indices(const assetdoc_accessor* accessor, void* out, assetdoc_size out_component_size, assetdoc_size index_count);

/* this function is deprecated and will be removed in the future; use assetdoc_extras::data instead */
assetdoc_result assetdoc_copy_extras_json(const assetdoc_data* data, const assetdoc_extras* extras, char* dest, assetdoc_size* dest_size);

assetdoc_size assetdoc_mesh_index(const assetdoc_data* data, const assetdoc_mesh* object);
assetdoc_size assetdoc_material_index(const assetdoc_data* data, const assetdoc_material* object);
assetdoc_size assetdoc_accessor_index(const assetdoc_data* data, const assetdoc_accessor* object);
assetdoc_size assetdoc_buffer_view_index(const assetdoc_data* data, const assetdoc_buffer_view* object);
assetdoc_size assetdoc_buffer_index(const assetdoc_data* data, const assetdoc_buffer* object);
assetdoc_size assetdoc_image_index(const assetdoc_data* data, const assetdoc_image* object);
assetdoc_size assetdoc_texture_index(const assetdoc_data* data, const assetdoc_texture* object);
assetdoc_size assetdoc_sampler_index(const assetdoc_data* data, const assetdoc_sampler* object);
assetdoc_size assetdoc_skin_index(const assetdoc_data* data, const assetdoc_skin* object);
assetdoc_size assetdoc_camera_index(const assetdoc_data* data, const assetdoc_camera* object);
assetdoc_size assetdoc_light_index(const assetdoc_data* data, const assetdoc_light* object);
assetdoc_size assetdoc_node_index(const assetdoc_data* data, const assetdoc_node* object);
assetdoc_size assetdoc_scene_index(const assetdoc_data* data, const assetdoc_scene* object);
assetdoc_size assetdoc_animation_index(const assetdoc_data* data, const assetdoc_animation* object);
assetdoc_size assetdoc_animation_sampler_index(const assetdoc_animation* animation, const assetdoc_animation_sampler* object);
assetdoc_size assetdoc_animation_channel_index(const assetdoc_animation* animation, const assetdoc_animation_channel* object);

#ifdef __cplusplus
}
#endif

#endif /* #ifndef ASSETDOC_H_INCLUDED__ */

/*
 *
 * Stop now, if you are only interested in the API.
 * Below, you find the implementation.
 *
 */

#if defined(__INTELLISENSE__) || defined(__JETBRAINS_IDE__)
/* This makes MSVC/CLion intellisense work. */
#define ASSETDOC_IMPLEMENTATION
#endif

#ifdef ASSETDOC_IMPLEMENTATION

#include <assert.h> /* For assert */
#include <string.h> /* For strncpy */
#include <stdio.h>  /* For fopen */
#include <limits.h> /* For UINT_MAX etc */
#include <float.h>  /* For FLT_MAX */

#if !defined(ASSETDOC_MALLOC) || !defined(ASSETDOC_FREE) || !defined(ASSETDOC_ATOI) || !defined(ASSETDOC_ATOF) || !defined(ASSETDOC_ATOLL)
#include <stdlib.h> /* For malloc, free, atoi, atof */
#endif

/* JSMN_PARENT_LINKS is necessary to make parsing large structures linear in input size */
#define JSMN_PARENT_LINKS

/* JSMN_STRICT is necessary to reject invalid JSON documents */
#define JSMN_STRICT

/*
 * -- jsmn.h start --
 */
typedef enum {
	JSMN_UNDEFINED = 0,
	JSMN_OBJECT = 1,
	JSMN_ARRAY = 2,
	JSMN_STRING = 3,
	JSMN_PRIMITIVE = 4
} jsmntype_t;
enum jsmnerr {
	/* Not enough tokens were provided */
	JSMN_ERROR_NOMEM = -1,
	/* Invalid character inside JSON string */
	JSMN_ERROR_INVAL = -2,
	/* The string is not a full JSON packet, more bytes expected */
	JSMN_ERROR_PART = -3
};
typedef struct {
	jsmntype_t type;
	ptrdiff_t start;
	ptrdiff_t end;
	int size;
#ifdef JSMN_PARENT_LINKS
	int parent;
#endif
} jsmntok_t;
typedef struct {
	size_t pos; /* offset in the JSON string */
	unsigned int toknext; /* next token to allocate */
	int toksuper; /* superior token node, e.g parent object or array */
} jsmn_parser;
static void jsmn_init(jsmn_parser *parser);
static int jsmn_parse(jsmn_parser *parser, const char *js, size_t len, jsmntok_t *tokens, size_t num_tokens);
/*
 * -- jsmn.h end --
 */


#ifndef ASSETDOC_CONSTS
#define GlbHeaderSize 12
#define GlbChunkHeaderSize 8
static const uint32_t GlbVersion = 2;
static const uint32_t GlbMagic = 0x46546C67;
static const uint32_t GlbMagicJsonChunk = 0x4E4F534A;
static const uint32_t GlbMagicBinChunk = 0x004E4942;
#define ASSETDOC_CONSTS
#endif

#ifndef ASSETDOC_MALLOC
#define ASSETDOC_MALLOC(size) malloc(size)
#endif
#ifndef ASSETDOC_FREE
#define ASSETDOC_FREE(ptr) free(ptr)
#endif
#ifndef ASSETDOC_ATOI
#define ASSETDOC_ATOI(str) atoi(str)
#endif
#ifndef ASSETDOC_ATOF
#define ASSETDOC_ATOF(str) atof(str)
#endif
#ifndef ASSETDOC_ATOLL
#define ASSETDOC_ATOLL(str) atoll(str)
#endif
#ifndef ASSETDOC_VALIDATE_ENABLE_ASSERTS
#define ASSETDOC_VALIDATE_ENABLE_ASSERTS 0
#endif

static void* assetdoc_default_alloc(void* user, assetdoc_size size)
{
	(void)user;
	return ASSETDOC_MALLOC(size);
}

static void assetdoc_default_free(void* user, void* ptr)
{
	(void)user;
	ASSETDOC_FREE(ptr);
}

static void* assetdoc_calloc(assetdoc_options* options, size_t element_size, assetdoc_size count)
{
	if (SIZE_MAX / element_size < count)
	{
		return NULL;
	}
	void* result = options->memory.alloc_func(options->memory.user_data, element_size * count);
	if (!result)
	{
		return NULL;
	}
	memset(result, 0, element_size * count);
	return result;
}

static assetdoc_result assetdoc_default_file_read(const struct assetdoc_memory_options* memory_options, const struct assetdoc_file_options* file_options, const char* path, assetdoc_size* size, void** data)
{
	(void)file_options;
	void* (*memory_alloc)(void*, assetdoc_size) = memory_options->alloc_func ? memory_options->alloc_func : &assetdoc_default_alloc;
	void (*memory_free)(void*, void*) = memory_options->free_func ? memory_options->free_func : &assetdoc_default_free;

	FILE* file = fopen(path, "rb");
	if (!file)
	{
		return assetdoc_result_file_not_found;
	}

	assetdoc_size file_size = size ? *size : 0;

	if (file_size == 0)
	{
		fseek(file, 0, SEEK_END);

#ifdef _MSC_VER
		__int64 length = _ftelli64(file);
#else
		long length = ftell(file);
#endif

		if (length < 0)
		{
			fclose(file);
			return assetdoc_result_io_error;
		}

		fseek(file, 0, SEEK_SET);
		file_size = (assetdoc_size)length;
	}

	char* file_data = (char*)memory_alloc(memory_options->user_data, file_size);
	if (!file_data)
	{
		fclose(file);
		return assetdoc_result_out_of_memory;
	}

	assetdoc_size read_size = fread(file_data, 1, file_size, file);

	fclose(file);

	if (read_size != file_size)
	{
		memory_free(memory_options->user_data, file_data);
		return assetdoc_result_io_error;
	}

	if (size)
	{
		*size = file_size;
	}
	if (data)
	{
		*data = file_data;
	}

	return assetdoc_result_success;
}

static void assetdoc_default_file_release(const struct assetdoc_memory_options* memory_options, const struct assetdoc_file_options* file_options, void* data, assetdoc_size size)
{
	(void)file_options;
	(void)size;
	void (*memfree)(void*, void*) = memory_options->free_func ? memory_options->free_func : &assetdoc_default_free;
	memfree(memory_options->user_data, data);
}

static assetdoc_result assetdoc_parse_json(assetdoc_options* options, const uint8_t* json_chunk, assetdoc_size size, assetdoc_data** out_data);

assetdoc_result assetdoc_parse(const assetdoc_options* options, const void* data, assetdoc_size size, assetdoc_data** out_data)
{
	if (size < GlbHeaderSize)
	{
		return assetdoc_result_data_too_short;
	}

	if (options == NULL)
	{
		return assetdoc_result_invalid_options;
	}

	assetdoc_options fixed_options = *options;
	if (fixed_options.memory.alloc_func == NULL)
	{
		fixed_options.memory.alloc_func = &assetdoc_default_alloc;
	}
	if (fixed_options.memory.free_func == NULL)
	{
		fixed_options.memory.free_func = &assetdoc_default_free;
	}

	uint32_t tmp;
	// Magic
	memcpy(&tmp, data, 4);
	if (tmp != GlbMagic)
	{
		if (fixed_options.type == assetdoc_file_type_invalid)
		{
			fixed_options.type = assetdoc_file_type_scene_doc;
		}
		else if (fixed_options.type == assetdoc_file_type_glb)
		{
			return assetdoc_result_unknown_format;
		}
	}

	if (fixed_options.type == assetdoc_file_type_scene_doc)
	{
		assetdoc_result json_result = assetdoc_parse_json(&fixed_options, (const uint8_t*)data, size, out_data);
		if (json_result != assetdoc_result_success)
		{
			return json_result;
		}

		(*out_data)->file_type = assetdoc_file_type_scene_doc;

		return assetdoc_result_success;
	}

	const uint8_t* ptr = (const uint8_t*)data;
	// Version
	memcpy(&tmp, ptr + 4, 4);
	uint32_t version = tmp;
	if (version != GlbVersion)
	{
		return version < GlbVersion ? assetdoc_result_legacy_scene_doc : assetdoc_result_unknown_format;
	}

	// Total length
	memcpy(&tmp, ptr + 8, 4);
	if (tmp > size)
	{
		return assetdoc_result_data_too_short;
	}

	const uint8_t* json_chunk = ptr + GlbHeaderSize;

	if (GlbHeaderSize + GlbChunkHeaderSize > size)
	{
		return assetdoc_result_data_too_short;
	}

	// JSON chunk: length
	uint32_t json_length;
	memcpy(&json_length, json_chunk, 4);
	if (json_length > size - GlbHeaderSize - GlbChunkHeaderSize)
	{
		return assetdoc_result_data_too_short;
	}

	// JSON chunk: magic
	memcpy(&tmp, json_chunk + 4, 4);
	if (tmp != GlbMagicJsonChunk)
	{
		return assetdoc_result_unknown_format;
	}

	json_chunk += GlbChunkHeaderSize;

	const void* bin = NULL;
	assetdoc_size bin_size = 0;

	if (GlbChunkHeaderSize <= size - GlbHeaderSize - GlbChunkHeaderSize - json_length)
	{
		// We can read another chunk
		const uint8_t* bin_chunk = json_chunk + json_length;

		// Bin chunk: length
		uint32_t bin_length;
		memcpy(&bin_length, bin_chunk, 4);
		if (bin_length > size - GlbHeaderSize - GlbChunkHeaderSize - json_length - GlbChunkHeaderSize)
		{
			return assetdoc_result_data_too_short;
		}

		// Bin chunk: magic
		memcpy(&tmp, bin_chunk + 4, 4);
		if (tmp != GlbMagicBinChunk)
		{
			return assetdoc_result_unknown_format;
		}

		bin_chunk += GlbChunkHeaderSize;

		bin = bin_chunk;
		bin_size = bin_length;
	}

	assetdoc_result json_result = assetdoc_parse_json(&fixed_options, json_chunk, json_length, out_data);
	if (json_result != assetdoc_result_success)
	{
		return json_result;
	}

	(*out_data)->file_type = assetdoc_file_type_glb;
	(*out_data)->bin = bin;
	(*out_data)->bin_size = bin_size;

	return assetdoc_result_success;
}

assetdoc_result assetdoc_parse_file(const assetdoc_options* options, const char* path, assetdoc_data** out_data)
{
	if (options == NULL)
	{
		return assetdoc_result_invalid_options;
	}

	assetdoc_result (*file_read)(const struct assetdoc_memory_options*, const struct assetdoc_file_options*, const char*, assetdoc_size*, void**) = options->file.read ? options->file.read : &assetdoc_default_file_read;
	void (*file_release)(const struct assetdoc_memory_options*, const struct assetdoc_file_options*, void* data, assetdoc_size size) = options->file.release ? options->file.release : assetdoc_default_file_release;

	void* file_data = NULL;
	assetdoc_size file_size = 0;
	assetdoc_result result = file_read(&options->memory, &options->file, path, &file_size, &file_data);
	if (result != assetdoc_result_success)
	{
		return result;
	}

	result = assetdoc_parse(options, file_data, file_size, out_data);

	if (result != assetdoc_result_success)
	{
		file_release(&options->memory, &options->file, file_data, file_size);
		return result;
	}

	(*out_data)->file_data = file_data;
	(*out_data)->file_size = file_size;

	return assetdoc_result_success;
}

static void assetdoc_combine_paths(char* path, const char* base, const char* uri)
{
	const char* s0 = strrchr(base, '/');
	const char* s1 = strrchr(base, '\\');
	const char* slash = s0 ? (s1 && s1 > s0 ? s1 : s0) : s1;

	if (slash)
	{
		size_t prefix = slash - base + 1;

		strncpy(path, base, prefix);
		strcpy(path + prefix, uri);
	}
	else
	{
		strcpy(path, uri);
	}
}

static assetdoc_result assetdoc_load_buffer_file(const assetdoc_options* options, assetdoc_size size, const char* uri, const char* scene_doc_path, void** out_data)
{
	void* (*memory_alloc)(void*, assetdoc_size) = options->memory.alloc_func ? options->memory.alloc_func : &assetdoc_default_alloc;
	void (*memory_free)(void*, void*) = options->memory.free_func ? options->memory.free_func : &assetdoc_default_free;
	assetdoc_result (*file_read)(const struct assetdoc_memory_options*, const struct assetdoc_file_options*, const char*, assetdoc_size*, void**) = options->file.read ? options->file.read : &assetdoc_default_file_read;

	char* path = (char*)memory_alloc(options->memory.user_data, strlen(uri) + strlen(scene_doc_path) + 1);
	if (!path)
	{
		return assetdoc_result_out_of_memory;
	}

	assetdoc_combine_paths(path, scene_doc_path, uri);

	// after combining, the tail of the resulting path is a uri; decode_uri converts it into path
	assetdoc_decode_uri(path + strlen(path) - strlen(uri));

	void* file_data = NULL;
	assetdoc_result result = file_read(&options->memory, &options->file, path, &size, &file_data);

	memory_free(options->memory.user_data, path);

	*out_data = (result == assetdoc_result_success) ? file_data : NULL;

	return result;
}

assetdoc_result assetdoc_load_buffer_base64(const assetdoc_options* options, assetdoc_size size, const char* base64, void** out_data)
{
	void* (*memory_alloc)(void*, assetdoc_size) = options->memory.alloc_func ? options->memory.alloc_func : &assetdoc_default_alloc;
	void (*memory_free)(void*, void*) = options->memory.free_func ? options->memory.free_func : &assetdoc_default_free;

	unsigned char* data = (unsigned char*)memory_alloc(options->memory.user_data, size);
	if (!data)
	{
		return assetdoc_result_out_of_memory;
	}

	unsigned int buffer = 0;
	unsigned int buffer_bits = 0;

	for (assetdoc_size i = 0; i < size; ++i)
	{
		while (buffer_bits < 8)
		{
			char ch = *base64++;

			int index =
				(unsigned)(ch - 'A') < 26 ? (ch - 'A') :
				(unsigned)(ch - 'a') < 26 ? (ch - 'a') + 26 :
				(unsigned)(ch - '0') < 10 ? (ch - '0') + 52 :
				ch == '+' ? 62 :
				ch == '/' ? 63 :
				-1;

			if (index < 0)
			{
				memory_free(options->memory.user_data, data);
				return assetdoc_result_io_error;
			}

			buffer = (buffer << 6) | index;
			buffer_bits += 6;
		}

		data[i] = (unsigned char)(buffer >> (buffer_bits - 8));
		buffer_bits -= 8;
	}

	*out_data = data;

	return assetdoc_result_success;
}

static int assetdoc_unhex(char ch)
{
	return
		(unsigned)(ch - '0') < 10 ? (ch - '0') :
		(unsigned)(ch - 'A') < 6 ? (ch - 'A') + 10 :
		(unsigned)(ch - 'a') < 6 ? (ch - 'a') + 10 :
		-1;
}

assetdoc_size assetdoc_decode_string(char* string)
{
	char* read = string + strcspn(string, "\\");
	if (*read == 0)
	{
		return read - string;
	}
	char* write = string;
	char* last = string;

	for (;;)
	{
		// Copy characters since last escaped sequence
		assetdoc_size written = read - last;
		memmove(write, last, written);
		write += written;

		if (*read++ == 0)
		{
			break;
		}

		// jsmn already checked that all escape sequences are valid
		switch (*read++)
		{
		case '\"': *write++ = '\"'; break;
		case '/':  *write++ = '/';  break;
		case '\\': *write++ = '\\'; break;
		case 'b':  *write++ = '\b'; break;
		case 'f':  *write++ = '\f'; break;
		case 'r':  *write++ = '\r'; break;
		case 'n':  *write++ = '\n'; break;
		case 't':  *write++ = '\t'; break;
		case 'u':
		{
			// UCS-2 codepoint \uXXXX to UTF-8
			int character = 0;
			for (assetdoc_size i = 0; i < 4; ++i)
			{
				character = (character << 4) + assetdoc_unhex(*read++);
			}

			if (character <= 0x7F)
			{
				*write++ = character & 0xFF;
			}
			else if (character <= 0x7FF)
			{
				*write++ = 0xC0 | ((character >> 6) & 0xFF);
				*write++ = 0x80 | (character & 0x3F);
			}
			else
			{
				*write++ = 0xE0 | ((character >> 12) & 0xFF);
				*write++ = 0x80 | ((character >> 6) & 0x3F);
				*write++ = 0x80 | (character & 0x3F);
			}
			break;
		}
		default:
			break;
		}

		last = read;
		read += strcspn(read, "\\");
	}

	*write = 0;
	return write - string;
}

assetdoc_size assetdoc_decode_uri(char* uri)
{
	char* write = uri;
	char* i = uri;

	while (*i)
	{
		if (*i == '%')
		{
			int ch1 = assetdoc_unhex(i[1]);

			if (ch1 >= 0)
			{
				int ch2 = assetdoc_unhex(i[2]);

				if (ch2 >= 0)
				{
					*write++ = (char)(ch1 * 16 + ch2);
					i += 3;
					continue;
				}
			}
		}

		*write++ = *i++;
	}

	*write = 0;
	return write - uri;
}

assetdoc_result assetdoc_load_buffers(const assetdoc_options* options, assetdoc_data* data, const char* scene_doc_path)
{
	if (options == NULL)
	{
		return assetdoc_result_invalid_options;
	}

	if (data->buffers_count && data->buffers[0].data == NULL && data->buffers[0].uri == NULL && data->bin)
	{
		if (data->bin_size < data->buffers[0].size)
		{
			return assetdoc_result_data_too_short;
		}

		data->buffers[0].data = (void*)data->bin;
		data->buffers[0].data_free_method = assetdoc_data_free_method_none;
	}

	for (assetdoc_size i = 0; i < data->buffers_count; ++i)
	{
		if (data->buffers[i].data)
		{
			continue;
		}

		const char* uri = data->buffers[i].uri;

		if (uri == NULL)
		{
			continue;
		}

		if (strncmp(uri, "data:", 5) == 0)
		{
			const char* comma = strchr(uri, ',');

			if (comma && comma - uri >= 7 && strncmp(comma - 7, ";base64", 7) == 0)
			{
				assetdoc_result res = assetdoc_load_buffer_base64(options, data->buffers[i].size, comma + 1, &data->buffers[i].data);
				data->buffers[i].data_free_method = assetdoc_data_free_method_memory_free;

				if (res != assetdoc_result_success)
				{
					return res;
				}
			}
			else
			{
				return assetdoc_result_unknown_format;
			}
		}
		else if (strstr(uri, "://") == NULL && scene_doc_path)
		{
			assetdoc_result res = assetdoc_load_buffer_file(options, data->buffers[i].size, uri, scene_doc_path, &data->buffers[i].data);
			data->buffers[i].data_free_method = assetdoc_data_free_method_file_release;

			if (res != assetdoc_result_success)
			{
				return res;
			}
		}
		else
		{
			return assetdoc_result_unknown_format;
		}
	}

	return assetdoc_result_success;
}

static assetdoc_size assetdoc_calc_index_bound(assetdoc_buffer_view* buffer_view, assetdoc_size offset, assetdoc_component_type component_type, assetdoc_size count)
{
	char* data = (char*)buffer_view->buffer->data + offset + buffer_view->offset;
	assetdoc_size bound = 0;

	switch (component_type)
	{
	case assetdoc_component_type_r_8u:
		for (size_t i = 0; i < count; ++i)
		{
			assetdoc_size v = ((unsigned char*)data)[i];
			bound = bound > v ? bound : v;
		}
		break;

	case assetdoc_component_type_r_16u:
		for (size_t i = 0; i < count; ++i)
		{
			assetdoc_size v = ((unsigned short*)data)[i];
			bound = bound > v ? bound : v;
		}
		break;

	case assetdoc_component_type_r_32u:
		for (size_t i = 0; i < count; ++i)
		{
			assetdoc_size v = ((unsigned int*)data)[i];
			bound = bound > v ? bound : v;
		}
		break;

	default:
		;
	}

	return bound;
}

#if ASSETDOC_VALIDATE_ENABLE_ASSERTS
#define ASSETDOC_ASSERT_IF(cond, result) assert(!(cond)); if (cond) return result;
#else
#define ASSETDOC_ASSERT_IF(cond, result) if (cond) return result;
#endif

assetdoc_result assetdoc_validate(assetdoc_data* data)
{
	for (assetdoc_size i = 0; i < data->accessors_count; ++i)
	{
		assetdoc_accessor* accessor = &data->accessors[i];

		ASSETDOC_ASSERT_IF(data->accessors[i].component_type == assetdoc_component_type_invalid, assetdoc_result_invalid_scene_doc);
		ASSETDOC_ASSERT_IF(data->accessors[i].type == assetdoc_type_invalid, assetdoc_result_invalid_scene_doc);

		assetdoc_size element_size = assetdoc_calc_size(accessor->type, accessor->component_type);

		if (accessor->buffer_view)
		{
			assetdoc_size req_size = accessor->offset + accessor->stride * (accessor->count - 1) + element_size;

			ASSETDOC_ASSERT_IF(accessor->buffer_view->size < req_size, assetdoc_result_data_too_short);
		}

		if (accessor->is_sparse)
		{
			assetdoc_accessor_sparse* sparse = &accessor->sparse;

			assetdoc_size indices_component_size = assetdoc_component_size(sparse->indices_component_type);
			assetdoc_size indices_req_size = sparse->indices_byte_offset + indices_component_size * sparse->count;
			assetdoc_size values_req_size = sparse->values_byte_offset + element_size * sparse->count;

			ASSETDOC_ASSERT_IF(sparse->indices_buffer_view->size < indices_req_size ||
							sparse->values_buffer_view->size < values_req_size, assetdoc_result_data_too_short);

			ASSETDOC_ASSERT_IF(sparse->indices_component_type != assetdoc_component_type_r_8u &&
							sparse->indices_component_type != assetdoc_component_type_r_16u &&
							sparse->indices_component_type != assetdoc_component_type_r_32u, assetdoc_result_invalid_scene_doc);

			if (sparse->indices_buffer_view->buffer->data)
			{
				assetdoc_size index_bound = assetdoc_calc_index_bound(sparse->indices_buffer_view, sparse->indices_byte_offset, sparse->indices_component_type, sparse->count);

				ASSETDOC_ASSERT_IF(index_bound >= accessor->count, assetdoc_result_data_too_short);
			}
		}
	}

	for (assetdoc_size i = 0; i < data->buffer_views_count; ++i)
	{
		assetdoc_size req_size = data->buffer_views[i].offset + data->buffer_views[i].size;

		ASSETDOC_ASSERT_IF(data->buffer_views[i].buffer && data->buffer_views[i].buffer->size < req_size, assetdoc_result_data_too_short);

		if (data->buffer_views[i].has_meshwork_compression)
		{
			assetdoc_meshwork_compression* mc = &data->buffer_views[i].meshwork_compression;

			ASSETDOC_ASSERT_IF(mc->buffer == NULL || mc->buffer->size < mc->offset + mc->size, assetdoc_result_data_too_short);

			ASSETDOC_ASSERT_IF(data->buffer_views[i].stride && mc->stride != data->buffer_views[i].stride, assetdoc_result_invalid_scene_doc);

			ASSETDOC_ASSERT_IF(data->buffer_views[i].size != mc->stride * mc->count, assetdoc_result_invalid_scene_doc);

			ASSETDOC_ASSERT_IF(mc->mode == assetdoc_meshwork_compression_mode_invalid, assetdoc_result_invalid_scene_doc);

			ASSETDOC_ASSERT_IF(mc->mode == assetdoc_meshwork_compression_mode_attributes && !(mc->stride % 4 == 0 && mc->stride <= 256), assetdoc_result_invalid_scene_doc);

			ASSETDOC_ASSERT_IF(mc->mode == assetdoc_meshwork_compression_mode_triangles && mc->count % 3 != 0, assetdoc_result_invalid_scene_doc);

			ASSETDOC_ASSERT_IF((mc->mode == assetdoc_meshwork_compression_mode_triangles || mc->mode == assetdoc_meshwork_compression_mode_indices) && mc->stride != 2 && mc->stride != 4, assetdoc_result_invalid_scene_doc);

			ASSETDOC_ASSERT_IF((mc->mode == assetdoc_meshwork_compression_mode_triangles || mc->mode == assetdoc_meshwork_compression_mode_indices) && mc->filter != assetdoc_meshwork_compression_filter_none, assetdoc_result_invalid_scene_doc);

			ASSETDOC_ASSERT_IF(mc->filter == assetdoc_meshwork_compression_filter_octahedral && mc->stride != 4 && mc->stride != 8, assetdoc_result_invalid_scene_doc);
			ASSETDOC_ASSERT_IF(mc->filter == assetdoc_meshwork_compression_filter_quaternion && mc->stride != 8, assetdoc_result_invalid_scene_doc);
			ASSETDOC_ASSERT_IF(mc->filter == assetdoc_meshwork_compression_filter_color && mc->stride != 4 && mc->stride != 8, assetdoc_result_invalid_scene_doc);
		}
	}

	for (assetdoc_size i = 0; i < data->meshes_count; ++i)
	{
		if (data->meshes[i].weights)
		{
			ASSETDOC_ASSERT_IF(data->meshes[i].primitives_count && data->meshes[i].primitives[0].targets_count != data->meshes[i].weights_count, assetdoc_result_invalid_scene_doc);
		}

		if (data->meshes[i].target_names)
		{
			ASSETDOC_ASSERT_IF(data->meshes[i].primitives_count && data->meshes[i].primitives[0].targets_count != data->meshes[i].target_names_count, assetdoc_result_invalid_scene_doc);
		}

		for (assetdoc_size j = 0; j < data->meshes[i].primitives_count; ++j)
		{
			ASSETDOC_ASSERT_IF(data->meshes[i].primitives[j].type == assetdoc_primitive_type_invalid, assetdoc_result_invalid_scene_doc);
			ASSETDOC_ASSERT_IF(data->meshes[i].primitives[j].targets_count != data->meshes[i].primitives[0].targets_count, assetdoc_result_invalid_scene_doc);

			ASSETDOC_ASSERT_IF(data->meshes[i].primitives[j].attributes_count == 0, assetdoc_result_invalid_scene_doc);

			assetdoc_accessor* first = data->meshes[i].primitives[j].attributes[0].data;

			ASSETDOC_ASSERT_IF(first->count == 0, assetdoc_result_invalid_scene_doc);

			for (assetdoc_size k = 0; k < data->meshes[i].primitives[j].attributes_count; ++k)
			{
				ASSETDOC_ASSERT_IF(data->meshes[i].primitives[j].attributes[k].data->count != first->count, assetdoc_result_invalid_scene_doc);
			}

			for (assetdoc_size k = 0; k < data->meshes[i].primitives[j].targets_count; ++k)
			{
				for (assetdoc_size m = 0; m < data->meshes[i].primitives[j].targets[k].attributes_count; ++m)
				{
					ASSETDOC_ASSERT_IF(data->meshes[i].primitives[j].targets[k].attributes[m].data->count != first->count, assetdoc_result_invalid_scene_doc);
				}
			}

			assetdoc_accessor* indices = data->meshes[i].primitives[j].indices;

			ASSETDOC_ASSERT_IF(indices &&
				indices->component_type != assetdoc_component_type_r_8u &&
				indices->component_type != assetdoc_component_type_r_16u &&
				indices->component_type != assetdoc_component_type_r_32u, assetdoc_result_invalid_scene_doc);

			ASSETDOC_ASSERT_IF(indices && indices->type != assetdoc_type_scalar, assetdoc_result_invalid_scene_doc);
			ASSETDOC_ASSERT_IF(indices && indices->stride != assetdoc_component_size(indices->component_type), assetdoc_result_invalid_scene_doc);

			if (indices && indices->buffer_view && indices->buffer_view->buffer->data)
			{
				assetdoc_size index_bound = assetdoc_calc_index_bound(indices->buffer_view, indices->offset, indices->component_type, indices->count);

				ASSETDOC_ASSERT_IF(index_bound >= first->count, assetdoc_result_data_too_short);
			}

			for (assetdoc_size k = 0; k < data->meshes[i].primitives[j].mappings_count; ++k)
			{
				ASSETDOC_ASSERT_IF(data->meshes[i].primitives[j].mappings[k].variant >= data->variants_count, assetdoc_result_invalid_scene_doc);
			}
		}
	}

	for (assetdoc_size i = 0; i < data->nodes_count; ++i)
	{
		if (data->nodes[i].weights && data->nodes[i].mesh)
		{
			ASSETDOC_ASSERT_IF(data->nodes[i].mesh->primitives_count && data->nodes[i].mesh->primitives[0].targets_count != data->nodes[i].weights_count, assetdoc_result_invalid_scene_doc);
		}

		if (data->nodes[i].has_mesh_gpu_instancing)
		{
			ASSETDOC_ASSERT_IF(data->nodes[i].mesh == NULL, assetdoc_result_invalid_scene_doc);
			ASSETDOC_ASSERT_IF(data->nodes[i].mesh_gpu_instancing.attributes_count == 0, assetdoc_result_invalid_scene_doc);

			assetdoc_accessor* first = data->nodes[i].mesh_gpu_instancing.attributes[0].data;

			for (assetdoc_size k = 0; k < data->nodes[i].mesh_gpu_instancing.attributes_count; ++k)
			{
				ASSETDOC_ASSERT_IF(data->nodes[i].mesh_gpu_instancing.attributes[k].data->count != first->count, assetdoc_result_invalid_scene_doc);
			}
		}
	}

	for (assetdoc_size i = 0; i < data->nodes_count; ++i)
	{
		assetdoc_node* p1 = data->nodes[i].parent;
		assetdoc_node* p2 = p1 ? p1->parent : NULL;

		while (p1 && p2)
		{
			ASSETDOC_ASSERT_IF(p1 == p2, assetdoc_result_invalid_scene_doc);

			p1 = p1->parent;
			p2 = p2->parent ? p2->parent->parent : NULL;
		}
	}

	for (assetdoc_size i = 0; i < data->scenes_count; ++i)
	{
		for (assetdoc_size j = 0; j < data->scenes[i].nodes_count; ++j)
		{
			ASSETDOC_ASSERT_IF(data->scenes[i].nodes[j]->parent, assetdoc_result_invalid_scene_doc);
		}
	}

	for (assetdoc_size i = 0; i < data->animations_count; ++i)
	{
		for (assetdoc_size j = 0; j < data->animations[i].channels_count; ++j)
		{
			assetdoc_animation_channel* channel = &data->animations[i].channels[j];

			if (!channel->target_node)
			{
				continue;
			}

			assetdoc_size components = 1;

			if (channel->target_path == assetdoc_animation_path_type_weights)
			{
				ASSETDOC_ASSERT_IF(!channel->target_node->mesh || !channel->target_node->mesh->primitives_count, assetdoc_result_invalid_scene_doc);

				components = channel->target_node->mesh->primitives[0].targets_count;
			}

			assetdoc_size values = channel->sampler->interpolation == assetdoc_interpolation_type_cubic_spline ? 3 : 1;

			ASSETDOC_ASSERT_IF(channel->sampler->input->count * components * values != channel->sampler->output->count, assetdoc_result_invalid_scene_doc);
		}
	}

	for (assetdoc_size i = 0; i < data->variants_count; ++i)
	{
		ASSETDOC_ASSERT_IF(!data->variants[i].name, assetdoc_result_invalid_scene_doc);
	}

	return assetdoc_result_success;
}

assetdoc_result assetdoc_copy_extras_json(const assetdoc_data* data, const assetdoc_extras* extras, char* dest, assetdoc_size* dest_size)
{
	assetdoc_size json_size = extras->end_offset - extras->start_offset;

	if (!dest)
	{
		if (dest_size)
		{
			*dest_size = json_size + 1;
			return assetdoc_result_success;
		}
		return assetdoc_result_invalid_options;
	}

	if (*dest_size + 1 < json_size)
	{
		strncpy(dest, data->json + extras->start_offset, *dest_size - 1);
		dest[*dest_size - 1] = 0;
	}
	else
	{
		strncpy(dest, data->json + extras->start_offset, json_size);
		dest[json_size] = 0;
	}

	return assetdoc_result_success;
}

static void assetdoc_free_extras(assetdoc_data* data, assetdoc_extras* extras)
{
	data->memory.free_func(data->memory.user_data, extras->data);
}

static void assetdoc_free_extensions(assetdoc_data* data, assetdoc_extension* extensions, assetdoc_size extensions_count)
{
	for (assetdoc_size i = 0; i < extensions_count; ++i)
	{
		data->memory.free_func(data->memory.user_data, extensions[i].name);
		data->memory.free_func(data->memory.user_data, extensions[i].data);
	}
	data->memory.free_func(data->memory.user_data, extensions);
}

void assetdoc_free(assetdoc_data* data)
{
	if (!data)
	{
		return;
	}

	void (*file_release)(const struct assetdoc_memory_options*, const struct assetdoc_file_options*, void* data, assetdoc_size size) = data->file.release ? data->file.release : assetdoc_default_file_release;

	data->memory.free_func(data->memory.user_data, data->asset.copyright);
	data->memory.free_func(data->memory.user_data, data->asset.generator);
	data->memory.free_func(data->memory.user_data, data->asset.version);
	data->memory.free_func(data->memory.user_data, data->asset.min_version);

	assetdoc_free_extensions(data, data->asset.extensions, data->asset.extensions_count);
	assetdoc_free_extras(data, &data->asset.extras);

	for (assetdoc_size i = 0; i < data->accessors_count; ++i)
	{
		data->memory.free_func(data->memory.user_data, data->accessors[i].name);

		assetdoc_free_extensions(data, data->accessors[i].extensions, data->accessors[i].extensions_count);
		assetdoc_free_extras(data, &data->accessors[i].extras);
	}
	data->memory.free_func(data->memory.user_data, data->accessors);

	for (assetdoc_size i = 0; i < data->buffer_views_count; ++i)
	{
		data->memory.free_func(data->memory.user_data, data->buffer_views[i].name);
		data->memory.free_func(data->memory.user_data, data->buffer_views[i].data);

		assetdoc_free_extensions(data, data->buffer_views[i].extensions, data->buffer_views[i].extensions_count);
		assetdoc_free_extras(data, &data->buffer_views[i].extras);
	}
	data->memory.free_func(data->memory.user_data, data->buffer_views);

	for (assetdoc_size i = 0; i < data->buffers_count; ++i)
	{
		data->memory.free_func(data->memory.user_data, data->buffers[i].name);

		if (data->buffers[i].data_free_method == assetdoc_data_free_method_file_release)
		{
			file_release(&data->memory, &data->file, data->buffers[i].data, data->buffers[i].size);
		}
		else if (data->buffers[i].data_free_method == assetdoc_data_free_method_memory_free)
		{
			data->memory.free_func(data->memory.user_data, data->buffers[i].data);
		}

		data->memory.free_func(data->memory.user_data, data->buffers[i].uri);

		assetdoc_free_extensions(data, data->buffers[i].extensions, data->buffers[i].extensions_count);
		assetdoc_free_extras(data, &data->buffers[i].extras);
	}
	data->memory.free_func(data->memory.user_data, data->buffers);

	for (assetdoc_size i = 0; i < data->meshes_count; ++i)
	{
		data->memory.free_func(data->memory.user_data, data->meshes[i].name);

		for (assetdoc_size j = 0; j < data->meshes[i].primitives_count; ++j)
		{
			for (assetdoc_size k = 0; k < data->meshes[i].primitives[j].attributes_count; ++k)
			{
				data->memory.free_func(data->memory.user_data, data->meshes[i].primitives[j].attributes[k].name);
			}

			data->memory.free_func(data->memory.user_data, data->meshes[i].primitives[j].attributes);

			for (assetdoc_size k = 0; k < data->meshes[i].primitives[j].targets_count; ++k)
			{
				for (assetdoc_size m = 0; m < data->meshes[i].primitives[j].targets[k].attributes_count; ++m)
				{
					data->memory.free_func(data->memory.user_data, data->meshes[i].primitives[j].targets[k].attributes[m].name);
				}

				data->memory.free_func(data->memory.user_data, data->meshes[i].primitives[j].targets[k].attributes);
			}

			data->memory.free_func(data->memory.user_data, data->meshes[i].primitives[j].targets);

			if (data->meshes[i].primitives[j].has_draco_mesh_compression)
			{
				for (assetdoc_size k = 0; k < data->meshes[i].primitives[j].draco_mesh_compression.attributes_count; ++k)
				{
					data->memory.free_func(data->memory.user_data, data->meshes[i].primitives[j].draco_mesh_compression.attributes[k].name);
				}

				data->memory.free_func(data->memory.user_data, data->meshes[i].primitives[j].draco_mesh_compression.attributes);
			}

			for (assetdoc_size k = 0; k < data->meshes[i].primitives[j].mappings_count; ++k)
			{
				assetdoc_free_extras(data, &data->meshes[i].primitives[j].mappings[k].extras);
			}

			data->memory.free_func(data->memory.user_data, data->meshes[i].primitives[j].mappings);

			assetdoc_free_extensions(data, data->meshes[i].primitives[j].extensions, data->meshes[i].primitives[j].extensions_count);
			assetdoc_free_extras(data, &data->meshes[i].primitives[j].extras);
		}

		data->memory.free_func(data->memory.user_data, data->meshes[i].primitives);
		data->memory.free_func(data->memory.user_data, data->meshes[i].weights);

		for (assetdoc_size j = 0; j < data->meshes[i].target_names_count; ++j)
		{
			data->memory.free_func(data->memory.user_data, data->meshes[i].target_names[j]);
		}

		assetdoc_free_extensions(data, data->meshes[i].extensions, data->meshes[i].extensions_count);
		assetdoc_free_extras(data, &data->meshes[i].extras);

		data->memory.free_func(data->memory.user_data, data->meshes[i].target_names);
	}

	data->memory.free_func(data->memory.user_data, data->meshes);

	for (assetdoc_size i = 0; i < data->materials_count; ++i)
	{
		data->memory.free_func(data->memory.user_data, data->materials[i].name);

		assetdoc_free_extensions(data, data->materials[i].extensions, data->materials[i].extensions_count);
		assetdoc_free_extras(data, &data->materials[i].extras);
	}

	data->memory.free_func(data->memory.user_data, data->materials);

	for (assetdoc_size i = 0; i < data->images_count; ++i)
	{
		data->memory.free_func(data->memory.user_data, data->images[i].name);
		data->memory.free_func(data->memory.user_data, data->images[i].uri);
		data->memory.free_func(data->memory.user_data, data->images[i].mime_type);

		assetdoc_free_extensions(data, data->images[i].extensions, data->images[i].extensions_count);
		assetdoc_free_extras(data, &data->images[i].extras);
	}

	data->memory.free_func(data->memory.user_data, data->images);

	for (assetdoc_size i = 0; i < data->textures_count; ++i)
	{
		data->memory.free_func(data->memory.user_data, data->textures[i].name);

		assetdoc_free_extensions(data, data->textures[i].extensions, data->textures[i].extensions_count);
		assetdoc_free_extras(data, &data->textures[i].extras);
	}

	data->memory.free_func(data->memory.user_data, data->textures);

	for (assetdoc_size i = 0; i < data->samplers_count; ++i)
	{
		data->memory.free_func(data->memory.user_data, data->samplers[i].name);

		assetdoc_free_extensions(data, data->samplers[i].extensions, data->samplers[i].extensions_count);
		assetdoc_free_extras(data, &data->samplers[i].extras);
	}

	data->memory.free_func(data->memory.user_data, data->samplers);

	for (assetdoc_size i = 0; i < data->skins_count; ++i)
	{
		data->memory.free_func(data->memory.user_data, data->skins[i].name);
		data->memory.free_func(data->memory.user_data, data->skins[i].joints);

		assetdoc_free_extensions(data, data->skins[i].extensions, data->skins[i].extensions_count);
		assetdoc_free_extras(data, &data->skins[i].extras);
	}

	data->memory.free_func(data->memory.user_data, data->skins);

	for (assetdoc_size i = 0; i < data->cameras_count; ++i)
	{
		data->memory.free_func(data->memory.user_data, data->cameras[i].name);

		if (data->cameras[i].type == assetdoc_camera_type_perspective)
		{
			assetdoc_free_extras(data, &data->cameras[i].data.perspective.extras);
		}
		else if (data->cameras[i].type == assetdoc_camera_type_orthographic)
		{
			assetdoc_free_extras(data, &data->cameras[i].data.orthographic.extras);
		}

		assetdoc_free_extensions(data, data->cameras[i].extensions, data->cameras[i].extensions_count);
		assetdoc_free_extras(data, &data->cameras[i].extras);
	}

	data->memory.free_func(data->memory.user_data, data->cameras);

	for (assetdoc_size i = 0; i < data->lights_count; ++i)
	{
		data->memory.free_func(data->memory.user_data, data->lights[i].name);

		assetdoc_free_extras(data, &data->lights[i].extras);
	}

	data->memory.free_func(data->memory.user_data, data->lights);

	for (assetdoc_size i = 0; i < data->nodes_count; ++i)
	{
		data->memory.free_func(data->memory.user_data, data->nodes[i].name);
		data->memory.free_func(data->memory.user_data, data->nodes[i].children);
		data->memory.free_func(data->memory.user_data, data->nodes[i].weights);

		if (data->nodes[i].has_mesh_gpu_instancing)
		{
			for (assetdoc_size j = 0; j < data->nodes[i].mesh_gpu_instancing.attributes_count; ++j)
			{
				data->memory.free_func(data->memory.user_data, data->nodes[i].mesh_gpu_instancing.attributes[j].name);
			}

			data->memory.free_func(data->memory.user_data, data->nodes[i].mesh_gpu_instancing.attributes);
		}

		assetdoc_free_extensions(data, data->nodes[i].extensions, data->nodes[i].extensions_count);
		assetdoc_free_extras(data, &data->nodes[i].extras);
	}

	data->memory.free_func(data->memory.user_data, data->nodes);

	for (assetdoc_size i = 0; i < data->scenes_count; ++i)
	{
		data->memory.free_func(data->memory.user_data, data->scenes[i].name);
		data->memory.free_func(data->memory.user_data, data->scenes[i].nodes);

		assetdoc_free_extensions(data, data->scenes[i].extensions, data->scenes[i].extensions_count);
		assetdoc_free_extras(data, &data->scenes[i].extras);
	}

	data->memory.free_func(data->memory.user_data, data->scenes);

	for (assetdoc_size i = 0; i < data->animations_count; ++i)
	{
		data->memory.free_func(data->memory.user_data, data->animations[i].name);
		for (assetdoc_size j = 0; j <  data->animations[i].samplers_count; ++j)
		{
			assetdoc_free_extensions(data, data->animations[i].samplers[j].extensions, data->animations[i].samplers[j].extensions_count);
			assetdoc_free_extras(data, &data->animations[i].samplers[j].extras);
		}
		data->memory.free_func(data->memory.user_data, data->animations[i].samplers);

		for (assetdoc_size j = 0; j <  data->animations[i].channels_count; ++j)
		{
			assetdoc_free_extensions(data, data->animations[i].channels[j].extensions, data->animations[i].channels[j].extensions_count);
			assetdoc_free_extras(data, &data->animations[i].channels[j].extras);
		}
		data->memory.free_func(data->memory.user_data, data->animations[i].channels);

		assetdoc_free_extensions(data, data->animations[i].extensions, data->animations[i].extensions_count);
		assetdoc_free_extras(data, &data->animations[i].extras);
	}

	data->memory.free_func(data->memory.user_data, data->animations);

	for (assetdoc_size i = 0; i < data->variants_count; ++i)
	{
		data->memory.free_func(data->memory.user_data, data->variants[i].name);

		assetdoc_free_extras(data, &data->variants[i].extras);
	}

	data->memory.free_func(data->memory.user_data, data->variants);

	assetdoc_free_extensions(data, data->data_extensions, data->data_extensions_count);
	assetdoc_free_extras(data, &data->extras);

	for (assetdoc_size i = 0; i < data->extensions_used_count; ++i)
	{
		data->memory.free_func(data->memory.user_data, data->extensions_used[i]);
	}

	data->memory.free_func(data->memory.user_data, data->extensions_used);

	for (assetdoc_size i = 0; i < data->extensions_required_count; ++i)
	{
		data->memory.free_func(data->memory.user_data, data->extensions_required[i]);
	}

	data->memory.free_func(data->memory.user_data, data->extensions_required);

	file_release(&data->memory, &data->file, data->file_data, data->file_size);

	data->memory.free_func(data->memory.user_data, data);
}

void assetdoc_node_transform_local(const assetdoc_node* node, assetdoc_float* out_matrix)
{
	assetdoc_float* lm = out_matrix;

	if (node->has_matrix)
	{
		memcpy(lm, node->matrix, sizeof(float) * 16);
	}
	else
	{
		float tx = node->translation[0];
		float ty = node->translation[1];
		float tz = node->translation[2];

		float qx = node->rotation[0];
		float qy = node->rotation[1];
		float qz = node->rotation[2];
		float qw = node->rotation[3];

		float sx = node->scale[0];
		float sy = node->scale[1];
		float sz = node->scale[2];

		lm[0] = (1 - 2 * qy*qy - 2 * qz*qz) * sx;
		lm[1] = (2 * qx*qy + 2 * qz*qw) * sx;
		lm[2] = (2 * qx*qz - 2 * qy*qw) * sx;
		lm[3] = 0.f;

		lm[4] = (2 * qx*qy - 2 * qz*qw) * sy;
		lm[5] = (1 - 2 * qx*qx - 2 * qz*qz) * sy;
		lm[6] = (2 * qy*qz + 2 * qx*qw) * sy;
		lm[7] = 0.f;

		lm[8] = (2 * qx*qz + 2 * qy*qw) * sz;
		lm[9] = (2 * qy*qz - 2 * qx*qw) * sz;
		lm[10] = (1 - 2 * qx*qx - 2 * qy*qy) * sz;
		lm[11] = 0.f;

		lm[12] = tx;
		lm[13] = ty;
		lm[14] = tz;
		lm[15] = 1.f;
	}
}

void assetdoc_node_transform_world(const assetdoc_node* node, assetdoc_float* out_matrix)
{
	assetdoc_float* lm = out_matrix;
	assetdoc_node_transform_local(node, lm);

	const assetdoc_node* parent = node->parent;

	while (parent)
	{
		float pm[16];
		assetdoc_node_transform_local(parent, pm);

		for (int i = 0; i < 4; ++i)
		{
			float l0 = lm[i * 4 + 0];
			float l1 = lm[i * 4 + 1];
			float l2 = lm[i * 4 + 2];

			float r0 = l0 * pm[0] + l1 * pm[4] + l2 * pm[8];
			float r1 = l0 * pm[1] + l1 * pm[5] + l2 * pm[9];
			float r2 = l0 * pm[2] + l1 * pm[6] + l2 * pm[10];

			lm[i * 4 + 0] = r0;
			lm[i * 4 + 1] = r1;
			lm[i * 4 + 2] = r2;
		}

		lm[12] += pm[12];
		lm[13] += pm[13];
		lm[14] += pm[14];

		parent = parent->parent;
	}
}

static assetdoc_ssize assetdoc_component_read_integer(const void* in, assetdoc_component_type component_type)
{
	switch (component_type)
	{
		case assetdoc_component_type_r_16:
			return *((const int16_t*) in);
		case assetdoc_component_type_r_16u:
			return *((const uint16_t*) in);
		case assetdoc_component_type_r_32u:
			return *((const uint32_t*) in);
		case assetdoc_component_type_r_8:
			return *((const int8_t*) in);
		case assetdoc_component_type_r_8u:
			return *((const uint8_t*) in);
		default:
			return 0;
	}
}

static assetdoc_size assetdoc_component_read_index(const void* in, assetdoc_component_type component_type)
{
	switch (component_type)
	{
		case assetdoc_component_type_r_16u:
			return *((const uint16_t*) in);
		case assetdoc_component_type_r_32u:
			return *((const uint32_t*) in);
		case assetdoc_component_type_r_8u:
			return *((const uint8_t*) in);
		default:
			return 0;
	}
}

static assetdoc_float assetdoc_component_read_float(const void* in, assetdoc_component_type component_type, assetdoc_bool normalized)
{
	if (component_type == assetdoc_component_type_r_32f)
	{
		return *((const float*) in);
	}

	if (normalized)
	{
		switch (component_type)
		{
			// note: SceneDoc spec doesn't currently define normalized conversions for 32-bit integers
			case assetdoc_component_type_r_16:
				return *((const int16_t*) in) / (assetdoc_float)32767;
			case assetdoc_component_type_r_16u:
				return *((const uint16_t*) in) / (assetdoc_float)65535;
			case assetdoc_component_type_r_8:
				return *((const int8_t*) in) / (assetdoc_float)127;
			case assetdoc_component_type_r_8u:
				return *((const uint8_t*) in) / (assetdoc_float)255;
			default:
				return 0;
		}
	}

	return (assetdoc_float)assetdoc_component_read_integer(in, component_type);
}

static assetdoc_bool assetdoc_element_read_float(const uint8_t* element, assetdoc_type type, assetdoc_component_type component_type, assetdoc_bool normalized, assetdoc_float* out, assetdoc_size element_size)
{
	assetdoc_size num_components = assetdoc_num_components(type);

	if (element_size < num_components) {
		return 0;
	}

	// There are three special cases for component extraction, see #data-alignment in the 2.0 spec.

	assetdoc_size component_size = assetdoc_component_size(component_type);

	if (type == assetdoc_type_mat2 && component_size == 1)
	{
		out[0] = assetdoc_component_read_float(element, component_type, normalized);
		out[1] = assetdoc_component_read_float(element + 1, component_type, normalized);
		out[2] = assetdoc_component_read_float(element + 4, component_type, normalized);
		out[3] = assetdoc_component_read_float(element + 5, component_type, normalized);
		return 1;
	}

	if (type == assetdoc_type_mat3 && component_size == 1)
	{
		out[0] = assetdoc_component_read_float(element, component_type, normalized);
		out[1] = assetdoc_component_read_float(element + 1, component_type, normalized);
		out[2] = assetdoc_component_read_float(element + 2, component_type, normalized);
		out[3] = assetdoc_component_read_float(element + 4, component_type, normalized);
		out[4] = assetdoc_component_read_float(element + 5, component_type, normalized);
		out[5] = assetdoc_component_read_float(element + 6, component_type, normalized);
		out[6] = assetdoc_component_read_float(element + 8, component_type, normalized);
		out[7] = assetdoc_component_read_float(element + 9, component_type, normalized);
		out[8] = assetdoc_component_read_float(element + 10, component_type, normalized);
		return 1;
	}

	if (type == assetdoc_type_mat3 && component_size == 2)
	{
		out[0] = assetdoc_component_read_float(element, component_type, normalized);
		out[1] = assetdoc_component_read_float(element + 2, component_type, normalized);
		out[2] = assetdoc_component_read_float(element + 4, component_type, normalized);
		out[3] = assetdoc_component_read_float(element + 8, component_type, normalized);
		out[4] = assetdoc_component_read_float(element + 10, component_type, normalized);
		out[5] = assetdoc_component_read_float(element + 12, component_type, normalized);
		out[6] = assetdoc_component_read_float(element + 16, component_type, normalized);
		out[7] = assetdoc_component_read_float(element + 18, component_type, normalized);
		out[8] = assetdoc_component_read_float(element + 20, component_type, normalized);
		return 1;
	}

	for (assetdoc_size i = 0; i < num_components; ++i)
	{
		out[i] = assetdoc_component_read_float(element + component_size * i, component_type, normalized);
	}
	return 1;
}

const uint8_t* assetdoc_buffer_view_data(const assetdoc_buffer_view* view)
{
	if (view->data)
		return (const uint8_t*)view->data;

	if (!view->buffer->data)
		return NULL;

	const uint8_t* result = (const uint8_t*)view->buffer->data;
	result += view->offset;
	return result;
}

const assetdoc_accessor* assetdoc_find_accessor(const assetdoc_primitive* prim, assetdoc_attribute_type type, assetdoc_int index)
{
	for (assetdoc_size i = 0; i < prim->attributes_count; ++i)
	{
		const assetdoc_attribute* attr = &prim->attributes[i];
		if (attr->type == type && attr->index == index)
			return attr->data;
	}

	return NULL;
}

static const uint8_t* assetdoc_find_sparse_index(const assetdoc_accessor* accessor, assetdoc_size needle)
{
	const assetdoc_accessor_sparse* sparse = &accessor->sparse;
	const uint8_t* index_data = assetdoc_buffer_view_data(sparse->indices_buffer_view);
	const uint8_t* value_data = assetdoc_buffer_view_data(sparse->values_buffer_view);

	if (index_data == NULL || value_data == NULL)
		return NULL;

	index_data += sparse->indices_byte_offset;
	value_data += sparse->values_byte_offset;

	assetdoc_size index_stride = assetdoc_component_size(sparse->indices_component_type);

	assetdoc_size offset = 0;
	assetdoc_size length = sparse->count;

	while (length)
	{
		assetdoc_size rem = length % 2;
		length /= 2;

		assetdoc_size index = assetdoc_component_read_index(index_data + (offset + length) * index_stride, sparse->indices_component_type);
		offset += index < needle ? length + rem : 0;
	}

	if (offset == sparse->count)
		return NULL;

	assetdoc_size index = assetdoc_component_read_index(index_data + offset * index_stride, sparse->indices_component_type);
	return index == needle ? value_data + offset * accessor->stride : NULL;
}

assetdoc_bool assetdoc_accessor_read_float(const assetdoc_accessor* accessor, assetdoc_size index, assetdoc_float* out, assetdoc_size element_size)
{
	if (accessor->is_sparse)
	{
		const uint8_t* element = assetdoc_find_sparse_index(accessor, index);
		if (element)
			return assetdoc_element_read_float(element, accessor->type, accessor->component_type, accessor->normalized, out, element_size);
	}
	if (accessor->buffer_view == NULL)
	{
		memset(out, 0, element_size * sizeof(assetdoc_float));
		return 1;
	}
	const uint8_t* element = assetdoc_buffer_view_data(accessor->buffer_view);
	if (element == NULL)
	{
		return 0;
	}
	element += accessor->offset + accessor->stride * index;
	return assetdoc_element_read_float(element, accessor->type, accessor->component_type, accessor->normalized, out, element_size);
}

assetdoc_size assetdoc_accessor_unpack_floats(const assetdoc_accessor* accessor, assetdoc_float* out, assetdoc_size float_count)
{
	assetdoc_size floats_per_element = assetdoc_num_components(accessor->type);
	assetdoc_size available_floats = accessor->count * floats_per_element;
	if (out == NULL)
	{
		return available_floats;
	}

	float_count = available_floats < float_count ? available_floats : float_count;
	assetdoc_size element_count = float_count / floats_per_element;

	// First pass: convert each element in the base accessor.
	if (accessor->buffer_view == NULL)
	{
		memset(out, 0, element_count * floats_per_element * sizeof(assetdoc_float));
	}
	else
	{
		const uint8_t* element = assetdoc_buffer_view_data(accessor->buffer_view);
		if (element == NULL)
		{
			return 0;
		}
		element += accessor->offset;

		if (accessor->component_type == assetdoc_component_type_r_32f && accessor->stride == floats_per_element * sizeof(assetdoc_float))
		{
			memcpy(out, element, element_count * floats_per_element * sizeof(assetdoc_float));
		}
		else
		{
			assetdoc_float* dest = out;

			for (assetdoc_size index = 0; index < element_count; index++, dest += floats_per_element, element += accessor->stride)
			{
				if (!assetdoc_element_read_float(element, accessor->type, accessor->component_type, accessor->normalized, dest, floats_per_element))
				{
					return 0;
				}
			}
		}
	}

	// Second pass: write out each element in the sparse accessor.
	if (accessor->is_sparse)
	{
		const assetdoc_accessor_sparse* sparse = &accessor->sparse;

		const uint8_t* index_data = assetdoc_buffer_view_data(sparse->indices_buffer_view);
		const uint8_t* reader_head = assetdoc_buffer_view_data(sparse->values_buffer_view);

		if (index_data == NULL || reader_head == NULL)
		{
			return 0;
		}

		index_data += sparse->indices_byte_offset;
		reader_head += sparse->values_byte_offset;

		assetdoc_size index_stride = assetdoc_component_size(sparse->indices_component_type);
		for (assetdoc_size reader_index = 0; reader_index < sparse->count; reader_index++, index_data += index_stride, reader_head += accessor->stride)
		{
			size_t writer_index = assetdoc_component_read_index(index_data, sparse->indices_component_type);
			float* writer_head = out + writer_index * floats_per_element;

			if (!assetdoc_element_read_float(reader_head, accessor->type, accessor->component_type, accessor->normalized, writer_head, floats_per_element))
			{
				return 0;
			}
		}
	}

	return element_count * floats_per_element;
}

static assetdoc_uint assetdoc_component_read_uint(const void* in, assetdoc_component_type component_type)
{
	switch (component_type)
	{
		case assetdoc_component_type_r_8:
			return *((const int8_t*) in);

		case assetdoc_component_type_r_8u:
			return *((const uint8_t*) in);

		case assetdoc_component_type_r_16:
			return *((const int16_t*) in);

		case assetdoc_component_type_r_16u:
			return *((const uint16_t*) in);

		case assetdoc_component_type_r_32u:
			return *((const uint32_t*) in);

		default:
			return 0;
	}
}

static assetdoc_bool assetdoc_element_read_uint(const uint8_t* element, assetdoc_type type, assetdoc_component_type component_type, assetdoc_uint* out, assetdoc_size element_size)
{
	assetdoc_size num_components = assetdoc_num_components(type);

	if (element_size < num_components)
	{
		return 0;
	}

	// Reading integer matrices is not a valid use case
	if (type == assetdoc_type_mat2 || type == assetdoc_type_mat3 || type == assetdoc_type_mat4)
	{
		return 0;
	}

	assetdoc_size component_size = assetdoc_component_size(component_type);

	for (assetdoc_size i = 0; i < num_components; ++i)
	{
		out[i] = assetdoc_component_read_uint(element + component_size * i, component_type);
	}
	return 1;
}

assetdoc_bool assetdoc_accessor_read_uint(const assetdoc_accessor* accessor, assetdoc_size index, assetdoc_uint* out, assetdoc_size element_size)
{
	if (accessor->is_sparse)
	{
		const uint8_t* element = assetdoc_find_sparse_index(accessor, index);
		if (element)
			return assetdoc_element_read_uint(element, accessor->type, accessor->component_type, out, element_size);
	}
	if (accessor->buffer_view == NULL)
	{
		memset(out, 0, element_size * sizeof(assetdoc_uint));
		return 1;
	}
	const uint8_t* element = assetdoc_buffer_view_data(accessor->buffer_view);
	if (element == NULL)
	{
		return 0;
	}
	element += accessor->offset + accessor->stride * index;
	return assetdoc_element_read_uint(element, accessor->type, accessor->component_type, out, element_size);
}

assetdoc_size assetdoc_accessor_read_index(const assetdoc_accessor* accessor, assetdoc_size index)
{
	if (accessor->is_sparse)
	{
		const uint8_t* element = assetdoc_find_sparse_index(accessor, index);
		if (element)
			return assetdoc_component_read_index(element, accessor->component_type);
	}
	if (accessor->buffer_view == NULL)
	{
		return 0;
	}
	const uint8_t* element = assetdoc_buffer_view_data(accessor->buffer_view);
	if (element == NULL)
	{
		return 0; // This is an error case, but we can't communicate the error with existing interface.
	}
	element += accessor->offset + accessor->stride * index;
	return assetdoc_component_read_index(element, accessor->component_type);
}

assetdoc_size assetdoc_mesh_index(const assetdoc_data* data, const assetdoc_mesh* object)
{
	assert(object && (assetdoc_size)(object - data->meshes) < data->meshes_count);
	return (assetdoc_size)(object - data->meshes);
}

assetdoc_size assetdoc_material_index(const assetdoc_data* data, const assetdoc_material* object)
{
	assert(object && (assetdoc_size)(object - data->materials) < data->materials_count);
	return (assetdoc_size)(object - data->materials);
}

assetdoc_size assetdoc_accessor_index(const assetdoc_data* data, const assetdoc_accessor* object)
{
	assert(object && (assetdoc_size)(object - data->accessors) < data->accessors_count);
	return (assetdoc_size)(object - data->accessors);
}

assetdoc_size assetdoc_buffer_view_index(const assetdoc_data* data, const assetdoc_buffer_view* object)
{
	assert(object && (assetdoc_size)(object - data->buffer_views) < data->buffer_views_count);
	return (assetdoc_size)(object - data->buffer_views);
}

assetdoc_size assetdoc_buffer_index(const assetdoc_data* data, const assetdoc_buffer* object)
{
	assert(object && (assetdoc_size)(object - data->buffers) < data->buffers_count);
	return (assetdoc_size)(object - data->buffers);
}

assetdoc_size assetdoc_image_index(const assetdoc_data* data, const assetdoc_image* object)
{
	assert(object && (assetdoc_size)(object - data->images) < data->images_count);
	return (assetdoc_size)(object - data->images);
}

assetdoc_size assetdoc_texture_index(const assetdoc_data* data, const assetdoc_texture* object)
{
	assert(object && (assetdoc_size)(object - data->textures) < data->textures_count);
	return (assetdoc_size)(object - data->textures);
}

assetdoc_size assetdoc_sampler_index(const assetdoc_data* data, const assetdoc_sampler* object)
{
	assert(object && (assetdoc_size)(object - data->samplers) < data->samplers_count);
	return (assetdoc_size)(object - data->samplers);
}

assetdoc_size assetdoc_skin_index(const assetdoc_data* data, const assetdoc_skin* object)
{
	assert(object && (assetdoc_size)(object - data->skins) < data->skins_count);
	return (assetdoc_size)(object - data->skins);
}

assetdoc_size assetdoc_camera_index(const assetdoc_data* data, const assetdoc_camera* object)
{
	assert(object && (assetdoc_size)(object - data->cameras) < data->cameras_count);
	return (assetdoc_size)(object - data->cameras);
}

assetdoc_size assetdoc_light_index(const assetdoc_data* data, const assetdoc_light* object)
{
	assert(object && (assetdoc_size)(object - data->lights) < data->lights_count);
	return (assetdoc_size)(object - data->lights);
}

assetdoc_size assetdoc_node_index(const assetdoc_data* data, const assetdoc_node* object)
{
	assert(object && (assetdoc_size)(object - data->nodes) < data->nodes_count);
	return (assetdoc_size)(object - data->nodes);
}

assetdoc_size assetdoc_scene_index(const assetdoc_data* data, const assetdoc_scene* object)
{
	assert(object && (assetdoc_size)(object - data->scenes) < data->scenes_count);
	return (assetdoc_size)(object - data->scenes);
}

assetdoc_size assetdoc_animation_index(const assetdoc_data* data, const assetdoc_animation* object)
{
	assert(object && (assetdoc_size)(object - data->animations) < data->animations_count);
	return (assetdoc_size)(object - data->animations);
}

assetdoc_size assetdoc_animation_sampler_index(const assetdoc_animation* animation, const assetdoc_animation_sampler* object)
{
	assert(object && (assetdoc_size)(object - animation->samplers) < animation->samplers_count);
	return (assetdoc_size)(object - animation->samplers);
}

assetdoc_size assetdoc_animation_channel_index(const assetdoc_animation* animation, const assetdoc_animation_channel* object)
{
	assert(object && (assetdoc_size)(object - animation->channels) < animation->channels_count);
	return (assetdoc_size)(object - animation->channels);
}

assetdoc_size assetdoc_accessor_unpack_indices(const assetdoc_accessor* accessor, void* out, assetdoc_size out_component_size, assetdoc_size index_count)
{
	if (out == NULL)
	{
		return accessor->count;
	}

	assetdoc_size numbers_per_element = assetdoc_num_components(accessor->type);
	assetdoc_size available_numbers = accessor->count * numbers_per_element;

	index_count = available_numbers < index_count ? available_numbers : index_count;
	assetdoc_size index_component_size = assetdoc_component_size(accessor->component_type);

	if (accessor->is_sparse)
	{
		return 0;
	}
	if (accessor->buffer_view == NULL)
	{
		return 0;
	}
	if (index_component_size > out_component_size)
	{
		return 0;
	}
	const uint8_t* element = assetdoc_buffer_view_data(accessor->buffer_view);
	if (element == NULL)
	{
		return 0;
	}
	element += accessor->offset;

	if (index_component_size == out_component_size && accessor->stride == out_component_size * numbers_per_element)
	{
		memcpy(out, element, index_count * index_component_size);
		return index_count;
	}

	// Data couldn't be copied with memcpy due to stride being larger than the component size.
	// OR
	// The component size of the output array is larger than the component size of the index data, so index data will be padded.
	switch (out_component_size)
	{
	case 1:
		for (assetdoc_size index = 0; index < index_count; index++, element += accessor->stride)
		{
			((uint8_t*)out)[index] = (uint8_t)assetdoc_component_read_index(element, accessor->component_type);
		}
		break;
	case 2:
		for (assetdoc_size index = 0; index < index_count; index++, element += accessor->stride)
		{
			((uint16_t*)out)[index] = (uint16_t)assetdoc_component_read_index(element, accessor->component_type);
		}
		break;
	case 4:
		for (assetdoc_size index = 0; index < index_count; index++, element += accessor->stride)
		{
			((uint32_t*)out)[index] = (uint32_t)assetdoc_component_read_index(element, accessor->component_type);
		}
		break;
	default:
		return 0;
	}

	return index_count;
}

#define ASSETDOC_ERROR_JSON -1
#define ASSETDOC_ERROR_NOMEM -2
#define ASSETDOC_ERROR_LEGACY -3

#define ASSETDOC_CHECK_TOKTYPE(tok_, type_) if ((tok_).type != (type_)) { return ASSETDOC_ERROR_JSON; }
#define ASSETDOC_CHECK_TOKTYPE_RET(tok_, type_, ret_) if ((tok_).type != (type_)) { return ret_; }
#define ASSETDOC_CHECK_KEY(tok_) if ((tok_).type != JSMN_STRING || (tok_).size == 0) { return ASSETDOC_ERROR_JSON; } /* checking size for 0 verifies that a value follows the key */

#define ASSETDOC_PTRINDEX(type, idx) (type*)((assetdoc_size)idx + 1)
#define ASSETDOC_PTRFIXUP(var, data, size) if (var) { if ((assetdoc_size)var > size) { return ASSETDOC_ERROR_JSON; } var = &data[(assetdoc_size)var-1]; }
#define ASSETDOC_PTRFIXUP_REQ(var, data, size) if (!var || (assetdoc_size)var > size) { return ASSETDOC_ERROR_JSON; } var = &data[(assetdoc_size)var-1];

static int assetdoc_json_strcmp(jsmntok_t const* tok, const uint8_t* json_chunk, const char* str)
{
	ASSETDOC_CHECK_TOKTYPE(*tok, JSMN_STRING);
	size_t const str_len = strlen(str);
	size_t const name_length = (size_t)(tok->end - tok->start);
	return (str_len == name_length) ? strncmp((const char*)json_chunk + tok->start, str, str_len) : 128;
}

static int assetdoc_json_to_int(jsmntok_t const* tok, const uint8_t* json_chunk)
{
	ASSETDOC_CHECK_TOKTYPE(*tok, JSMN_PRIMITIVE);
	char tmp[128];
	int size = (size_t)(tok->end - tok->start) < sizeof(tmp) ? (int)(tok->end - tok->start) : (int)(sizeof(tmp) - 1);
	strncpy(tmp, (const char*)json_chunk + tok->start, size);
	tmp[size] = 0;
	return ASSETDOC_ATOI(tmp);
}

static assetdoc_size assetdoc_json_to_size(jsmntok_t const* tok, const uint8_t* json_chunk)
{
	ASSETDOC_CHECK_TOKTYPE_RET(*tok, JSMN_PRIMITIVE, 0);
	char tmp[128];
	int size = (size_t)(tok->end - tok->start) < sizeof(tmp) ? (int)(tok->end - tok->start) : (int)(sizeof(tmp) - 1);
	strncpy(tmp, (const char*)json_chunk + tok->start, size);
	tmp[size] = 0;
	long long res = ASSETDOC_ATOLL(tmp);
	return res < 0 ? 0 : (assetdoc_size)res;
}

static assetdoc_float assetdoc_json_to_float(jsmntok_t const* tok, const uint8_t* json_chunk)
{
	ASSETDOC_CHECK_TOKTYPE(*tok, JSMN_PRIMITIVE);
	char tmp[128];
	int size = (size_t)(tok->end - tok->start) < sizeof(tmp) ? (int)(tok->end - tok->start) : (int)(sizeof(tmp) - 1);
	strncpy(tmp, (const char*)json_chunk + tok->start, size);
	tmp[size] = 0;
	return (assetdoc_float)ASSETDOC_ATOF(tmp);
}

static assetdoc_bool assetdoc_json_to_bool(jsmntok_t const* tok, const uint8_t* json_chunk)
{
	int size = (int)(tok->end - tok->start);
	return size == 4 && memcmp(json_chunk + tok->start, "true", 4) == 0;
}

static int assetdoc_skip_json(jsmntok_t const* tokens, int i)
{
	int end = i + 1;

	while (i < end)
	{
		switch (tokens[i].type)
		{
		case JSMN_OBJECT:
			end += tokens[i].size * 2;
			break;

		case JSMN_ARRAY:
			end += tokens[i].size;
			break;

		case JSMN_PRIMITIVE:
		case JSMN_STRING:
			break;

		default:
			return -1;
		}

		i++;
	}

	return i;
}

static void assetdoc_fill_float_array(float* out_array, int size, float value)
{
	for (int j = 0; j < size; ++j)
	{
		out_array[j] = value;
	}
}

static int assetdoc_parse_json_float_array(jsmntok_t const* tokens, int i, const uint8_t* json_chunk, float* out_array, int size)
{
	ASSETDOC_CHECK_TOKTYPE(tokens[i], JSMN_ARRAY);
	if (tokens[i].size != size)
	{
		return ASSETDOC_ERROR_JSON;
	}
	++i;
	for (int j = 0; j < size; ++j)
	{
		ASSETDOC_CHECK_TOKTYPE(tokens[i], JSMN_PRIMITIVE);
		out_array[j] = assetdoc_json_to_float(tokens + i, json_chunk);
		++i;
	}
	return i;
}

static int assetdoc_parse_json_string(assetdoc_options* options, jsmntok_t const* tokens, int i, const uint8_t* json_chunk, char** out_string)
{
	ASSETDOC_CHECK_TOKTYPE(tokens[i], JSMN_STRING);
	if (*out_string)
	{
		return ASSETDOC_ERROR_JSON;
	}
	int size = (int)(tokens[i].end - tokens[i].start);
	char* result = (char*)options->memory.alloc_func(options->memory.user_data, size + 1);
	if (!result)
	{
		return ASSETDOC_ERROR_NOMEM;
	}
	strncpy(result, (const char*)json_chunk + tokens[i].start, size);
	result[size] = 0;
	*out_string = result;
	return i + 1;
}

static int assetdoc_parse_json_array(assetdoc_options* options, jsmntok_t const* tokens, int i, const uint8_t* json_chunk, size_t element_size, void** out_array, assetdoc_size* out_size)
{
	(void)json_chunk;
	if (tokens[i].type != JSMN_ARRAY)
	{
		return tokens[i].type == JSMN_OBJECT ? ASSETDOC_ERROR_LEGACY : ASSETDOC_ERROR_JSON;
	}
	if (*out_array)
	{
		return ASSETDOC_ERROR_JSON;
	}
	int size = tokens[i].size;
	void* result = assetdoc_calloc(options, element_size, size);
	if (!result)
	{
		return ASSETDOC_ERROR_NOMEM;
	}
	*out_array = result;
	*out_size = size;
	return i + 1;
}

static int assetdoc_parse_json_string_array(assetdoc_options* options, jsmntok_t const* tokens, int i, const uint8_t* json_chunk, char*** out_array, assetdoc_size* out_size)
{
	ASSETDOC_CHECK_TOKTYPE(tokens[i], JSMN_ARRAY);
	i = assetdoc_parse_json_array(options, tokens, i, json_chunk, sizeof(char*), (void**)out_array, out_size);
	if (i < 0)
	{
		return i;
	}

	for (assetdoc_size j = 0; j < *out_size; ++j)
	{
		i = assetdoc_parse_json_string(options, tokens, i, json_chunk, j + (*out_array));
		if (i < 0)
		{
			return i;
		}
	}
	return i;
}

static void assetdoc_parse_attribute_type(const char* name, assetdoc_attribute_type* out_type, int* out_index)
{
	if (*name == '_')
	{
		*out_type = assetdoc_attribute_type_custom;
		return;
	}

	const char* us = strchr(name, '_');
	size_t len = us ? (size_t)(us - name) : strlen(name);

	if (len == 8 && strncmp(name, "POSITION", 8) == 0)
	{
		*out_type = assetdoc_attribute_type_position;
	}
	else if (len == 6 && strncmp(name, "NORMAL", 6) == 0)
	{
		*out_type = assetdoc_attribute_type_normal;
	}
	else if (len == 7 && strncmp(name, "TANGENT", 7) == 0)
	{
		*out_type = assetdoc_attribute_type_tangent;
	}
	else if (len == 8 && strncmp(name, "TEXCOORD", 8) == 0)
	{
		*out_type = assetdoc_attribute_type_texcoord;
	}
	else if (len == 5 && strncmp(name, "COLOR", 5) == 0)
	{
		*out_type = assetdoc_attribute_type_color;
	}
	else if (len == 6 && strncmp(name, "JOINTS", 6) == 0)
	{
		*out_type = assetdoc_attribute_type_joints;
	}
	else if (len == 7 && strncmp(name, "WEIGHTS", 7) == 0)
	{
		*out_type = assetdoc_attribute_type_weights;
	}
	else
	{
		*out_type = assetdoc_attribute_type_invalid;
	}

	if (us && *out_type != assetdoc_attribute_type_invalid)
	{
		*out_index = ASSETDOC_ATOI(us + 1);
		if (*out_index < 0)
		{
			*out_type = assetdoc_attribute_type_invalid;
			*out_index = 0;
		}
	}
}

static int assetdoc_parse_json_attribute_list(assetdoc_options* options, jsmntok_t const* tokens, int i, const uint8_t* json_chunk, assetdoc_attribute** out_attributes, assetdoc_size* out_attributes_count)
{
	ASSETDOC_CHECK_TOKTYPE(tokens[i], JSMN_OBJECT);

	if (*out_attributes)
	{
		return ASSETDOC_ERROR_JSON;
	}

	*out_attributes_count = tokens[i].size;
	*out_attributes = (assetdoc_attribute*)assetdoc_calloc(options, sizeof(assetdoc_attribute), *out_attributes_count);
	++i;

	if (!*out_attributes)
	{
		return ASSETDOC_ERROR_NOMEM;
	}

	for (assetdoc_size j = 0; j < *out_attributes_count; ++j)
	{
		ASSETDOC_CHECK_KEY(tokens[i]);

		i = assetdoc_parse_json_string(options, tokens, i, json_chunk, &(*out_attributes)[j].name);
		if (i < 0)
		{
			return ASSETDOC_ERROR_JSON;
		}

		assetdoc_parse_attribute_type((*out_attributes)[j].name, &(*out_attributes)[j].type, &(*out_attributes)[j].index);

		(*out_attributes)[j].data = ASSETDOC_PTRINDEX(assetdoc_accessor, assetdoc_json_to_int(tokens + i, json_chunk));
		++i;
	}

	return i;
}

static int assetdoc_parse_json_extras(assetdoc_options* options, jsmntok_t const* tokens, int i, const uint8_t* json_chunk, assetdoc_extras* out_extras)
{
	if (out_extras->data)
	{
		return ASSETDOC_ERROR_JSON;
	}

	/* fill deprecated fields for now, this will be removed in the future */
	out_extras->start_offset = tokens[i].start;
	out_extras->end_offset = tokens[i].end;

	size_t start = tokens[i].start;
	size_t size = tokens[i].end - start;
	out_extras->data = (char*)options->memory.alloc_func(options->memory.user_data, size + 1);
	if (!out_extras->data)
	{
		return ASSETDOC_ERROR_NOMEM;
	}
	strncpy(out_extras->data, (const char*)json_chunk + start, size);
	out_extras->data[size] = '\0';

	i = assetdoc_skip_json(tokens, i);
	return i;
}

static int assetdoc_parse_json_unprocessed_extension(assetdoc_options* options, jsmntok_t const* tokens, int i, const uint8_t* json_chunk, assetdoc_extension* out_extension)
{
	ASSETDOC_CHECK_TOKTYPE(tokens[i], JSMN_STRING);
	ASSETDOC_CHECK_TOKTYPE(tokens[i+1], JSMN_OBJECT);
	if (out_extension->name)
	{
		return ASSETDOC_ERROR_JSON;
	}

	assetdoc_size name_length = tokens[i].end - tokens[i].start;
	out_extension->name = (char*)options->memory.alloc_func(options->memory.user_data, name_length + 1);
	if (!out_extension->name)
	{
		return ASSETDOC_ERROR_NOMEM;
	}
	strncpy(out_extension->name, (const char*)json_chunk + tokens[i].start, name_length);
	out_extension->name[name_length] = 0;
	i++;

	size_t start = tokens[i].start;
	size_t size = tokens[i].end - start;
	out_extension->data = (char*)options->memory.alloc_func(options->memory.user_data, size + 1);
	if (!out_extension->data)
	{
		return ASSETDOC_ERROR_NOMEM;
	}
	strncpy(out_extension->data, (const char*)json_chunk + start, size);
	out_extension->data[size] = '\0';

	i = assetdoc_skip_json(tokens, i);

	return i;
}

static int assetdoc_parse_json_unprocessed_extensions(assetdoc_options* options, jsmntok_t const* tokens, int i, const uint8_t* json_chunk, assetdoc_size* out_extensions_count, assetdoc_extension** out_extensions)
{
	++i;

	ASSETDOC_CHECK_TOKTYPE(tokens[i], JSMN_OBJECT);
	if(*out_extensions)
	{
		return ASSETDOC_ERROR_JSON;
	}

	int extensions_size = tokens[i].size;
	*out_extensions_count = 0;
	*out_extensions = (assetdoc_extension*)assetdoc_calloc(options, sizeof(assetdoc_extension), extensions_size);

	if (!*out_extensions)
	{
		return ASSETDOC_ERROR_NOMEM;
	}

	++i;

	for (int j = 0; j < extensions_size; ++j)
	{
		ASSETDOC_CHECK_KEY(tokens[i]);

		assetdoc_size extension_index = (*out_extensions_count)++;
		assetdoc_extension* extension = &((*out_extensions)[extension_index]);
		i = assetdoc_parse_json_unprocessed_extension(options, tokens, i, json_chunk, extension);

		if (i < 0)
		{
			return i;
		}
	}
	return i;
}

static int assetdoc_parse_json_draco_mesh_compression(assetdoc_options* options, jsmntok_t const* tokens, int i, const uint8_t* json_chunk, assetdoc_draco_mesh_compression* out_draco_mesh_compression)
{
	ASSETDOC_CHECK_TOKTYPE(tokens[i], JSMN_OBJECT);

	int size = tokens[i].size;
	++i;

	for (int j = 0; j < size; ++j)
	{
		ASSETDOC_CHECK_KEY(tokens[i]);

		if (assetdoc_json_strcmp(tokens + i, json_chunk, "attributes") == 0)
		{
			i = assetdoc_parse_json_attribute_list(options, tokens, i + 1, json_chunk, &out_draco_mesh_compression->attributes, &out_draco_mesh_compression->attributes_count);
		}
		else if (assetdoc_json_strcmp(tokens + i, json_chunk, "bufferView") == 0)
		{
			++i;
			out_draco_mesh_compression->buffer_view = ASSETDOC_PTRINDEX(assetdoc_buffer_view, assetdoc_json_to_int(tokens + i, json_chunk));
			++i;
		}
		else
		{
			i = assetdoc_skip_json(tokens, i+1);
		}

		if (i < 0)
		{
			return i;
		}
	}

	return i;
}

static int assetdoc_parse_json_mesh_gpu_instancing(assetdoc_options* options, jsmntok_t const* tokens, int i, const uint8_t* json_chunk, assetdoc_mesh_gpu_instancing* out_mesh_gpu_instancing)
{
	ASSETDOC_CHECK_TOKTYPE(tokens[i], JSMN_OBJECT);

	int size = tokens[i].size;
	++i;

	for (int j = 0; j < size; ++j)
	{
		ASSETDOC_CHECK_KEY(tokens[i]);

		if (assetdoc_json_strcmp(tokens + i, json_chunk, "attributes") == 0)
		{
			i = assetdoc_parse_json_attribute_list(options, tokens, i + 1, json_chunk, &out_mesh_gpu_instancing->attributes, &out_mesh_gpu_instancing->attributes_count);
		}
		else
		{
			i = assetdoc_skip_json(tokens, i+1);
		}

		if (i < 0)
		{
			return i;
		}
	}

	return i;
}

static int assetdoc_parse_json_material_mapping_data(assetdoc_options* options, jsmntok_t const* tokens, int i, const uint8_t* json_chunk, assetdoc_material_mapping* out_mappings, assetdoc_size* offset)
{
	(void)options;
	ASSETDOC_CHECK_TOKTYPE(tokens[i], JSMN_ARRAY);

	int size = tokens[i].size;
	++i;

	for (int j = 0; j < size; ++j)
	{
		ASSETDOC_CHECK_TOKTYPE(tokens[i], JSMN_OBJECT);

		int obj_size = tokens[i].size;
		++i;

		int material = -1;
		int variants_tok = -1;
		int extras_tok = -1;

		for (int k = 0; k < obj_size; ++k)
		{
			ASSETDOC_CHECK_KEY(tokens[i]);

			if (assetdoc_json_strcmp(tokens + i, json_chunk, "material") == 0)
			{
				++i;
				material = assetdoc_json_to_int(tokens + i, json_chunk);
				++i;
			}
			else if (assetdoc_json_strcmp(tokens + i, json_chunk, "variants") == 0)
			{
				variants_tok = i+1;
				ASSETDOC_CHECK_TOKTYPE(tokens[variants_tok], JSMN_ARRAY);

				i = assetdoc_skip_json(tokens, i+1);
			}
			else if (assetdoc_json_strcmp(tokens + i, json_chunk, "extras") == 0)
			{
				extras_tok = i + 1;
				i = assetdoc_skip_json(tokens, extras_tok);
			}
			else
			{
				i = assetdoc_skip_json(tokens, i+1);
			}

			if (i < 0)
			{
				return i;
			}
		}

		if (material < 0 || variants_tok < 0)
		{
			return ASSETDOC_ERROR_JSON;
		}

		if (out_mappings)
		{
			for (int k = 0; k < tokens[variants_tok].size; ++k)
			{
				int variant = assetdoc_json_to_int(&tokens[variants_tok + 1 + k], json_chunk);
				if (variant < 0)
					return variant;

				out_mappings[*offset].material = ASSETDOC_PTRINDEX(assetdoc_material, material);
				out_mappings[*offset].variant = variant;

				if (extras_tok >= 0)
				{
					int e = assetdoc_parse_json_extras(options, tokens, extras_tok, json_chunk, &out_mappings[*offset].extras);
					if (e < 0)
						return e;
				}

				(*offset)++;
			}
		}
		else
		{
			(*offset) += tokens[variants_tok].size;
		}
	}

	return i;
}

static int assetdoc_parse_json_material_mappings(assetdoc_options* options, jsmntok_t const* tokens, int i, const uint8_t* json_chunk, assetdoc_primitive* out_prim)
{
	ASSETDOC_CHECK_TOKTYPE(tokens[i], JSMN_OBJECT);

	int size = tokens[i].size;
	++i;

	for (int j = 0; j < size; ++j)
	{
		ASSETDOC_CHECK_KEY(tokens[i]);

		if (assetdoc_json_strcmp(tokens + i, json_chunk, "mappings") == 0)
		{
			if (out_prim->mappings)
			{
				return ASSETDOC_ERROR_JSON;
			}

			assetdoc_size mappings_offset = 0;
			int k = assetdoc_parse_json_material_mapping_data(options, tokens, i + 1, json_chunk, NULL, &mappings_offset);
			if (k < 0)
			{
				return k;
			}

			out_prim->mappings_count = mappings_offset;
			out_prim->mappings = (assetdoc_material_mapping*)assetdoc_calloc(options, sizeof(assetdoc_material_mapping), out_prim->mappings_count);

			mappings_offset = 0;
			i = assetdoc_parse_json_material_mapping_data(options, tokens, i + 1, json_chunk, out_prim->mappings, &mappings_offset);
		}
		else
		{
			i = assetdoc_skip_json(tokens, i+1);
		}

		if (i < 0)
		{
			return i;
		}
	}

	return i;
}

static assetdoc_primitive_type assetdoc_json_to_primitive_type(jsmntok_t const* tok, const uint8_t* json_chunk)
{
	int type = assetdoc_json_to_int(tok, json_chunk);

	switch (type)
	{
	case 0:
		return assetdoc_primitive_type_points;
	case 1:
		return assetdoc_primitive_type_lines;
	case 2:
		return assetdoc_primitive_type_line_loop;
	case 3:
		return assetdoc_primitive_type_line_strip;
	case 4:
		return assetdoc_primitive_type_triangles;
	case 5:
		return assetdoc_primitive_type_triangle_strip;
	case 6:
		return assetdoc_primitive_type_triangle_fan;
	default:
		return assetdoc_primitive_type_invalid;
	}
}

static int assetdoc_parse_json_primitive(assetdoc_options* options, jsmntok_t const* tokens, int i, const uint8_t* json_chunk, assetdoc_primitive* out_prim)
{
	ASSETDOC_CHECK_TOKTYPE(tokens[i], JSMN_OBJECT);

	out_prim->type = assetdoc_primitive_type_triangles;

	int size = tokens[i].size;
	++i;

	for (int j = 0; j < size; ++j)
	{
		ASSETDOC_CHECK_KEY(tokens[i]);

		if (assetdoc_json_strcmp(tokens+i, json_chunk, "mode") == 0)
		{
			++i;
			out_prim->type = assetdoc_json_to_primitive_type(tokens+i, json_chunk);
			++i;
		}
		else if (assetdoc_json_strcmp(tokens+i, json_chunk, "indices") == 0)
		{
			++i;
			out_prim->indices = ASSETDOC_PTRINDEX(assetdoc_accessor, assetdoc_json_to_int(tokens + i, json_chunk));
			++i;
		}
		else if (assetdoc_json_strcmp(tokens+i, json_chunk, "material") == 0)
		{
			++i;
			out_prim->material = ASSETDOC_PTRINDEX(assetdoc_material, assetdoc_json_to_int(tokens + i, json_chunk));
			++i;
		}
		else if (assetdoc_json_strcmp(tokens+i, json_chunk, "attributes") == 0)
		{
			i = assetdoc_parse_json_attribute_list(options, tokens, i + 1, json_chunk, &out_prim->attributes, &out_prim->attributes_count);
		}
		else if (assetdoc_json_strcmp(tokens+i, json_chunk, "targets") == 0)
		{
			i = assetdoc_parse_json_array(options, tokens, i + 1, json_chunk, sizeof(assetdoc_morph_target), (void**)&out_prim->targets, &out_prim->targets_count);
			if (i < 0)
			{
				return i;
			}

			for (assetdoc_size k = 0; k < out_prim->targets_count; ++k)
			{
				i = assetdoc_parse_json_attribute_list(options, tokens, i, json_chunk, &out_prim->targets[k].attributes, &out_prim->targets[k].attributes_count);
				if (i < 0)
				{
					return i;
				}
			}
		}
		else if (assetdoc_json_strcmp(tokens + i, json_chunk, "extras") == 0)
		{
			i = assetdoc_parse_json_extras(options, tokens, i + 1, json_chunk, &out_prim->extras);
		}
		else if (assetdoc_json_strcmp(tokens + i, json_chunk, "extensions") == 0)
		{
			++i;

			ASSETDOC_CHECK_TOKTYPE(tokens[i], JSMN_OBJECT);
			if(out_prim->extensions)
			{
				return ASSETDOC_ERROR_JSON;
			}

			int extensions_size = tokens[i].size;
			out_prim->extensions_count = 0;
			out_prim->extensions = (assetdoc_extension*)assetdoc_calloc(options, sizeof(assetdoc_extension), extensions_size);

			if (!out_prim->extensions)
			{
				return ASSETDOC_ERROR_NOMEM;
			}

			++i;
			for (int k = 0; k < extensions_size; ++k)
			{
				ASSETDOC_CHECK_KEY(tokens[i]);

				if (assetdoc_json_strcmp(tokens+i, json_chunk, "KHR_draco_mesh_compression") == 0)
				{
					out_prim->has_draco_mesh_compression = 1;
					i = assetdoc_parse_json_draco_mesh_compression(options, tokens, i + 1, json_chunk, &out_prim->draco_mesh_compression);
				}
				else if (assetdoc_json_strcmp(tokens+i, json_chunk, "KHR_materials_variants") == 0)
				{
					i = assetdoc_parse_json_material_mappings(options, tokens, i + 1, json_chunk, out_prim);
				}
				else
				{
					i = assetdoc_parse_json_unprocessed_extension(options, tokens, i, json_chunk, &(out_prim->extensions[out_prim->extensions_count++]));
				}

				if (i < 0)
				{
					return i;
				}
			}
		}
		else
		{
			i = assetdoc_skip_json(tokens, i+1);
		}

		if (i < 0)
		{
			return i;
		}
	}

	return i;
}

static int assetdoc_parse_json_mesh(assetdoc_options* options, jsmntok_t const* tokens, int i, const uint8_t* json_chunk, assetdoc_mesh* out_mesh)
{
	ASSETDOC_CHECK_TOKTYPE(tokens[i], JSMN_OBJECT);

	int size = tokens[i].size;
	++i;

	for (int j = 0; j < size; ++j)
	{
		ASSETDOC_CHECK_KEY(tokens[i]);

		if (assetdoc_json_strcmp(tokens+i, json_chunk, "name") == 0)
		{
			i = assetdoc_parse_json_string(options, tokens, i + 1, json_chunk, &out_mesh->name);
		}
		else if (assetdoc_json_strcmp(tokens+i, json_chunk, "primitives") == 0)
		{
			i = assetdoc_parse_json_array(options, tokens, i + 1, json_chunk, sizeof(assetdoc_primitive), (void**)&out_mesh->primitives, &out_mesh->primitives_count);
			if (i < 0)
			{
				return i;
			}

			for (assetdoc_size prim_index = 0; prim_index < out_mesh->primitives_count; ++prim_index)
			{
				i = assetdoc_parse_json_primitive(options, tokens, i, json_chunk, &out_mesh->primitives[prim_index]);
				if (i < 0)
				{
					return i;
				}
			}
		}
		else if (assetdoc_json_strcmp(tokens + i, json_chunk, "weights") == 0)
		{
			i = assetdoc_parse_json_array(options, tokens, i + 1, json_chunk, sizeof(assetdoc_float), (void**)&out_mesh->weights, &out_mesh->weights_count);
			if (i < 0)
			{
				return i;
			}

			i = assetdoc_parse_json_float_array(tokens, i - 1, json_chunk, out_mesh->weights, (int)out_mesh->weights_count);
		}
		else if (assetdoc_json_strcmp(tokens + i, json_chunk, "extras") == 0)
		{
			++i;

			out_mesh->extras.start_offset = tokens[i].start;
			out_mesh->extras.end_offset = tokens[i].end;

			if (tokens[i].type == JSMN_OBJECT)
			{
				int extras_size = tokens[i].size;
				++i;

				for (int k = 0; k < extras_size; ++k)
				{
					ASSETDOC_CHECK_KEY(tokens[i]);

					if (assetdoc_json_strcmp(tokens+i, json_chunk, "targetNames") == 0 && tokens[i+1].type == JSMN_ARRAY)
					{
						i = assetdoc_parse_json_string_array(options, tokens, i + 1, json_chunk, &out_mesh->target_names, &out_mesh->target_names_count);
					}
					else
					{
						i = assetdoc_skip_json(tokens, i+1);
					}

					if (i < 0)
					{
						return i;
					}
				}
			}
			else
			{
				i = assetdoc_skip_json(tokens, i);
			}
		}
		else if (assetdoc_json_strcmp(tokens + i, json_chunk, "extensions") == 0)
		{
			i = assetdoc_parse_json_unprocessed_extensions(options, tokens, i, json_chunk, &out_mesh->extensions_count, &out_mesh->extensions);
		}
		else
		{
			i = assetdoc_skip_json(tokens, i+1);
		}

		if (i < 0)
		{
			return i;
		}
	}

	return i;
}

static int assetdoc_parse_json_meshes(assetdoc_options* options, jsmntok_t const* tokens, int i, const uint8_t* json_chunk, assetdoc_data* out_data)
{
	i = assetdoc_parse_json_array(options, tokens, i, json_chunk, sizeof(assetdoc_mesh), (void**)&out_data->meshes, &out_data->meshes_count);
	if (i < 0)
	{
		return i;
	}

	for (assetdoc_size j = 0; j < out_data->meshes_count; ++j)
	{
		i = assetdoc_parse_json_mesh(options, tokens, i, json_chunk, &out_data->meshes[j]);
		if (i < 0)
		{
			return i;
		}
	}
	return i;
}

static assetdoc_component_type assetdoc_json_to_component_type(jsmntok_t const* tok, const uint8_t* json_chunk)
{
	int type = assetdoc_json_to_int(tok, json_chunk);

	switch (type)
	{
	case 5120:
		return assetdoc_component_type_r_8;
	case 5121:
		return assetdoc_component_type_r_8u;
	case 5122:
		return assetdoc_component_type_r_16;
	case 5123:
		return assetdoc_component_type_r_16u;
	case 5125:
		return assetdoc_component_type_r_32u;
	case 5126:
		return assetdoc_component_type_r_32f;
	default:
		return assetdoc_component_type_invalid;
	}
}

static int assetdoc_parse_json_accessor_sparse(jsmntok_t const* tokens, int i, const uint8_t* json_chunk, assetdoc_accessor_sparse* out_sparse)
{
	ASSETDOC_CHECK_TOKTYPE(tokens[i], JSMN_OBJECT);

	int size = tokens[i].size;
	++i;

	for (int j = 0; j < size; ++j)
	{
		ASSETDOC_CHECK_KEY(tokens[i]);

		if (assetdoc_json_strcmp(tokens+i, json_chunk, "count") == 0)
		{
			++i;
			out_sparse->count = assetdoc_json_to_size(tokens + i, json_chunk);
			++i;
		}
		else if (assetdoc_json_strcmp(tokens+i, json_chunk, "indices") == 0)
		{
			++i;
			ASSETDOC_CHECK_TOKTYPE(tokens[i], JSMN_OBJECT);

			int indices_size = tokens[i].size;
			++i;

			for (int k = 0; k < indices_size; ++k)
			{
				ASSETDOC_CHECK_KEY(tokens[i]);

				if (assetdoc_json_strcmp(tokens+i, json_chunk, "bufferView") == 0)
				{
					++i;
					out_sparse->indices_buffer_view = ASSETDOC_PTRINDEX(assetdoc_buffer_view, assetdoc_json_to_int(tokens + i, json_chunk));
					++i;
				}
				else if (assetdoc_json_strcmp(tokens+i, json_chunk, "byteOffset") == 0)
				{
					++i;
					out_sparse->indices_byte_offset = assetdoc_json_to_size(tokens + i, json_chunk);
					++i;
				}
				else if (assetdoc_json_strcmp(tokens+i, json_chunk, "componentType") == 0)
				{
					++i;
					out_sparse->indices_component_type = assetdoc_json_to_component_type(tokens + i, json_chunk);
					++i;
				}
				else
				{
					i = assetdoc_skip_json(tokens, i+1);
				}

				if (i < 0)
				{
					return i;
				}
			}
		}
		else if (assetdoc_json_strcmp(tokens+i, json_chunk, "values") == 0)
		{
			++i;
			ASSETDOC_CHECK_TOKTYPE(tokens[i], JSMN_OBJECT);

			int values_size = tokens[i].size;
			++i;

			for (int k = 0; k < values_size; ++k)
			{
				ASSETDOC_CHECK_KEY(tokens[i]);

				if (assetdoc_json_strcmp(tokens+i, json_chunk, "bufferView") == 0)
				{
					++i;
					out_sparse->values_buffer_view = ASSETDOC_PTRINDEX(assetdoc_buffer_view, assetdoc_json_to_int(tokens + i, json_chunk));
					++i;
				}
				else if (assetdoc_json_strcmp(tokens+i, json_chunk, "byteOffset") == 0)
				{
					++i;
					out_sparse->values_byte_offset = assetdoc_json_to_size(tokens + i, json_chunk);
					++i;
				}
				else
				{
					i = assetdoc_skip_json(tokens, i+1);
				}

				if (i < 0)
				{
					return i;
				}
			}
		}
		else
		{
			i = assetdoc_skip_json(tokens, i+1);
		}

		if (i < 0)
		{
			return i;
		}
	}

	return i;
}

static int assetdoc_parse_json_accessor(assetdoc_options* options, jsmntok_t const* tokens, int i, const uint8_t* json_chunk, assetdoc_accessor* out_accessor)
{
	ASSETDOC_CHECK_TOKTYPE(tokens[i], JSMN_OBJECT);

	int size = tokens[i].size;
	++i;

	for (int j = 0; j < size; ++j)
	{
		ASSETDOC_CHECK_KEY(tokens[i]);

		if (assetdoc_json_strcmp(tokens + i, json_chunk, "name") == 0)
		{
			i = assetdoc_parse_json_string(options, tokens, i + 1, json_chunk, &out_accessor->name);
		}
		else if (assetdoc_json_strcmp(tokens+i, json_chunk, "bufferView") == 0)
		{
			++i;
			out_accessor->buffer_view = ASSETDOC_PTRINDEX(assetdoc_buffer_view, assetdoc_json_to_int(tokens + i, json_chunk));
			++i;
		}
		else if (assetdoc_json_strcmp(tokens+i, json_chunk, "byteOffset") == 0)
		{
			++i;
			out_accessor->offset =
					assetdoc_json_to_size(tokens+i, json_chunk);
			++i;
		}
		else if (assetdoc_json_strcmp(tokens+i, json_chunk, "componentType") == 0)
		{
			++i;
			out_accessor->component_type = assetdoc_json_to_component_type(tokens + i, json_chunk);
			++i;
		}
		else if (assetdoc_json_strcmp(tokens+i, json_chunk, "normalized") == 0)
		{
			++i;
			out_accessor->normalized = assetdoc_json_to_bool(tokens+i, json_chunk);
			++i;
		}
		else if (assetdoc_json_strcmp(tokens+i, json_chunk, "count") == 0)
		{
			++i;
			out_accessor->count = assetdoc_json_to_size(tokens+i, json_chunk);
			++i;
		}
		else if (assetdoc_json_strcmp(tokens+i, json_chunk, "type") == 0)
		{
			++i;
			if (assetdoc_json_strcmp(tokens+i, json_chunk, "SCALAR") == 0)
			{
				out_accessor->type = assetdoc_type_scalar;
			}
			else if (assetdoc_json_strcmp(tokens+i, json_chunk, "VEC2") == 0)
			{
				out_accessor->type = assetdoc_type_vec2;
			}
			else if (assetdoc_json_strcmp(tokens+i, json_chunk, "VEC3") == 0)
			{
				out_accessor->type = assetdoc_type_vec3;
			}
			else if (assetdoc_json_strcmp(tokens+i, json_chunk, "VEC4") == 0)
			{
				out_accessor->type = assetdoc_type_vec4;
			}
			else if (assetdoc_json_strcmp(tokens+i, json_chunk, "MAT2") == 0)
			{
				out_accessor->type = assetdoc_type_mat2;
			}
			else if (assetdoc_json_strcmp(tokens+i, json_chunk, "MAT3") == 0)
			{
				out_accessor->type = assetdoc_type_mat3;
			}
			else if (assetdoc_json_strcmp(tokens+i, json_chunk, "MAT4") == 0)
			{
				out_accessor->type = assetdoc_type_mat4;
			}
			++i;
		}
		else if (assetdoc_json_strcmp(tokens + i, json_chunk, "min") == 0)
		{
			++i;
			out_accessor->has_min = 1;
			// note: we can't parse the precise number of elements since type may not have been computed yet
			int min_size = tokens[i].size > 16 ? 16 : tokens[i].size;
			i = assetdoc_parse_json_float_array(tokens, i, json_chunk, out_accessor->min, min_size);
		}
		else if (assetdoc_json_strcmp(tokens + i, json_chunk, "max") == 0)
		{
			++i;
			out_accessor->has_max = 1;
			// note: we can't parse the precise number of elements since type may not have been computed yet
			int max_size = tokens[i].size > 16 ? 16 : tokens[i].size;
			i = assetdoc_parse_json_float_array(tokens, i, json_chunk, out_accessor->max, max_size);
		}
		else if (assetdoc_json_strcmp(tokens + i, json_chunk, "sparse") == 0)
		{
			out_accessor->is_sparse = 1;
			i = assetdoc_parse_json_accessor_sparse(tokens, i + 1, json_chunk, &out_accessor->sparse);
		}
		else if (assetdoc_json_strcmp(tokens + i, json_chunk, "extras") == 0)
		{
			i = assetdoc_parse_json_extras(options, tokens, i + 1, json_chunk, &out_accessor->extras);
		}
		else if (assetdoc_json_strcmp(tokens + i, json_chunk, "extensions") == 0)
		{
			i = assetdoc_parse_json_unprocessed_extensions(options, tokens, i, json_chunk, &out_accessor->extensions_count, &out_accessor->extensions);
		}
		else
		{
			i = assetdoc_skip_json(tokens, i+1);
		}

		if (i < 0)
		{
			return i;
		}
	}

	return i;
}

static int assetdoc_parse_json_texture_transform(jsmntok_t const* tokens, int i, const uint8_t* json_chunk, assetdoc_texture_transform* out_texture_transform)
{
	ASSETDOC_CHECK_TOKTYPE(tokens[i], JSMN_OBJECT);

	int size = tokens[i].size;
	++i;

	for (int j = 0; j < size; ++j)
	{
		ASSETDOC_CHECK_KEY(tokens[i]);

		if (assetdoc_json_strcmp(tokens + i, json_chunk, "offset") == 0)
		{
			i = assetdoc_parse_json_float_array(tokens, i + 1, json_chunk, out_texture_transform->offset, 2);
		}
		else if (assetdoc_json_strcmp(tokens + i, json_chunk, "rotation") == 0)
		{
			++i;
			out_texture_transform->rotation = assetdoc_json_to_float(tokens + i, json_chunk);
			++i;
		}
		else if (assetdoc_json_strcmp(tokens + i, json_chunk, "scale") == 0)
		{
			i = assetdoc_parse_json_float_array(tokens, i + 1, json_chunk, out_texture_transform->scale, 2);
		}
		else if (assetdoc_json_strcmp(tokens + i, json_chunk, "texCoord") == 0)
		{
			++i;
			out_texture_transform->has_texcoord = 1;
			out_texture_transform->texcoord = assetdoc_json_to_int(tokens + i, json_chunk);
			++i;
		}
		else
		{
			i = assetdoc_skip_json(tokens, i + 1);
		}

		if (i < 0)
		{
			return i;
		}
	}

	return i;
}

static int assetdoc_parse_json_texture_view(assetdoc_options* options, jsmntok_t const* tokens, int i, const uint8_t* json_chunk, assetdoc_texture_view* out_texture_view)
{
	(void)options;

	ASSETDOC_CHECK_TOKTYPE(tokens[i], JSMN_OBJECT);

	out_texture_view->scale = 1.0f;
	assetdoc_fill_float_array(out_texture_view->transform.scale, 2, 1.0f);

	int size = tokens[i].size;
	++i;

	for (int j = 0; j < size; ++j)
	{
		ASSETDOC_CHECK_KEY(tokens[i]);

		if (assetdoc_json_strcmp(tokens + i, json_chunk, "index") == 0)
		{
			++i;
			out_texture_view->texture = ASSETDOC_PTRINDEX(assetdoc_texture, assetdoc_json_to_int(tokens + i, json_chunk));
			++i;
		}
		else if (assetdoc_json_strcmp(tokens + i, json_chunk, "texCoord") == 0)
		{
			++i;
			out_texture_view->texcoord = assetdoc_json_to_int(tokens + i, json_chunk);
			++i;
		}
		else if (assetdoc_json_strcmp(tokens + i, json_chunk, "scale") == 0)
		{
			++i;
			out_texture_view->scale = assetdoc_json_to_float(tokens + i, json_chunk);
			++i;
		}
		else if (assetdoc_json_strcmp(tokens + i, json_chunk, "strength") == 0)
		{
			++i;
			out_texture_view->scale = assetdoc_json_to_float(tokens + i, json_chunk);
			++i;
		}
		else if (assetdoc_json_strcmp(tokens + i, json_chunk, "extensions") == 0)
		{
			++i;

			ASSETDOC_CHECK_TOKTYPE(tokens[i], JSMN_OBJECT);
			int extensions_size = tokens[i].size;

			++i;

			for (int k = 0; k < extensions_size; ++k)
			{
				ASSETDOC_CHECK_KEY(tokens[i]);

				if (assetdoc_json_strcmp(tokens+i, json_chunk, "KHR_texture_transform") == 0)
				{
					out_texture_view->has_transform = 1;
					i = assetdoc_parse_json_texture_transform(tokens, i + 1, json_chunk, &out_texture_view->transform);
				}
				else
				{
					i = assetdoc_skip_json(tokens, i + 1);
				}

				if (i < 0)
				{
					return i;
				}
			}
		}
		else
		{
			i = assetdoc_skip_json(tokens, i + 1);
		}

		if (i < 0)
		{
			return i;
		}
	}

	return i;
}

static int assetdoc_parse_json_pbr_metallic_roughness(assetdoc_options* options, jsmntok_t const* tokens, int i, const uint8_t* json_chunk, assetdoc_pbr_metallic_roughness* out_pbr)
{
	ASSETDOC_CHECK_TOKTYPE(tokens[i], JSMN_OBJECT);

	int size = tokens[i].size;
	++i;

	for (int j = 0; j < size; ++j)
	{
		ASSETDOC_CHECK_KEY(tokens[i]);

		if (assetdoc_json_strcmp(tokens+i, json_chunk, "metallicFactor") == 0)
		{
			++i;
			out_pbr->metallic_factor =
				assetdoc_json_to_float(tokens + i, json_chunk);
			++i;
		}
		else if (assetdoc_json_strcmp(tokens+i, json_chunk, "roughnessFactor") == 0)
		{
			++i;
			out_pbr->roughness_factor =
				assetdoc_json_to_float(tokens+i, json_chunk);
			++i;
		}
		else if (assetdoc_json_strcmp(tokens+i, json_chunk, "baseColorFactor") == 0)
		{
			i = assetdoc_parse_json_float_array(tokens, i + 1, json_chunk, out_pbr->base_color_factor, 4);
		}
		else if (assetdoc_json_strcmp(tokens+i, json_chunk, "baseColorTexture") == 0)
		{
			i = assetdoc_parse_json_texture_view(options, tokens, i + 1, json_chunk, &out_pbr->base_color_texture);
		}
		else if (assetdoc_json_strcmp(tokens + i, json_chunk, "metallicRoughnessTexture") == 0)
		{
			i = assetdoc_parse_json_texture_view(options, tokens, i + 1, json_chunk, &out_pbr->metallic_roughness_texture);
		}
		else
		{
			i = assetdoc_skip_json(tokens, i+1);
		}

		if (i < 0)
		{
			return i;
		}
	}

	return i;
}

static int assetdoc_parse_json_pbr_specular_glossiness(assetdoc_options* options, jsmntok_t const* tokens, int i, const uint8_t* json_chunk, assetdoc_pbr_specular_glossiness* out_pbr)
{
	ASSETDOC_CHECK_TOKTYPE(tokens[i], JSMN_OBJECT);
	int size = tokens[i].size;
	++i;

	for (int j = 0; j < size; ++j)
	{
		ASSETDOC_CHECK_KEY(tokens[i]);

		if (assetdoc_json_strcmp(tokens+i, json_chunk, "diffuseFactor") == 0)
		{
			i = assetdoc_parse_json_float_array(tokens, i + 1, json_chunk, out_pbr->diffuse_factor, 4);
		}
		else if (assetdoc_json_strcmp(tokens+i, json_chunk, "specularFactor") == 0)
		{
			i = assetdoc_parse_json_float_array(tokens, i + 1, json_chunk, out_pbr->specular_factor, 3);
		}
		else if (assetdoc_json_strcmp(tokens+i, json_chunk, "glossinessFactor") == 0)
		{
			++i;
			out_pbr->glossiness_factor = assetdoc_json_to_float(tokens + i, json_chunk);
			++i;
		}
		else if (assetdoc_json_strcmp(tokens+i, json_chunk, "diffuseTexture") == 0)
		{
			i = assetdoc_parse_json_texture_view(options, tokens, i + 1, json_chunk, &out_pbr->diffuse_texture);
		}
		else if (assetdoc_json_strcmp(tokens+i, json_chunk, "specularGlossinessTexture") == 0)
		{
			i = assetdoc_parse_json_texture_view(options, tokens, i + 1, json_chunk, &out_pbr->specular_glossiness_texture);
		}
		else
		{
			i = assetdoc_skip_json(tokens, i+1);
		}

		if (i < 0)
		{
			return i;
		}
	}

	return i;
}

static int assetdoc_parse_json_clearcoat(assetdoc_options* options, jsmntok_t const* tokens, int i, const uint8_t* json_chunk, assetdoc_clearcoat* out_clearcoat)
{
	ASSETDOC_CHECK_TOKTYPE(tokens[i], JSMN_OBJECT);
	int size = tokens[i].size;
	++i;

	for (int j = 0; j < size; ++j)
	{
		ASSETDOC_CHECK_KEY(tokens[i]);

		if (assetdoc_json_strcmp(tokens+i, json_chunk, "clearcoatFactor") == 0)
		{
			++i;
			out_clearcoat->clearcoat_factor = assetdoc_json_to_float(tokens + i, json_chunk);
			++i;
		}
		else if (assetdoc_json_strcmp(tokens+i, json_chunk, "clearcoatRoughnessFactor") == 0)
		{
			++i;
			out_clearcoat->clearcoat_roughness_factor = assetdoc_json_to_float(tokens + i, json_chunk);
			++i;
		}
		else if (assetdoc_json_strcmp(tokens+i, json_chunk, "clearcoatTexture") == 0)
		{
			i = assetdoc_parse_json_texture_view(options, tokens, i + 1, json_chunk, &out_clearcoat->clearcoat_texture);
		}
		else if (assetdoc_json_strcmp(tokens+i, json_chunk, "clearcoatRoughnessTexture") == 0)
		{
			i = assetdoc_parse_json_texture_view(options, tokens, i + 1, json_chunk, &out_clearcoat->clearcoat_roughness_texture);
		}
		else if (assetdoc_json_strcmp(tokens+i, json_chunk, "clearcoatNormalTexture") == 0)
		{
			i = assetdoc_parse_json_texture_view(options, tokens, i + 1, json_chunk, &out_clearcoat->clearcoat_normal_texture);
		}
		else
		{
			i = assetdoc_skip_json(tokens, i+1);
		}

		if (i < 0)
		{
			return i;
		}
	}

	return i;
}

static int assetdoc_parse_json_ior(jsmntok_t const* tokens, int i, const uint8_t* json_chunk, assetdoc_ior* out_ior)
{
	ASSETDOC_CHECK_TOKTYPE(tokens[i], JSMN_OBJECT);
	int size = tokens[i].size;
	++i;

	// Default values
	out_ior->ior = 1.5f;

	for (int j = 0; j < size; ++j)
	{
		ASSETDOC_CHECK_KEY(tokens[i]);

		if (assetdoc_json_strcmp(tokens+i, json_chunk, "ior") == 0)
		{
			++i;
			out_ior->ior = assetdoc_json_to_float(tokens + i, json_chunk);
			++i;
		}
		else
		{
			i = assetdoc_skip_json(tokens, i+1);
		}

		if (i < 0)
		{
			return i;
		}
	}

	return i;
}

static int assetdoc_parse_json_specular(assetdoc_options* options, jsmntok_t const* tokens, int i, const uint8_t* json_chunk, assetdoc_specular* out_specular)
{
	ASSETDOC_CHECK_TOKTYPE(tokens[i], JSMN_OBJECT);
	int size = tokens[i].size;
	++i;

	// Default values
	out_specular->specular_factor = 1.0f;
	assetdoc_fill_float_array(out_specular->specular_color_factor, 3, 1.0f);

	for (int j = 0; j < size; ++j)
	{
		ASSETDOC_CHECK_KEY(tokens[i]);

		if (assetdoc_json_strcmp(tokens+i, json_chunk, "specularFactor") == 0)
		{
			++i;
			out_specular->specular_factor = assetdoc_json_to_float(tokens + i, json_chunk);
			++i;
		}
		else if (assetdoc_json_strcmp(tokens+i, json_chunk, "specularColorFactor") == 0)
		{
			i = assetdoc_parse_json_float_array(tokens, i + 1, json_chunk, out_specular->specular_color_factor, 3);
		}
		else if (assetdoc_json_strcmp(tokens+i, json_chunk, "specularTexture") == 0)
		{
			i = assetdoc_parse_json_texture_view(options, tokens, i + 1, json_chunk, &out_specular->specular_texture);
		}
		else if (assetdoc_json_strcmp(tokens + i, json_chunk, "specularColorTexture") == 0)
		{
			i = assetdoc_parse_json_texture_view(options, tokens, i + 1, json_chunk, &out_specular->specular_color_texture);
		}
		else
		{
			i = assetdoc_skip_json(tokens, i+1);
		}

		if (i < 0)
		{
			return i;
		}
	}

	return i;
}

static int assetdoc_parse_json_transmission(assetdoc_options* options, jsmntok_t const* tokens, int i, const uint8_t* json_chunk, assetdoc_transmission* out_transmission)
{
	ASSETDOC_CHECK_TOKTYPE(tokens[i], JSMN_OBJECT);
	int size = tokens[i].size;
	++i;

	for (int j = 0; j < size; ++j)
	{
		ASSETDOC_CHECK_KEY(tokens[i]);

		if (assetdoc_json_strcmp(tokens+i, json_chunk, "transmissionFactor") == 0)
		{
			++i;
			out_transmission->transmission_factor = assetdoc_json_to_float(tokens + i, json_chunk);
			++i;
		}
		else if (assetdoc_json_strcmp(tokens+i, json_chunk, "transmissionTexture") == 0)
		{
			i = assetdoc_parse_json_texture_view(options, tokens, i + 1, json_chunk, &out_transmission->transmission_texture);
		}
		else
		{
			i = assetdoc_skip_json(tokens, i+1);
		}

		if (i < 0)
		{
			return i;
		}
	}

	return i;
}

static int assetdoc_parse_json_volume(assetdoc_options* options, jsmntok_t const* tokens, int i, const uint8_t* json_chunk, assetdoc_volume* out_volume)
{
	ASSETDOC_CHECK_TOKTYPE(tokens[i], JSMN_OBJECT);
	int size = tokens[i].size;
	++i;

	for (int j = 0; j < size; ++j)
	{
		ASSETDOC_CHECK_KEY(tokens[i]);

		if (assetdoc_json_strcmp(tokens + i, json_chunk, "thicknessFactor") == 0)
		{
			++i;
			out_volume->thickness_factor = assetdoc_json_to_float(tokens + i, json_chunk);
			++i;
		}
		else if (assetdoc_json_strcmp(tokens + i, json_chunk, "thicknessTexture") == 0)
		{
			i = assetdoc_parse_json_texture_view(options, tokens, i + 1, json_chunk, &out_volume->thickness_texture);
		}
		else if (assetdoc_json_strcmp(tokens + i, json_chunk, "attenuationColor") == 0)
		{
			i = assetdoc_parse_json_float_array(tokens, i + 1, json_chunk, out_volume->attenuation_color, 3);
		}
		else if (assetdoc_json_strcmp(tokens + i, json_chunk, "attenuationDistance") == 0)
		{
			++i;
			out_volume->attenuation_distance = assetdoc_json_to_float(tokens + i, json_chunk);
			++i;
		}
		else
		{
			i = assetdoc_skip_json(tokens, i + 1);
		}

		if (i < 0)
		{
			return i;
		}
	}

	return i;
}

static int assetdoc_parse_json_sheen(assetdoc_options* options, jsmntok_t const* tokens, int i, const uint8_t* json_chunk, assetdoc_sheen* out_sheen)
{
	ASSETDOC_CHECK_TOKTYPE(tokens[i], JSMN_OBJECT);
	int size = tokens[i].size;
	++i;

	for (int j = 0; j < size; ++j)
	{
		ASSETDOC_CHECK_KEY(tokens[i]);

		if (assetdoc_json_strcmp(tokens+i, json_chunk, "sheenColorFactor") == 0)
		{
			i = assetdoc_parse_json_float_array(tokens, i + 1, json_chunk, out_sheen->sheen_color_factor, 3);
		}
		else if (assetdoc_json_strcmp(tokens+i, json_chunk, "sheenColorTexture") == 0)
		{
			i = assetdoc_parse_json_texture_view(options, tokens, i + 1, json_chunk, &out_sheen->sheen_color_texture);
		}
		else if (assetdoc_json_strcmp(tokens+i, json_chunk, "sheenRoughnessFactor") == 0)
		{
			++i;
			out_sheen->sheen_roughness_factor = assetdoc_json_to_float(tokens + i, json_chunk);
			++i;
		}
		else if (assetdoc_json_strcmp(tokens+i, json_chunk, "sheenRoughnessTexture") == 0)
		{
			i = assetdoc_parse_json_texture_view(options, tokens, i + 1, json_chunk, &out_sheen->sheen_roughness_texture);
		}
		else
		{
			i = assetdoc_skip_json(tokens, i+1);
		}

		if (i < 0)
		{
			return i;
		}
	}

	return i;
}

static int assetdoc_parse_json_emissive_strength(jsmntok_t const* tokens, int i, const uint8_t* json_chunk, assetdoc_emissive_strength* out_emissive_strength)
{
	ASSETDOC_CHECK_TOKTYPE(tokens[i], JSMN_OBJECT);
	int size = tokens[i].size;
	++i;

	// Default
	out_emissive_strength->emissive_strength = 1.f;

	for (int j = 0; j < size; ++j)
	{
		ASSETDOC_CHECK_KEY(tokens[i]);

		if (assetdoc_json_strcmp(tokens + i, json_chunk, "emissiveStrength") == 0)
		{
			++i;
			out_emissive_strength->emissive_strength = assetdoc_json_to_float(tokens + i, json_chunk);
			++i;
		}
		else
		{
			i = assetdoc_skip_json(tokens, i + 1);
		}

		if (i < 0)
		{
			return i;
		}
	}

	return i;
}

static int assetdoc_parse_json_iridescence(assetdoc_options* options, jsmntok_t const* tokens, int i, const uint8_t* json_chunk, assetdoc_iridescence* out_iridescence)
{
	ASSETDOC_CHECK_TOKTYPE(tokens[i], JSMN_OBJECT);
	int size = tokens[i].size;
	++i;

	// Default
	out_iridescence->iridescence_ior = 1.3f;
	out_iridescence->iridescence_thickness_min = 100.f;
	out_iridescence->iridescence_thickness_max = 400.f;

	for (int j = 0; j < size; ++j)
	{
		ASSETDOC_CHECK_KEY(tokens[i]);

		if (assetdoc_json_strcmp(tokens + i, json_chunk, "iridescenceFactor") == 0)
		{
			++i;
			out_iridescence->iridescence_factor = assetdoc_json_to_float(tokens + i, json_chunk);
			++i;
		}
		else if (assetdoc_json_strcmp(tokens + i, json_chunk, "iridescenceTexture") == 0)
		{
			i = assetdoc_parse_json_texture_view(options, tokens, i + 1, json_chunk, &out_iridescence->iridescence_texture);
		}
		else if (assetdoc_json_strcmp(tokens + i, json_chunk, "iridescenceIor") == 0)
		{
			++i;
			out_iridescence->iridescence_ior = assetdoc_json_to_float(tokens + i, json_chunk);
			++i;
		}
		else if (assetdoc_json_strcmp(tokens + i, json_chunk, "iridescenceThicknessMinimum") == 0)
		{
			++i;
			out_iridescence->iridescence_thickness_min = assetdoc_json_to_float(tokens + i, json_chunk);
			++i;
		}
		else if (assetdoc_json_strcmp(tokens + i, json_chunk, "iridescenceThicknessMaximum") == 0)
		{
			++i;
			out_iridescence->iridescence_thickness_max = assetdoc_json_to_float(tokens + i, json_chunk);
			++i;
		}
		else if (assetdoc_json_strcmp(tokens + i, json_chunk, "iridescenceThicknessTexture") == 0)
		{
			i = assetdoc_parse_json_texture_view(options, tokens, i + 1, json_chunk, &out_iridescence->iridescence_thickness_texture);
		}
		else
		{
			i = assetdoc_skip_json(tokens, i + 1);
		}

		if (i < 0)
		{
			return i;
		}
	}

	return i;
}

static int assetdoc_parse_json_diffuse_transmission(assetdoc_options* options, jsmntok_t const* tokens, int i, const uint8_t* json_chunk, assetdoc_diffuse_transmission* out_diff_transmission)
{
	ASSETDOC_CHECK_TOKTYPE(tokens[i], JSMN_OBJECT);
	int size = tokens[i].size;
	++i;

	// Defaults
	assetdoc_fill_float_array(out_diff_transmission->diffuse_transmission_color_factor, 3, 1.0f);
	out_diff_transmission->diffuse_transmission_factor = 0.f;
	
	for (int j = 0; j < size; ++j)
	{
		ASSETDOC_CHECK_KEY(tokens[i]);

		if (assetdoc_json_strcmp(tokens + i, json_chunk, "diffuseTransmissionFactor") == 0)
		{
			++i;
			out_diff_transmission->diffuse_transmission_factor = assetdoc_json_to_float(tokens + i, json_chunk);
			++i;
		}
		else if (assetdoc_json_strcmp(tokens + i, json_chunk, "diffuseTransmissionTexture") == 0)
		{
			i = assetdoc_parse_json_texture_view(options, tokens, i + 1, json_chunk, &out_diff_transmission->diffuse_transmission_texture);
		}
		else if (assetdoc_json_strcmp(tokens + i, json_chunk, "diffuseTransmissionColorFactor") == 0)
		{
			i = assetdoc_parse_json_float_array(tokens, i + 1, json_chunk, out_diff_transmission->diffuse_transmission_color_factor, 3);
		}
		else if (assetdoc_json_strcmp(tokens + i, json_chunk, "diffuseTransmissionColorTexture") == 0)
		{
			i = assetdoc_parse_json_texture_view(options, tokens, i + 1, json_chunk, &out_diff_transmission->diffuse_transmission_color_texture);
		}
		else
		{
			i = assetdoc_skip_json(tokens, i + 1);
		}

		if (i < 0)
		{
			return i;
		}
	}

	return i;
}

static int assetdoc_parse_json_anisotropy(assetdoc_options* options, jsmntok_t const* tokens, int i, const uint8_t* json_chunk, assetdoc_anisotropy* out_anisotropy)
{
	ASSETDOC_CHECK_TOKTYPE(tokens[i], JSMN_OBJECT);
	int size = tokens[i].size;
	++i;


	for (int j = 0; j < size; ++j)
	{
		ASSETDOC_CHECK_KEY(tokens[i]);

		if (assetdoc_json_strcmp(tokens + i, json_chunk, "anisotropyStrength") == 0)
		{
			++i;
			out_anisotropy->anisotropy_strength = assetdoc_json_to_float(tokens + i, json_chunk);
			++i;
		}
		else if (assetdoc_json_strcmp(tokens + i, json_chunk, "anisotropyRotation") == 0)
		{
			++i;
			out_anisotropy->anisotropy_rotation = assetdoc_json_to_float(tokens + i, json_chunk);
			++i;
		}
		else if (assetdoc_json_strcmp(tokens + i, json_chunk, "anisotropyTexture") == 0)
		{
			i = assetdoc_parse_json_texture_view(options, tokens, i + 1, json_chunk, &out_anisotropy->anisotropy_texture);
		}
		else
		{
			i = assetdoc_skip_json(tokens, i + 1);
		}

		if (i < 0)
		{
			return i;
		}
	}

	return i;
}

static int assetdoc_parse_json_dispersion(jsmntok_t const* tokens, int i, const uint8_t* json_chunk, assetdoc_dispersion* out_dispersion)
{
	ASSETDOC_CHECK_TOKTYPE(tokens[i], JSMN_OBJECT);
	int size = tokens[i].size;
	++i;


	for (int j = 0; j < size; ++j)
	{
		ASSETDOC_CHECK_KEY(tokens[i]);

		if (assetdoc_json_strcmp(tokens + i, json_chunk, "dispersion") == 0)
		{
			++i;
			out_dispersion->dispersion = assetdoc_json_to_float(tokens + i, json_chunk);
			++i;
		}
		else
		{
			i = assetdoc_skip_json(tokens, i + 1);
		}

		if (i < 0)
		{
			return i;
		}
	}

	return i;
}

static int assetdoc_parse_json_image(assetdoc_options* options, jsmntok_t const* tokens, int i, const uint8_t* json_chunk, assetdoc_image* out_image)
{
	ASSETDOC_CHECK_TOKTYPE(tokens[i], JSMN_OBJECT);

	int size = tokens[i].size;
	++i;

	for (int j = 0; j < size; ++j)
	{
		ASSETDOC_CHECK_KEY(tokens[i]);

		if (assetdoc_json_strcmp(tokens + i, json_chunk, "uri") == 0)
		{
			i = assetdoc_parse_json_string(options, tokens, i + 1, json_chunk, &out_image->uri);
		}
		else if (assetdoc_json_strcmp(tokens+i, json_chunk, "bufferView") == 0)
		{
			++i;
			out_image->buffer_view = ASSETDOC_PTRINDEX(assetdoc_buffer_view, assetdoc_json_to_int(tokens + i, json_chunk));
			++i;
		}
		else if (assetdoc_json_strcmp(tokens + i, json_chunk, "mimeType") == 0)
		{
			i = assetdoc_parse_json_string(options, tokens, i + 1, json_chunk, &out_image->mime_type);
		}
		else if (assetdoc_json_strcmp(tokens + i, json_chunk, "name") == 0)
		{
			i = assetdoc_parse_json_string(options, tokens, i + 1, json_chunk, &out_image->name);
		}
		else if (assetdoc_json_strcmp(tokens + i, json_chunk, "extras") == 0)
		{
			i = assetdoc_parse_json_extras(options, tokens, i + 1, json_chunk, &out_image->extras);
		}
		else if (assetdoc_json_strcmp(tokens + i, json_chunk, "extensions") == 0)
		{
			i = assetdoc_parse_json_unprocessed_extensions(options, tokens, i, json_chunk, &out_image->extensions_count, &out_image->extensions);
		}
		else
		{
			i = assetdoc_skip_json(tokens, i + 1);
		}

		if (i < 0)
		{
			return i;
		}
	}

	return i;
}

static int assetdoc_parse_json_sampler(assetdoc_options* options, jsmntok_t const* tokens, int i, const uint8_t* json_chunk, assetdoc_sampler* out_sampler)
{
	(void)options;
	ASSETDOC_CHECK_TOKTYPE(tokens[i], JSMN_OBJECT);

	out_sampler->wrap_s = assetdoc_wrap_mode_repeat;
	out_sampler->wrap_t = assetdoc_wrap_mode_repeat;

	int size = tokens[i].size;
	++i;

	for (int j = 0; j < size; ++j)
	{
		ASSETDOC_CHECK_KEY(tokens[i]);

		if (assetdoc_json_strcmp(tokens + i, json_chunk, "name") == 0)
		{
			i = assetdoc_parse_json_string(options, tokens, i + 1, json_chunk, &out_sampler->name);
		}
		else if (assetdoc_json_strcmp(tokens + i, json_chunk, "magFilter") == 0)
		{
			++i;
			out_sampler->mag_filter
				= (assetdoc_filter_type)assetdoc_json_to_int(tokens + i, json_chunk);
			++i;
		}
		else if (assetdoc_json_strcmp(tokens + i, json_chunk, "minFilter") == 0)
		{
			++i;
			out_sampler->min_filter
				= (assetdoc_filter_type)assetdoc_json_to_int(tokens + i, json_chunk);
			++i;
		}
		else if (assetdoc_json_strcmp(tokens + i, json_chunk, "wrapS") == 0)
		{
			++i;
			out_sampler->wrap_s
				= (assetdoc_wrap_mode)assetdoc_json_to_int(tokens + i, json_chunk);
			++i;
		}
		else if (assetdoc_json_strcmp(tokens + i, json_chunk, "wrapT") == 0)
		{
			++i;
			out_sampler->wrap_t
				= (assetdoc_wrap_mode)assetdoc_json_to_int(tokens + i, json_chunk);
			++i;
		}
		else if (assetdoc_json_strcmp(tokens + i, json_chunk, "extras") == 0)
		{
			i = assetdoc_parse_json_extras(options, tokens, i + 1, json_chunk, &out_sampler->extras);
		}
		else if (assetdoc_json_strcmp(tokens + i, json_chunk, "extensions") == 0)
		{
			i = assetdoc_parse_json_unprocessed_extensions(options, tokens, i, json_chunk, &out_sampler->extensions_count, &out_sampler->extensions);
		}
		else
		{
			i = assetdoc_skip_json(tokens, i + 1);
		}

		if (i < 0)
		{
			return i;
		}
	}

	return i;
}

static int assetdoc_parse_json_texture(assetdoc_options* options, jsmntok_t const* tokens, int i, const uint8_t* json_chunk, assetdoc_texture* out_texture)
{
	ASSETDOC_CHECK_TOKTYPE(tokens[i], JSMN_OBJECT);

	int size = tokens[i].size;
	++i;

	for (int j = 0; j < size; ++j)
	{
		ASSETDOC_CHECK_KEY(tokens[i]);

		if (assetdoc_json_strcmp(tokens+i, json_chunk, "name") == 0)
		{
			i = assetdoc_parse_json_string(options, tokens, i + 1, json_chunk, &out_texture->name);
		}
		else if (assetdoc_json_strcmp(tokens + i, json_chunk, "sampler") == 0)
		{
			++i;
			out_texture->sampler = ASSETDOC_PTRINDEX(assetdoc_sampler, assetdoc_json_to_int(tokens + i, json_chunk));
			++i;
		}
		else if (assetdoc_json_strcmp(tokens + i, json_chunk, "source") == 0)
		{
			++i;
			out_texture->image = ASSETDOC_PTRINDEX(assetdoc_image, assetdoc_json_to_int(tokens + i, json_chunk));
			++i;
		}
		else if (assetdoc_json_strcmp(tokens + i, json_chunk, "extras") == 0)
		{
			i = assetdoc_parse_json_extras(options, tokens, i + 1, json_chunk, &out_texture->extras);
		}
		else if (assetdoc_json_strcmp(tokens + i, json_chunk, "extensions") == 0)
		{
			++i;

			ASSETDOC_CHECK_TOKTYPE(tokens[i], JSMN_OBJECT);
			if (out_texture->extensions)
			{
				return ASSETDOC_ERROR_JSON;
			}

			int extensions_size = tokens[i].size;
			++i;
			out_texture->extensions = (assetdoc_extension*)assetdoc_calloc(options, sizeof(assetdoc_extension), extensions_size);
			out_texture->extensions_count = 0;

			if (!out_texture->extensions)
			{
				return ASSETDOC_ERROR_NOMEM;
			}

			for (int k = 0; k < extensions_size; ++k)
			{
				ASSETDOC_CHECK_KEY(tokens[i]);

				if (assetdoc_json_strcmp(tokens + i, json_chunk, "KHR_texture_basisu") == 0)
				{
					out_texture->has_basisu = 1;
					++i;
					ASSETDOC_CHECK_TOKTYPE(tokens[i], JSMN_OBJECT);
					int num_properties = tokens[i].size;
					++i;

					for (int t = 0; t < num_properties; ++t)
					{
						ASSETDOC_CHECK_KEY(tokens[i]);

						if (assetdoc_json_strcmp(tokens + i, json_chunk, "source") == 0)
						{
							++i;
							out_texture->basisu_image = ASSETDOC_PTRINDEX(assetdoc_image, assetdoc_json_to_int(tokens + i, json_chunk));
							++i;
						}
						else
						{
							i = assetdoc_skip_json(tokens, i + 1);
						}
						if (i < 0)
						{
							return i;
						}
					}
				}
				else if (assetdoc_json_strcmp(tokens + i, json_chunk, "EXT_texture_webp") == 0)
				{
					out_texture->has_webp = 1;
					++i;
					ASSETDOC_CHECK_TOKTYPE(tokens[i], JSMN_OBJECT);
					int num_properties = tokens[i].size;
					++i;

					for (int t = 0; t < num_properties; ++t)
					{
						ASSETDOC_CHECK_KEY(tokens[i]);

						if (assetdoc_json_strcmp(tokens + i, json_chunk, "source") == 0)
						{
							++i;
							out_texture->webp_image = ASSETDOC_PTRINDEX(assetdoc_image, assetdoc_json_to_int(tokens + i, json_chunk));
							++i;
						}
						else
						{
							i = assetdoc_skip_json(tokens, i + 1);
						}
						if (i < 0)
						{
							return i;
						}
					}
				}
				else
				{
					i = assetdoc_parse_json_unprocessed_extension(options, tokens, i, json_chunk, &(out_texture->extensions[out_texture->extensions_count++]));
				}

				if (i < 0)
				{
					return i;
				}
			}
		}
		else
		{
			i = assetdoc_skip_json(tokens, i + 1);
		}

		if (i < 0)
		{
			return i;
		}
	}

	return i;
}

static int assetdoc_parse_json_material(assetdoc_options* options, jsmntok_t const* tokens, int i, const uint8_t* json_chunk, assetdoc_material* out_material)
{
	ASSETDOC_CHECK_TOKTYPE(tokens[i], JSMN_OBJECT);

	assetdoc_fill_float_array(out_material->pbr_metallic_roughness.base_color_factor, 4, 1.0f);
	out_material->pbr_metallic_roughness.metallic_factor = 1.0f;
	out_material->pbr_metallic_roughness.roughness_factor = 1.0f;

	assetdoc_fill_float_array(out_material->pbr_specular_glossiness.diffuse_factor, 4, 1.0f);
	assetdoc_fill_float_array(out_material->pbr_specular_glossiness.specular_factor, 3, 1.0f);
	out_material->pbr_specular_glossiness.glossiness_factor = 1.0f;

	assetdoc_fill_float_array(out_material->volume.attenuation_color, 3, 1.0f);
	out_material->volume.attenuation_distance = FLT_MAX;

	out_material->alpha_cutoff = 0.5f;

	int size = tokens[i].size;
	++i;

	for (int j = 0; j < size; ++j)
	{
		ASSETDOC_CHECK_KEY(tokens[i]);

		if (assetdoc_json_strcmp(tokens+i, json_chunk, "name") == 0)
		{
			i = assetdoc_parse_json_string(options, tokens, i + 1, json_chunk, &out_material->name);
		}
		else if (assetdoc_json_strcmp(tokens+i, json_chunk, "pbrMetallicRoughness") == 0)
		{
			out_material->has_pbr_metallic_roughness = 1;
			i = assetdoc_parse_json_pbr_metallic_roughness(options, tokens, i + 1, json_chunk, &out_material->pbr_metallic_roughness);
		}
		else if (assetdoc_json_strcmp(tokens+i, json_chunk, "emissiveFactor") == 0)
		{
			i = assetdoc_parse_json_float_array(tokens, i + 1, json_chunk, out_material->emissive_factor, 3);
		}
		else if (assetdoc_json_strcmp(tokens + i, json_chunk, "normalTexture") == 0)
		{
			i = assetdoc_parse_json_texture_view(options, tokens, i + 1, json_chunk,
				&out_material->normal_texture);
		}
		else if (assetdoc_json_strcmp(tokens + i, json_chunk, "occlusionTexture") == 0)
		{
			i = assetdoc_parse_json_texture_view(options, tokens, i + 1, json_chunk,
				&out_material->occlusion_texture);
		}
		else if (assetdoc_json_strcmp(tokens + i, json_chunk, "emissiveTexture") == 0)
		{
			i = assetdoc_parse_json_texture_view(options, tokens, i + 1, json_chunk,
				&out_material->emissive_texture);
		}
		else if (assetdoc_json_strcmp(tokens + i, json_chunk, "alphaMode") == 0)
		{
			++i;
			if (assetdoc_json_strcmp(tokens + i, json_chunk, "OPAQUE") == 0)
			{
				out_material->alpha_mode = assetdoc_alpha_mode_opaque;
			}
			else if (assetdoc_json_strcmp(tokens + i, json_chunk, "MASK") == 0)
			{
				out_material->alpha_mode = assetdoc_alpha_mode_mask;
			}
			else if (assetdoc_json_strcmp(tokens + i, json_chunk, "BLEND") == 0)
			{
				out_material->alpha_mode = assetdoc_alpha_mode_blend;
			}
			++i;
		}
		else if (assetdoc_json_strcmp(tokens + i, json_chunk, "alphaCutoff") == 0)
		{
			++i;
			out_material->alpha_cutoff = assetdoc_json_to_float(tokens + i, json_chunk);
			++i;
		}
		else if (assetdoc_json_strcmp(tokens + i, json_chunk, "doubleSided") == 0)
		{
			++i;
			out_material->double_sided =
				assetdoc_json_to_bool(tokens + i, json_chunk);
			++i;
		}
		else if (assetdoc_json_strcmp(tokens + i, json_chunk, "extras") == 0)
		{
			i = assetdoc_parse_json_extras(options, tokens, i + 1, json_chunk, &out_material->extras);
		}
		else if (assetdoc_json_strcmp(tokens + i, json_chunk, "extensions") == 0)
		{
			++i;

			ASSETDOC_CHECK_TOKTYPE(tokens[i], JSMN_OBJECT);
			if(out_material->extensions)
			{
				return ASSETDOC_ERROR_JSON;
			}

			int extensions_size = tokens[i].size;
			++i;
			out_material->extensions = (assetdoc_extension*)assetdoc_calloc(options, sizeof(assetdoc_extension), extensions_size);
			out_material->extensions_count= 0;

			if (!out_material->extensions)
			{
				return ASSETDOC_ERROR_NOMEM;
			}

			for (int k = 0; k < extensions_size; ++k)
			{
				ASSETDOC_CHECK_KEY(tokens[i]);

				if (assetdoc_json_strcmp(tokens+i, json_chunk, "KHR_materials_pbrSpecularGlossiness") == 0)
				{
					out_material->has_pbr_specular_glossiness = 1;
					i = assetdoc_parse_json_pbr_specular_glossiness(options, tokens, i + 1, json_chunk, &out_material->pbr_specular_glossiness);
				}
				else if (assetdoc_json_strcmp(tokens+i, json_chunk, "KHR_materials_unlit") == 0)
				{
					out_material->unlit = 1;
					i = assetdoc_skip_json(tokens, i+1);
				}
				else if (assetdoc_json_strcmp(tokens+i, json_chunk, "KHR_materials_clearcoat") == 0)
				{
					out_material->has_clearcoat = 1;
					i = assetdoc_parse_json_clearcoat(options, tokens, i + 1, json_chunk, &out_material->clearcoat);
				}
				else if (assetdoc_json_strcmp(tokens+i, json_chunk, "KHR_materials_ior") == 0)
				{
					out_material->has_ior = 1;
					i = assetdoc_parse_json_ior(tokens, i + 1, json_chunk, &out_material->ior);
				}
				else if (assetdoc_json_strcmp(tokens+i, json_chunk, "KHR_materials_specular") == 0)
				{
					out_material->has_specular = 1;
					i = assetdoc_parse_json_specular(options, tokens, i + 1, json_chunk, &out_material->specular);
				}
				else if (assetdoc_json_strcmp(tokens+i, json_chunk, "KHR_materials_transmission") == 0)
				{
					out_material->has_transmission = 1;
					i = assetdoc_parse_json_transmission(options, tokens, i + 1, json_chunk, &out_material->transmission);
				}
				else if (assetdoc_json_strcmp(tokens + i, json_chunk, "KHR_materials_volume") == 0)
				{
					out_material->has_volume = 1;
					i = assetdoc_parse_json_volume(options, tokens, i + 1, json_chunk, &out_material->volume);
				}
				else if (assetdoc_json_strcmp(tokens+i, json_chunk, "KHR_materials_sheen") == 0)
				{
					out_material->has_sheen = 1;
					i = assetdoc_parse_json_sheen(options, tokens, i + 1, json_chunk, &out_material->sheen);
				}
				else if (assetdoc_json_strcmp(tokens + i, json_chunk, "KHR_materials_emissive_strength") == 0)
				{
					out_material->has_emissive_strength = 1;
					i = assetdoc_parse_json_emissive_strength(tokens, i + 1, json_chunk, &out_material->emissive_strength);
				}
				else if (assetdoc_json_strcmp(tokens + i, json_chunk, "KHR_materials_iridescence") == 0)
				{
					out_material->has_iridescence = 1;
					i = assetdoc_parse_json_iridescence(options, tokens, i + 1, json_chunk, &out_material->iridescence);
				}
				else if (assetdoc_json_strcmp(tokens + i, json_chunk, "KHR_materials_diffuse_transmission") == 0)
				{
					out_material->has_diffuse_transmission = 1;
					i = assetdoc_parse_json_diffuse_transmission(options, tokens, i + 1, json_chunk, &out_material->diffuse_transmission);
				}
				else if (assetdoc_json_strcmp(tokens + i, json_chunk, "KHR_materials_anisotropy") == 0)
				{
					out_material->has_anisotropy = 1;
					i = assetdoc_parse_json_anisotropy(options, tokens, i + 1, json_chunk, &out_material->anisotropy);
				}
				else if (assetdoc_json_strcmp(tokens + i, json_chunk, "KHR_materials_dispersion") == 0)
				{
					out_material->has_dispersion = 1;
					i = assetdoc_parse_json_dispersion(tokens, i + 1, json_chunk, &out_material->dispersion);
				}
				else
				{
					i = assetdoc_parse_json_unprocessed_extension(options, tokens, i, json_chunk, &(out_material->extensions[out_material->extensions_count++]));
				}

				if (i < 0)
				{
					return i;
				}
			}
		}
		else
		{
			i = assetdoc_skip_json(tokens, i+1);
		}

		if (i < 0)
		{
			return i;
		}
	}

	return i;
}

static int assetdoc_parse_json_accessors(assetdoc_options* options, jsmntok_t const* tokens, int i, const uint8_t* json_chunk, assetdoc_data* out_data)
{
	i = assetdoc_parse_json_array(options, tokens, i, json_chunk, sizeof(assetdoc_accessor), (void**)&out_data->accessors, &out_data->accessors_count);
	if (i < 0)
	{
		return i;
	}

	for (assetdoc_size j = 0; j < out_data->accessors_count; ++j)
	{
		i = assetdoc_parse_json_accessor(options, tokens, i, json_chunk, &out_data->accessors[j]);
		if (i < 0)
		{
			return i;
		}
	}
	return i;
}

static int assetdoc_parse_json_materials(assetdoc_options* options, jsmntok_t const* tokens, int i, const uint8_t* json_chunk, assetdoc_data* out_data)
{
	i = assetdoc_parse_json_array(options, tokens, i, json_chunk, sizeof(assetdoc_material), (void**)&out_data->materials, &out_data->materials_count);
	if (i < 0)
	{
		return i;
	}

	for (assetdoc_size j = 0; j < out_data->materials_count; ++j)
	{
		i = assetdoc_parse_json_material(options, tokens, i, json_chunk, &out_data->materials[j]);
		if (i < 0)
		{
			return i;
		}
	}
	return i;
}

static int assetdoc_parse_json_images(assetdoc_options* options, jsmntok_t const* tokens, int i, const uint8_t* json_chunk, assetdoc_data* out_data)
{
	i = assetdoc_parse_json_array(options, tokens, i, json_chunk, sizeof(assetdoc_image), (void**)&out_data->images, &out_data->images_count);
	if (i < 0)
	{
		return i;
	}

	for (assetdoc_size j = 0; j < out_data->images_count; ++j)
	{
		i = assetdoc_parse_json_image(options, tokens, i, json_chunk, &out_data->images[j]);
		if (i < 0)
		{
			return i;
		}
	}
	return i;
}

static int assetdoc_parse_json_textures(assetdoc_options* options, jsmntok_t const* tokens, int i, const uint8_t* json_chunk, assetdoc_data* out_data)
{
	i = assetdoc_parse_json_array(options, tokens, i, json_chunk, sizeof(assetdoc_texture), (void**)&out_data->textures, &out_data->textures_count);
	if (i < 0)
	{
		return i;
	}

	for (assetdoc_size j = 0; j < out_data->textures_count; ++j)
	{
		i = assetdoc_parse_json_texture(options, tokens, i, json_chunk, &out_data->textures[j]);
		if (i < 0)
		{
			return i;
		}
	}
	return i;
}

static int assetdoc_parse_json_samplers(assetdoc_options* options, jsmntok_t const* tokens, int i, const uint8_t* json_chunk, assetdoc_data* out_data)
{
	i = assetdoc_parse_json_array(options, tokens, i, json_chunk, sizeof(assetdoc_sampler), (void**)&out_data->samplers, &out_data->samplers_count);
	if (i < 0)
	{
		return i;
	}

	for (assetdoc_size j = 0; j < out_data->samplers_count; ++j)
	{
		i = assetdoc_parse_json_sampler(options, tokens, i, json_chunk, &out_data->samplers[j]);
		if (i < 0)
		{
			return i;
		}
	}
	return i;
}

static int assetdoc_parse_json_meshwork_compression(assetdoc_options* options, jsmntok_t const* tokens, int i, const uint8_t* json_chunk, assetdoc_meshwork_compression* out_meshwork_compression)
{
	(void)options;
	ASSETDOC_CHECK_TOKTYPE(tokens[i], JSMN_OBJECT);

	int size = tokens[i].size;
	++i;

	for (int j = 0; j < size; ++j)
	{
		ASSETDOC_CHECK_KEY(tokens[i]);

		if (assetdoc_json_strcmp(tokens+i, json_chunk, "buffer") == 0)
		{
			++i;
			out_meshwork_compression->buffer = ASSETDOC_PTRINDEX(assetdoc_buffer, assetdoc_json_to_int(tokens + i, json_chunk));
			++i;
		}
		else if (assetdoc_json_strcmp(tokens+i, json_chunk, "byteOffset") == 0)
		{
			++i;
			out_meshwork_compression->offset = assetdoc_json_to_size(tokens+i, json_chunk);
			++i;
		}
		else if (assetdoc_json_strcmp(tokens+i, json_chunk, "byteLength") == 0)
		{
			++i;
			out_meshwork_compression->size = assetdoc_json_to_size(tokens+i, json_chunk);
			++i;
		}
		else if (assetdoc_json_strcmp(tokens+i, json_chunk, "byteStride") == 0)
		{
			++i;
			out_meshwork_compression->stride = assetdoc_json_to_size(tokens+i, json_chunk);
			++i;
		}
		else if (assetdoc_json_strcmp(tokens+i, json_chunk, "count") == 0)
		{
			++i;
			out_meshwork_compression->count = assetdoc_json_to_size(tokens+i, json_chunk);
			++i;
		}
		else if (assetdoc_json_strcmp(tokens+i, json_chunk, "mode") == 0)
		{
			++i;
			if (assetdoc_json_strcmp(tokens+i, json_chunk, "ATTRIBUTES") == 0)
			{
				out_meshwork_compression->mode = assetdoc_meshwork_compression_mode_attributes;
			}
			else if (assetdoc_json_strcmp(tokens+i, json_chunk, "TRIANGLES") == 0)
			{
				out_meshwork_compression->mode = assetdoc_meshwork_compression_mode_triangles;
			}
			else if (assetdoc_json_strcmp(tokens+i, json_chunk, "INDICES") == 0)
			{
				out_meshwork_compression->mode = assetdoc_meshwork_compression_mode_indices;
			}
			++i;
		}
		else if (assetdoc_json_strcmp(tokens+i, json_chunk, "filter") == 0)
		{
			++i;
			if (assetdoc_json_strcmp(tokens+i, json_chunk, "NONE") == 0)
			{
				out_meshwork_compression->filter = assetdoc_meshwork_compression_filter_none;
			}
			else if (assetdoc_json_strcmp(tokens+i, json_chunk, "OCTAHEDRAL") == 0)
			{
				out_meshwork_compression->filter = assetdoc_meshwork_compression_filter_octahedral;
			}
			else if (assetdoc_json_strcmp(tokens+i, json_chunk, "QUATERNION") == 0)
			{
				out_meshwork_compression->filter = assetdoc_meshwork_compression_filter_quaternion;
			}
			else if (assetdoc_json_strcmp(tokens+i, json_chunk, "EXPONENTIAL") == 0)
			{
				out_meshwork_compression->filter = assetdoc_meshwork_compression_filter_exponential;
			}
			else if (assetdoc_json_strcmp(tokens+i, json_chunk, "COLOR") == 0)
			{
				out_meshwork_compression->filter = assetdoc_meshwork_compression_filter_color;
			}
			++i;
		}
		else
		{
			i = assetdoc_skip_json(tokens, i+1);
		}

		if (i < 0)
		{
			return i;
		}
	}

	return i;
}

static int assetdoc_parse_json_buffer_view(assetdoc_options* options, jsmntok_t const* tokens, int i, const uint8_t* json_chunk, assetdoc_buffer_view* out_buffer_view)
{
	ASSETDOC_CHECK_TOKTYPE(tokens[i], JSMN_OBJECT);

	int size = tokens[i].size;
	++i;

	for (int j = 0; j < size; ++j)
	{
		ASSETDOC_CHECK_KEY(tokens[i]);

		if (assetdoc_json_strcmp(tokens + i, json_chunk, "name") == 0)
		{
			i = assetdoc_parse_json_string(options, tokens, i + 1, json_chunk, &out_buffer_view->name);
		}
		else if (assetdoc_json_strcmp(tokens+i, json_chunk, "buffer") == 0)
		{
			++i;
			out_buffer_view->buffer = ASSETDOC_PTRINDEX(assetdoc_buffer, assetdoc_json_to_int(tokens + i, json_chunk));
			++i;
		}
		else if (assetdoc_json_strcmp(tokens+i, json_chunk, "byteOffset") == 0)
		{
			++i;
			out_buffer_view->offset =
					assetdoc_json_to_size(tokens+i, json_chunk);
			++i;
		}
		else if (assetdoc_json_strcmp(tokens+i, json_chunk, "byteLength") == 0)
		{
			++i;
			out_buffer_view->size =
					assetdoc_json_to_size(tokens+i, json_chunk);
			++i;
		}
		else if (assetdoc_json_strcmp(tokens+i, json_chunk, "byteStride") == 0)
		{
			++i;
			out_buffer_view->stride =
					assetdoc_json_to_size(tokens+i, json_chunk);
			++i;
		}
		else if (assetdoc_json_strcmp(tokens+i, json_chunk, "target") == 0)
		{
			++i;
			int type = assetdoc_json_to_int(tokens+i, json_chunk);
			switch (type)
			{
			case 34962:
				type = assetdoc_buffer_view_type_vertices;
				break;
			case 34963:
				type = assetdoc_buffer_view_type_indices;
				break;
			default:
				type = assetdoc_buffer_view_type_invalid;
				break;
			}
			out_buffer_view->type = (assetdoc_buffer_view_type)type;
			++i;
		}
		else if (assetdoc_json_strcmp(tokens + i, json_chunk, "extras") == 0)
		{
			i = assetdoc_parse_json_extras(options, tokens, i + 1, json_chunk, &out_buffer_view->extras);
		}
		else if (assetdoc_json_strcmp(tokens + i, json_chunk, "extensions") == 0)
		{
			++i;

			ASSETDOC_CHECK_TOKTYPE(tokens[i], JSMN_OBJECT);
			if(out_buffer_view->extensions)
			{
				return ASSETDOC_ERROR_JSON;
			}

			int extensions_size = tokens[i].size;
			out_buffer_view->extensions_count = 0;
			out_buffer_view->extensions = (assetdoc_extension*)assetdoc_calloc(options, sizeof(assetdoc_extension), extensions_size);

			if (!out_buffer_view->extensions)
			{
				return ASSETDOC_ERROR_NOMEM;
			}

			++i;
			for (int k = 0; k < extensions_size; ++k)
			{
				ASSETDOC_CHECK_KEY(tokens[i]);

				if (assetdoc_json_strcmp(tokens+i, json_chunk, "EXT_meshwork_compression") == 0)
				{
					out_buffer_view->has_meshwork_compression = 1;
					i = assetdoc_parse_json_meshwork_compression(options, tokens, i + 1, json_chunk, &out_buffer_view->meshwork_compression);
				}
				else if (assetdoc_json_strcmp(tokens+i, json_chunk, "KHR_meshwork_compression") == 0)
				{
					out_buffer_view->has_meshwork_compression = 1;
					out_buffer_view->meshwork_compression.is_khr = 1;
					i = assetdoc_parse_json_meshwork_compression(options, tokens, i + 1, json_chunk, &out_buffer_view->meshwork_compression);
				}
				else
				{
					i = assetdoc_parse_json_unprocessed_extension(options, tokens, i, json_chunk, &(out_buffer_view->extensions[out_buffer_view->extensions_count++]));
				}

				if (i < 0)
				{
					return i;
				}
			}
		}
		else
		{
			i = assetdoc_skip_json(tokens, i+1);
		}

		if (i < 0)
		{
			return i;
		}
	}

	return i;
}

static int assetdoc_parse_json_buffer_views(assetdoc_options* options, jsmntok_t const* tokens, int i, const uint8_t* json_chunk, assetdoc_data* out_data)
{
	i = assetdoc_parse_json_array(options, tokens, i, json_chunk, sizeof(assetdoc_buffer_view), (void**)&out_data->buffer_views, &out_data->buffer_views_count);
	if (i < 0)
	{
		return i;
	}

	for (assetdoc_size j = 0; j < out_data->buffer_views_count; ++j)
	{
		i = assetdoc_parse_json_buffer_view(options, tokens, i, json_chunk, &out_data->buffer_views[j]);
		if (i < 0)
		{
			return i;
		}
	}
	return i;
}

static int assetdoc_parse_json_buffer(assetdoc_options* options, jsmntok_t const* tokens, int i, const uint8_t* json_chunk, assetdoc_buffer* out_buffer)
{
	ASSETDOC_CHECK_TOKTYPE(tokens[i], JSMN_OBJECT);

	int size = tokens[i].size;
	++i;

	for (int j = 0; j < size; ++j)
	{
		ASSETDOC_CHECK_KEY(tokens[i]);

		if (assetdoc_json_strcmp(tokens + i, json_chunk, "name") == 0)
		{
			i = assetdoc_parse_json_string(options, tokens, i + 1, json_chunk, &out_buffer->name);
		}
		else if (assetdoc_json_strcmp(tokens+i, json_chunk, "byteLength") == 0)
		{
			++i;
			out_buffer->size =
					assetdoc_json_to_size(tokens+i, json_chunk);
			++i;
		}
		else if (assetdoc_json_strcmp(tokens+i, json_chunk, "uri") == 0)
		{
			i = assetdoc_parse_json_string(options, tokens, i + 1, json_chunk, &out_buffer->uri);
		}
		else if (assetdoc_json_strcmp(tokens + i, json_chunk, "extras") == 0)
		{
			i = assetdoc_parse_json_extras(options, tokens, i + 1, json_chunk, &out_buffer->extras);
		}
		else if (assetdoc_json_strcmp(tokens + i, json_chunk, "extensions") == 0)
		{
			i = assetdoc_parse_json_unprocessed_extensions(options, tokens, i, json_chunk, &out_buffer->extensions_count, &out_buffer->extensions);
		}
		else
		{
			i = assetdoc_skip_json(tokens, i+1);
		}

		if (i < 0)
		{
			return i;
		}
	}

	return i;
}

static int assetdoc_parse_json_buffers(assetdoc_options* options, jsmntok_t const* tokens, int i, const uint8_t* json_chunk, assetdoc_data* out_data)
{
	i = assetdoc_parse_json_array(options, tokens, i, json_chunk, sizeof(assetdoc_buffer), (void**)&out_data->buffers, &out_data->buffers_count);
	if (i < 0)
	{
		return i;
	}

	for (assetdoc_size j = 0; j < out_data->buffers_count; ++j)
	{
		i = assetdoc_parse_json_buffer(options, tokens, i, json_chunk, &out_data->buffers[j]);
		if (i < 0)
		{
			return i;
		}
	}
	return i;
}

static int assetdoc_parse_json_skin(assetdoc_options* options, jsmntok_t const* tokens, int i, const uint8_t* json_chunk, assetdoc_skin* out_skin)
{
	ASSETDOC_CHECK_TOKTYPE(tokens[i], JSMN_OBJECT);

	int size = tokens[i].size;
	++i;

	for (int j = 0; j < size; ++j)
	{
		ASSETDOC_CHECK_KEY(tokens[i]);

		if (assetdoc_json_strcmp(tokens+i, json_chunk, "name") == 0)
		{
			i = assetdoc_parse_json_string(options, tokens, i + 1, json_chunk, &out_skin->name);
		}
		else if (assetdoc_json_strcmp(tokens+i, json_chunk, "joints") == 0)
		{
			i = assetdoc_parse_json_array(options, tokens, i + 1, json_chunk, sizeof(assetdoc_node*), (void**)&out_skin->joints, &out_skin->joints_count);
			if (i < 0)
			{
				return i;
			}

			for (assetdoc_size k = 0; k < out_skin->joints_count; ++k)
			{
				out_skin->joints[k] = ASSETDOC_PTRINDEX(assetdoc_node, assetdoc_json_to_int(tokens + i, json_chunk));
				++i;
			}
		}
		else if (assetdoc_json_strcmp(tokens+i, json_chunk, "skeleton") == 0)
		{
			++i;
			ASSETDOC_CHECK_TOKTYPE(tokens[i], JSMN_PRIMITIVE);
			out_skin->skeleton = ASSETDOC_PTRINDEX(assetdoc_node, assetdoc_json_to_int(tokens + i, json_chunk));
			++i;
		}
		else if (assetdoc_json_strcmp(tokens+i, json_chunk, "inverseBindMatrices") == 0)
		{
			++i;
			ASSETDOC_CHECK_TOKTYPE(tokens[i], JSMN_PRIMITIVE);
			out_skin->inverse_bind_matrices = ASSETDOC_PTRINDEX(assetdoc_accessor, assetdoc_json_to_int(tokens + i, json_chunk));
			++i;
		}
		else if (assetdoc_json_strcmp(tokens + i, json_chunk, "extras") == 0)
		{
			i = assetdoc_parse_json_extras(options, tokens, i + 1, json_chunk, &out_skin->extras);
		}
		else if (assetdoc_json_strcmp(tokens + i, json_chunk, "extensions") == 0)
		{
			i = assetdoc_parse_json_unprocessed_extensions(options, tokens, i, json_chunk, &out_skin->extensions_count, &out_skin->extensions);
		}
		else
		{
			i = assetdoc_skip_json(tokens, i+1);
		}

		if (i < 0)
		{
			return i;
		}
	}

	return i;
}

static int assetdoc_parse_json_skins(assetdoc_options* options, jsmntok_t const* tokens, int i, const uint8_t* json_chunk, assetdoc_data* out_data)
{
	i = assetdoc_parse_json_array(options, tokens, i, json_chunk, sizeof(assetdoc_skin), (void**)&out_data->skins, &out_data->skins_count);
	if (i < 0)
	{
		return i;
	}

	for (assetdoc_size j = 0; j < out_data->skins_count; ++j)
	{
		i = assetdoc_parse_json_skin(options, tokens, i, json_chunk, &out_data->skins[j]);
		if (i < 0)
		{
			return i;
		}
	}
	return i;
}

static int assetdoc_parse_json_camera(assetdoc_options* options, jsmntok_t const* tokens, int i, const uint8_t* json_chunk, assetdoc_camera* out_camera)
{
	ASSETDOC_CHECK_TOKTYPE(tokens[i], JSMN_OBJECT);

	int size = tokens[i].size;
	++i;

	for (int j = 0; j < size; ++j)
	{
		ASSETDOC_CHECK_KEY(tokens[i]);

		if (assetdoc_json_strcmp(tokens+i, json_chunk, "name") == 0)
		{
			i = assetdoc_parse_json_string(options, tokens, i + 1, json_chunk, &out_camera->name);
		}
		else if (assetdoc_json_strcmp(tokens+i, json_chunk, "perspective") == 0)
		{
			++i;

			ASSETDOC_CHECK_TOKTYPE(tokens[i], JSMN_OBJECT);

			int data_size = tokens[i].size;
			++i;

			if (out_camera->type != assetdoc_camera_type_invalid)
			{
				return ASSETDOC_ERROR_JSON;
			}

			out_camera->type = assetdoc_camera_type_perspective;

			for (int k = 0; k < data_size; ++k)
			{
				ASSETDOC_CHECK_KEY(tokens[i]);

				if (assetdoc_json_strcmp(tokens+i, json_chunk, "aspectRatio") == 0)
				{
					++i;
					out_camera->data.perspective.has_aspect_ratio = 1;
					out_camera->data.perspective.aspect_ratio = assetdoc_json_to_float(tokens + i, json_chunk);
					++i;
				}
				else if (assetdoc_json_strcmp(tokens+i, json_chunk, "yfov") == 0)
				{
					++i;
					out_camera->data.perspective.yfov = assetdoc_json_to_float(tokens + i, json_chunk);
					++i;
				}
				else if (assetdoc_json_strcmp(tokens+i, json_chunk, "zfar") == 0)
				{
					++i;
					out_camera->data.perspective.has_zfar = 1;
					out_camera->data.perspective.zfar = assetdoc_json_to_float(tokens + i, json_chunk);
					++i;
				}
				else if (assetdoc_json_strcmp(tokens+i, json_chunk, "znear") == 0)
				{
					++i;
					out_camera->data.perspective.znear = assetdoc_json_to_float(tokens + i, json_chunk);
					++i;
				}
				else if (assetdoc_json_strcmp(tokens + i, json_chunk, "extras") == 0)
				{
					i = assetdoc_parse_json_extras(options, tokens, i + 1, json_chunk, &out_camera->data.perspective.extras);
				}
				else
				{
					i = assetdoc_skip_json(tokens, i+1);
				}

				if (i < 0)
				{
					return i;
				}
			}
		}
		else if (assetdoc_json_strcmp(tokens+i, json_chunk, "orthographic") == 0)
		{
			++i;

			ASSETDOC_CHECK_TOKTYPE(tokens[i], JSMN_OBJECT);

			int data_size = tokens[i].size;
			++i;

			if (out_camera->type != assetdoc_camera_type_invalid)
			{
				return ASSETDOC_ERROR_JSON;
			}

			out_camera->type = assetdoc_camera_type_orthographic;

			for (int k = 0; k < data_size; ++k)
			{
				ASSETDOC_CHECK_KEY(tokens[i]);

				if (assetdoc_json_strcmp(tokens+i, json_chunk, "xmag") == 0)
				{
					++i;
					out_camera->data.orthographic.xmag = assetdoc_json_to_float(tokens + i, json_chunk);
					++i;
				}
				else if (assetdoc_json_strcmp(tokens+i, json_chunk, "ymag") == 0)
				{
					++i;
					out_camera->data.orthographic.ymag = assetdoc_json_to_float(tokens + i, json_chunk);
					++i;
				}
				else if (assetdoc_json_strcmp(tokens+i, json_chunk, "zfar") == 0)
				{
					++i;
					out_camera->data.orthographic.zfar = assetdoc_json_to_float(tokens + i, json_chunk);
					++i;
				}
				else if (assetdoc_json_strcmp(tokens+i, json_chunk, "znear") == 0)
				{
					++i;
					out_camera->data.orthographic.znear = assetdoc_json_to_float(tokens + i, json_chunk);
					++i;
				}
				else if (assetdoc_json_strcmp(tokens + i, json_chunk, "extras") == 0)
				{
					i = assetdoc_parse_json_extras(options, tokens, i + 1, json_chunk, &out_camera->data.orthographic.extras);
				}
				else
				{
					i = assetdoc_skip_json(tokens, i+1);
				}

				if (i < 0)
				{
					return i;
				}
			}
		}
		else if (assetdoc_json_strcmp(tokens + i, json_chunk, "extras") == 0)
		{
			i = assetdoc_parse_json_extras(options, tokens, i + 1, json_chunk, &out_camera->extras);
		}
		else if (assetdoc_json_strcmp(tokens + i, json_chunk, "extensions") == 0)
		{
			i = assetdoc_parse_json_unprocessed_extensions(options, tokens, i, json_chunk, &out_camera->extensions_count, &out_camera->extensions);
		}
		else
		{
			i = assetdoc_skip_json(tokens, i+1);
		}

		if (i < 0)
		{
			return i;
		}
	}

	return i;
}

static int assetdoc_parse_json_cameras(assetdoc_options* options, jsmntok_t const* tokens, int i, const uint8_t* json_chunk, assetdoc_data* out_data)
{
	i = assetdoc_parse_json_array(options, tokens, i, json_chunk, sizeof(assetdoc_camera), (void**)&out_data->cameras, &out_data->cameras_count);
	if (i < 0)
	{
		return i;
	}

	for (assetdoc_size j = 0; j < out_data->cameras_count; ++j)
	{
		i = assetdoc_parse_json_camera(options, tokens, i, json_chunk, &out_data->cameras[j]);
		if (i < 0)
		{
			return i;
		}
	}
	return i;
}

static int assetdoc_parse_json_light(assetdoc_options* options, jsmntok_t const* tokens, int i, const uint8_t* json_chunk, assetdoc_light* out_light)
{
	ASSETDOC_CHECK_TOKTYPE(tokens[i], JSMN_OBJECT);

	out_light->color[0] = 1.f;
	out_light->color[1] = 1.f;
	out_light->color[2] = 1.f;
	out_light->intensity = 1.f;

	out_light->spot_inner_cone_angle = 0.f;
	out_light->spot_outer_cone_angle = 3.1415926535f / 4.0f;

	int size = tokens[i].size;
	++i;

	for (int j = 0; j < size; ++j)
	{
		ASSETDOC_CHECK_KEY(tokens[i]);

		if (assetdoc_json_strcmp(tokens+i, json_chunk, "name") == 0)
		{
			i = assetdoc_parse_json_string(options, tokens, i + 1, json_chunk, &out_light->name);
		}
		else if (assetdoc_json_strcmp(tokens + i, json_chunk, "color") == 0)
		{
			i = assetdoc_parse_json_float_array(tokens, i + 1, json_chunk, out_light->color, 3);
		}
		else if (assetdoc_json_strcmp(tokens + i, json_chunk, "intensity") == 0)
		{
			++i;
			out_light->intensity = assetdoc_json_to_float(tokens + i, json_chunk);
			++i;
		}
		else if (assetdoc_json_strcmp(tokens+i, json_chunk, "type") == 0)
		{
			++i;
			if (assetdoc_json_strcmp(tokens + i, json_chunk, "directional") == 0)
			{
				out_light->type = assetdoc_light_type_directional;
			}
			else if (assetdoc_json_strcmp(tokens + i, json_chunk, "point") == 0)
			{
				out_light->type = assetdoc_light_type_point;
			}
			else if (assetdoc_json_strcmp(tokens + i, json_chunk, "spot") == 0)
			{
				out_light->type = assetdoc_light_type_spot;
			}
			++i;
		}
		else if (assetdoc_json_strcmp(tokens + i, json_chunk, "range") == 0)
		{
			++i;
			out_light->range = assetdoc_json_to_float(tokens + i, json_chunk);
			++i;
		}
		else if (assetdoc_json_strcmp(tokens+i, json_chunk, "spot") == 0)
		{
			++i;

			ASSETDOC_CHECK_TOKTYPE(tokens[i], JSMN_OBJECT);

			int data_size = tokens[i].size;
			++i;

			for (int k = 0; k < data_size; ++k)
			{
				ASSETDOC_CHECK_KEY(tokens[i]);

				if (assetdoc_json_strcmp(tokens+i, json_chunk, "innerConeAngle") == 0)
				{
					++i;
					out_light->spot_inner_cone_angle = assetdoc_json_to_float(tokens + i, json_chunk);
					++i;
				}
				else if (assetdoc_json_strcmp(tokens+i, json_chunk, "outerConeAngle") == 0)
				{
					++i;
					out_light->spot_outer_cone_angle = assetdoc_json_to_float(tokens + i, json_chunk);
					++i;
				}
				else
				{
					i = assetdoc_skip_json(tokens, i+1);
				}

				if (i < 0)
				{
					return i;
				}
			}
		}
		else if (assetdoc_json_strcmp(tokens + i, json_chunk, "extras") == 0)
		{
			i = assetdoc_parse_json_extras(options, tokens, i + 1, json_chunk, &out_light->extras);
		}
		else
		{
			i = assetdoc_skip_json(tokens, i+1);
		}

		if (i < 0)
		{
			return i;
		}
	}

	return i;
}

static int assetdoc_parse_json_lights(assetdoc_options* options, jsmntok_t const* tokens, int i, const uint8_t* json_chunk, assetdoc_data* out_data)
{
	i = assetdoc_parse_json_array(options, tokens, i, json_chunk, sizeof(assetdoc_light), (void**)&out_data->lights, &out_data->lights_count);
	if (i < 0)
	{
		return i;
	}

	for (assetdoc_size j = 0; j < out_data->lights_count; ++j)
	{
		i = assetdoc_parse_json_light(options, tokens, i, json_chunk, &out_data->lights[j]);
		if (i < 0)
		{
			return i;
		}
	}
	return i;
}

static int assetdoc_parse_json_node(assetdoc_options* options, jsmntok_t const* tokens, int i, const uint8_t* json_chunk, assetdoc_node* out_node)
{
	ASSETDOC_CHECK_TOKTYPE(tokens[i], JSMN_OBJECT);

	out_node->rotation[3] = 1.0f;
	out_node->scale[0] = 1.0f;
	out_node->scale[1] = 1.0f;
	out_node->scale[2] = 1.0f;
	out_node->matrix[0] = 1.0f;
	out_node->matrix[5] = 1.0f;
	out_node->matrix[10] = 1.0f;
	out_node->matrix[15] = 1.0f;

	int size = tokens[i].size;
	++i;

	for (int j = 0; j < size; ++j)
	{
		ASSETDOC_CHECK_KEY(tokens[i]);

		if (assetdoc_json_strcmp(tokens+i, json_chunk, "name") == 0)
		{
			i = assetdoc_parse_json_string(options, tokens, i + 1, json_chunk, &out_node->name);
		}
		else if (assetdoc_json_strcmp(tokens+i, json_chunk, "children") == 0)
		{
			i = assetdoc_parse_json_array(options, tokens, i + 1, json_chunk, sizeof(assetdoc_node*), (void**)&out_node->children, &out_node->children_count);
			if (i < 0)
			{
				return i;
			}

			for (assetdoc_size k = 0; k < out_node->children_count; ++k)
			{
				out_node->children[k] = ASSETDOC_PTRINDEX(assetdoc_node, assetdoc_json_to_int(tokens + i, json_chunk));
				++i;
			}
		}
		else if (assetdoc_json_strcmp(tokens+i, json_chunk, "mesh") == 0)
		{
			++i;
			ASSETDOC_CHECK_TOKTYPE(tokens[i], JSMN_PRIMITIVE);
			out_node->mesh = ASSETDOC_PTRINDEX(assetdoc_mesh, assetdoc_json_to_int(tokens + i, json_chunk));
			++i;
		}
		else if (assetdoc_json_strcmp(tokens+i, json_chunk, "skin") == 0)
		{
			++i;
			ASSETDOC_CHECK_TOKTYPE(tokens[i], JSMN_PRIMITIVE);
			out_node->skin = ASSETDOC_PTRINDEX(assetdoc_skin, assetdoc_json_to_int(tokens + i, json_chunk));
			++i;
		}
		else if (assetdoc_json_strcmp(tokens+i, json_chunk, "camera") == 0)
		{
			++i;
			ASSETDOC_CHECK_TOKTYPE(tokens[i], JSMN_PRIMITIVE);
			out_node->camera = ASSETDOC_PTRINDEX(assetdoc_camera, assetdoc_json_to_int(tokens + i, json_chunk));
			++i;
		}
		else if (assetdoc_json_strcmp(tokens+i, json_chunk, "translation") == 0)
		{
			out_node->has_translation = 1;
			i = assetdoc_parse_json_float_array(tokens, i + 1, json_chunk, out_node->translation, 3);
		}
		else if (assetdoc_json_strcmp(tokens+i, json_chunk, "rotation") == 0)
		{
			out_node->has_rotation = 1;
			i = assetdoc_parse_json_float_array(tokens, i + 1, json_chunk, out_node->rotation, 4);
		}
		else if (assetdoc_json_strcmp(tokens+i, json_chunk, "scale") == 0)
		{
			out_node->has_scale = 1;
			i = assetdoc_parse_json_float_array(tokens, i + 1, json_chunk, out_node->scale, 3);
		}
		else if (assetdoc_json_strcmp(tokens+i, json_chunk, "matrix") == 0)
		{
			out_node->has_matrix = 1;
			i = assetdoc_parse_json_float_array(tokens, i + 1, json_chunk, out_node->matrix, 16);
		}
		else if (assetdoc_json_strcmp(tokens + i, json_chunk, "weights") == 0)
		{
			i = assetdoc_parse_json_array(options, tokens, i + 1, json_chunk, sizeof(assetdoc_float), (void**)&out_node->weights, &out_node->weights_count);
			if (i < 0)
			{
				return i;
			}

			i = assetdoc_parse_json_float_array(tokens, i - 1, json_chunk, out_node->weights, (int)out_node->weights_count);
		}
		else if (assetdoc_json_strcmp(tokens + i, json_chunk, "extras") == 0)
		{
			i = assetdoc_parse_json_extras(options, tokens, i + 1, json_chunk, &out_node->extras);
		}
		else if (assetdoc_json_strcmp(tokens + i, json_chunk, "extensions") == 0)
		{
			++i;

			ASSETDOC_CHECK_TOKTYPE(tokens[i], JSMN_OBJECT);
			if(out_node->extensions)
			{
				return ASSETDOC_ERROR_JSON;
			}

			int extensions_size = tokens[i].size;
			out_node->extensions_count= 0;
			out_node->extensions = (assetdoc_extension*)assetdoc_calloc(options, sizeof(assetdoc_extension), extensions_size);

			if (!out_node->extensions)
			{
				return ASSETDOC_ERROR_NOMEM;
			}

			++i;

			for (int k = 0; k < extensions_size; ++k)
			{
				ASSETDOC_CHECK_KEY(tokens[i]);

				if (assetdoc_json_strcmp(tokens+i, json_chunk, "KHR_lights_punctual") == 0)
				{
					++i;

					ASSETDOC_CHECK_TOKTYPE(tokens[i], JSMN_OBJECT);

					int data_size = tokens[i].size;
					++i;

					for (int m = 0; m < data_size; ++m)
					{
						ASSETDOC_CHECK_KEY(tokens[i]);

						if (assetdoc_json_strcmp(tokens + i, json_chunk, "light") == 0)
						{
							++i;
							ASSETDOC_CHECK_TOKTYPE(tokens[i], JSMN_PRIMITIVE);
							out_node->light = ASSETDOC_PTRINDEX(assetdoc_light, assetdoc_json_to_int(tokens + i, json_chunk));
							++i;
						}
						else
						{
							i = assetdoc_skip_json(tokens, i + 1);
						}

						if (i < 0)
						{
							return i;
						}
					}
				}
				else if (assetdoc_json_strcmp(tokens + i, json_chunk, "EXT_mesh_gpu_instancing") == 0)
				{
					out_node->has_mesh_gpu_instancing = 1;
					i = assetdoc_parse_json_mesh_gpu_instancing(options, tokens, i + 1, json_chunk, &out_node->mesh_gpu_instancing);
				}
				else
				{
					i = assetdoc_parse_json_unprocessed_extension(options, tokens, i, json_chunk, &(out_node->extensions[out_node->extensions_count++]));
				}

				if (i < 0)
				{
					return i;
				}
			}
		}
		else
		{
			i = assetdoc_skip_json(tokens, i+1);
		}

		if (i < 0)
		{
			return i;
		}
	}

	return i;
}

static int assetdoc_parse_json_nodes(assetdoc_options* options, jsmntok_t const* tokens, int i, const uint8_t* json_chunk, assetdoc_data* out_data)
{
	i = assetdoc_parse_json_array(options, tokens, i, json_chunk, sizeof(assetdoc_node), (void**)&out_data->nodes, &out_data->nodes_count);
	if (i < 0)
	{
		return i;
	}

	for (assetdoc_size j = 0; j < out_data->nodes_count; ++j)
	{
		i = assetdoc_parse_json_node(options, tokens, i, json_chunk, &out_data->nodes[j]);
		if (i < 0)
		{
			return i;
		}
	}
	return i;
}

static int assetdoc_parse_json_scene(assetdoc_options* options, jsmntok_t const* tokens, int i, const uint8_t* json_chunk, assetdoc_scene* out_scene)
{
	ASSETDOC_CHECK_TOKTYPE(tokens[i], JSMN_OBJECT);

	int size = tokens[i].size;
	++i;

	for (int j = 0; j < size; ++j)
	{
		ASSETDOC_CHECK_KEY(tokens[i]);

		if (assetdoc_json_strcmp(tokens+i, json_chunk, "name") == 0)
		{
			i = assetdoc_parse_json_string(options, tokens, i + 1, json_chunk, &out_scene->name);
		}
		else if (assetdoc_json_strcmp(tokens+i, json_chunk, "nodes") == 0)
		{
			i = assetdoc_parse_json_array(options, tokens, i + 1, json_chunk, sizeof(assetdoc_node*), (void**)&out_scene->nodes, &out_scene->nodes_count);
			if (i < 0)
			{
				return i;
			}

			for (assetdoc_size k = 0; k < out_scene->nodes_count; ++k)
			{
				out_scene->nodes[k] = ASSETDOC_PTRINDEX(assetdoc_node, assetdoc_json_to_int(tokens + i, json_chunk));
				++i;
			}
		}
		else if (assetdoc_json_strcmp(tokens + i, json_chunk, "extras") == 0)
		{
			i = assetdoc_parse_json_extras(options, tokens, i + 1, json_chunk, &out_scene->extras);
		}
		else if (assetdoc_json_strcmp(tokens + i, json_chunk, "extensions") == 0)
		{
			i = assetdoc_parse_json_unprocessed_extensions(options, tokens, i, json_chunk, &out_scene->extensions_count, &out_scene->extensions);
		}
		else
		{
			i = assetdoc_skip_json(tokens, i+1);
		}

		if (i < 0)
		{
			return i;
		}
	}

	return i;
}

static int assetdoc_parse_json_scenes(assetdoc_options* options, jsmntok_t const* tokens, int i, const uint8_t* json_chunk, assetdoc_data* out_data)
{
	i = assetdoc_parse_json_array(options, tokens, i, json_chunk, sizeof(assetdoc_scene), (void**)&out_data->scenes, &out_data->scenes_count);
	if (i < 0)
	{
		return i;
	}

	for (assetdoc_size j = 0; j < out_data->scenes_count; ++j)
	{
		i = assetdoc_parse_json_scene(options, tokens, i, json_chunk, &out_data->scenes[j]);
		if (i < 0)
		{
			return i;
		}
	}
	return i;
}

static int assetdoc_parse_json_animation_sampler(assetdoc_options* options, jsmntok_t const* tokens, int i, const uint8_t* json_chunk, assetdoc_animation_sampler* out_sampler)
{
	(void)options;
	ASSETDOC_CHECK_TOKTYPE(tokens[i], JSMN_OBJECT);

	int size = tokens[i].size;
	++i;

	for (int j = 0; j < size; ++j)
	{
		ASSETDOC_CHECK_KEY(tokens[i]);

		if (assetdoc_json_strcmp(tokens+i, json_chunk, "input") == 0)
		{
			++i;
			out_sampler->input = ASSETDOC_PTRINDEX(assetdoc_accessor, assetdoc_json_to_int(tokens + i, json_chunk));
			++i;
		}
		else if (assetdoc_json_strcmp(tokens+i, json_chunk, "output") == 0)
		{
			++i;
			out_sampler->output = ASSETDOC_PTRINDEX(assetdoc_accessor, assetdoc_json_to_int(tokens + i, json_chunk));
			++i;
		}
		else if (assetdoc_json_strcmp(tokens+i, json_chunk, "interpolation") == 0)
		{
			++i;
			if (assetdoc_json_strcmp(tokens + i, json_chunk, "LINEAR") == 0)
			{
				out_sampler->interpolation = assetdoc_interpolation_type_linear;
			}
			else if (assetdoc_json_strcmp(tokens + i, json_chunk, "STEP") == 0)
			{
				out_sampler->interpolation = assetdoc_interpolation_type_step;
			}
			else if (assetdoc_json_strcmp(tokens + i, json_chunk, "CUBICSPLINE") == 0)
			{
				out_sampler->interpolation = assetdoc_interpolation_type_cubic_spline;
			}
			++i;
		}
		else if (assetdoc_json_strcmp(tokens + i, json_chunk, "extras") == 0)
		{
			i = assetdoc_parse_json_extras(options, tokens, i + 1, json_chunk, &out_sampler->extras);
		}
		else if (assetdoc_json_strcmp(tokens + i, json_chunk, "extensions") == 0)
		{
			i = assetdoc_parse_json_unprocessed_extensions(options, tokens, i, json_chunk, &out_sampler->extensions_count, &out_sampler->extensions);
		}
		else
		{
			i = assetdoc_skip_json(tokens, i+1);
		}

		if (i < 0)
		{
			return i;
		}
	}

	return i;
}

static int assetdoc_parse_json_animation_channel(assetdoc_options* options, jsmntok_t const* tokens, int i, const uint8_t* json_chunk, assetdoc_animation_channel* out_channel)
{
	(void)options;
	ASSETDOC_CHECK_TOKTYPE(tokens[i], JSMN_OBJECT);

	int size = tokens[i].size;
	++i;

	for (int j = 0; j < size; ++j)
	{
		ASSETDOC_CHECK_KEY(tokens[i]);

		if (assetdoc_json_strcmp(tokens+i, json_chunk, "sampler") == 0)
		{
			++i;
			out_channel->sampler = ASSETDOC_PTRINDEX(assetdoc_animation_sampler, assetdoc_json_to_int(tokens + i, json_chunk));
			++i;
		}
		else if (assetdoc_json_strcmp(tokens+i, json_chunk, "target") == 0)
		{
			++i;

			ASSETDOC_CHECK_TOKTYPE(tokens[i], JSMN_OBJECT);

			int target_size = tokens[i].size;
			++i;

			for (int k = 0; k < target_size; ++k)
			{
				ASSETDOC_CHECK_KEY(tokens[i]);

				if (assetdoc_json_strcmp(tokens+i, json_chunk, "node") == 0)
				{
					++i;
					out_channel->target_node = ASSETDOC_PTRINDEX(assetdoc_node, assetdoc_json_to_int(tokens + i, json_chunk));
					++i;
				}
				else if (assetdoc_json_strcmp(tokens+i, json_chunk, "path") == 0)
				{
					++i;
					if (assetdoc_json_strcmp(tokens+i, json_chunk, "translation") == 0)
					{
						out_channel->target_path = assetdoc_animation_path_type_translation;
					}
					else if (assetdoc_json_strcmp(tokens+i, json_chunk, "rotation") == 0)
					{
						out_channel->target_path = assetdoc_animation_path_type_rotation;
					}
					else if (assetdoc_json_strcmp(tokens+i, json_chunk, "scale") == 0)
					{
						out_channel->target_path = assetdoc_animation_path_type_scale;
					}
					else if (assetdoc_json_strcmp(tokens+i, json_chunk, "weights") == 0)
					{
						out_channel->target_path = assetdoc_animation_path_type_weights;
					}
					++i;
				}
				else if (assetdoc_json_strcmp(tokens + i, json_chunk, "extras") == 0)
				{
					i = assetdoc_parse_json_extras(options, tokens, i + 1, json_chunk, &out_channel->extras);
				}
				else if (assetdoc_json_strcmp(tokens + i, json_chunk, "extensions") == 0)
				{
					i = assetdoc_parse_json_unprocessed_extensions(options, tokens, i, json_chunk, &out_channel->extensions_count, &out_channel->extensions);
				}
				else
				{
					i = assetdoc_skip_json(tokens, i+1);
				}

				if (i < 0)
				{
					return i;
				}
			}
		}
		else
		{
			i = assetdoc_skip_json(tokens, i+1);
		}

		if (i < 0)
		{
			return i;
		}
	}

	return i;
}

static int assetdoc_parse_json_animation(assetdoc_options* options, jsmntok_t const* tokens, int i, const uint8_t* json_chunk, assetdoc_animation* out_animation)
{
	ASSETDOC_CHECK_TOKTYPE(tokens[i], JSMN_OBJECT);

	int size = tokens[i].size;
	++i;

	for (int j = 0; j < size; ++j)
	{
		ASSETDOC_CHECK_KEY(tokens[i]);

		if (assetdoc_json_strcmp(tokens+i, json_chunk, "name") == 0)
		{
			i = assetdoc_parse_json_string(options, tokens, i + 1, json_chunk, &out_animation->name);
		}
		else if (assetdoc_json_strcmp(tokens+i, json_chunk, "samplers") == 0)
		{
			i = assetdoc_parse_json_array(options, tokens, i + 1, json_chunk, sizeof(assetdoc_animation_sampler), (void**)&out_animation->samplers, &out_animation->samplers_count);
			if (i < 0)
			{
				return i;
			}

			for (assetdoc_size k = 0; k < out_animation->samplers_count; ++k)
			{
				i = assetdoc_parse_json_animation_sampler(options, tokens, i, json_chunk, &out_animation->samplers[k]);
				if (i < 0)
				{
					return i;
				}
			}
		}
		else if (assetdoc_json_strcmp(tokens+i, json_chunk, "channels") == 0)
		{
			i = assetdoc_parse_json_array(options, tokens, i + 1, json_chunk, sizeof(assetdoc_animation_channel), (void**)&out_animation->channels, &out_animation->channels_count);
			if (i < 0)
			{
				return i;
			}

			for (assetdoc_size k = 0; k < out_animation->channels_count; ++k)
			{
				i = assetdoc_parse_json_animation_channel(options, tokens, i, json_chunk, &out_animation->channels[k]);
				if (i < 0)
				{
					return i;
				}
			}
		}
		else if (assetdoc_json_strcmp(tokens + i, json_chunk, "extras") == 0)
		{
			i = assetdoc_parse_json_extras(options, tokens, i + 1, json_chunk, &out_animation->extras);
		}
		else if (assetdoc_json_strcmp(tokens + i, json_chunk, "extensions") == 0)
		{
			i = assetdoc_parse_json_unprocessed_extensions(options, tokens, i, json_chunk, &out_animation->extensions_count, &out_animation->extensions);
		}
		else
		{
			i = assetdoc_skip_json(tokens, i+1);
		}

		if (i < 0)
		{
			return i;
		}
	}

	return i;
}

static int assetdoc_parse_json_animations(assetdoc_options* options, jsmntok_t const* tokens, int i, const uint8_t* json_chunk, assetdoc_data* out_data)
{
	i = assetdoc_parse_json_array(options, tokens, i, json_chunk, sizeof(assetdoc_animation), (void**)&out_data->animations, &out_data->animations_count);
	if (i < 0)
	{
		return i;
	}

	for (assetdoc_size j = 0; j < out_data->animations_count; ++j)
	{
		i = assetdoc_parse_json_animation(options, tokens, i, json_chunk, &out_data->animations[j]);
		if (i < 0)
		{
			return i;
		}
	}
	return i;
}

static int assetdoc_parse_json_variant(assetdoc_options* options, jsmntok_t const* tokens, int i, const uint8_t* json_chunk, assetdoc_material_variant* out_variant)
{
	ASSETDOC_CHECK_TOKTYPE(tokens[i], JSMN_OBJECT);

	int size = tokens[i].size;
	++i;

	for (int j = 0; j < size; ++j)
	{
		ASSETDOC_CHECK_KEY(tokens[i]);

		if (assetdoc_json_strcmp(tokens+i, json_chunk, "name") == 0)
		{
			i = assetdoc_parse_json_string(options, tokens, i + 1, json_chunk, &out_variant->name);
		}
		else if (assetdoc_json_strcmp(tokens + i, json_chunk, "extras") == 0)
		{
			i = assetdoc_parse_json_extras(options, tokens, i + 1, json_chunk, &out_variant->extras);
		}
		else
		{
			i = assetdoc_skip_json(tokens, i+1);
		}

		if (i < 0)
		{
			return i;
		}
	}

	return i;
}

static int assetdoc_parse_json_variants(assetdoc_options* options, jsmntok_t const* tokens, int i, const uint8_t* json_chunk, assetdoc_data* out_data)
{
	i = assetdoc_parse_json_array(options, tokens, i, json_chunk, sizeof(assetdoc_material_variant), (void**)&out_data->variants, &out_data->variants_count);
	if (i < 0)
	{
		return i;
	}

	for (assetdoc_size j = 0; j < out_data->variants_count; ++j)
	{
		i = assetdoc_parse_json_variant(options, tokens, i, json_chunk, &out_data->variants[j]);
		if (i < 0)
		{
			return i;
		}
	}
	return i;
}

static int assetdoc_parse_json_asset(assetdoc_options* options, jsmntok_t const* tokens, int i, const uint8_t* json_chunk, assetdoc_asset* out_asset)
{
	ASSETDOC_CHECK_TOKTYPE(tokens[i], JSMN_OBJECT);

	int size = tokens[i].size;
	++i;

	for (int j = 0; j < size; ++j)
	{
		ASSETDOC_CHECK_KEY(tokens[i]);

		if (assetdoc_json_strcmp(tokens+i, json_chunk, "copyright") == 0)
		{
			i = assetdoc_parse_json_string(options, tokens, i + 1, json_chunk, &out_asset->copyright);
		}
		else if (assetdoc_json_strcmp(tokens+i, json_chunk, "generator") == 0)
		{
			i = assetdoc_parse_json_string(options, tokens, i + 1, json_chunk, &out_asset->generator);
		}
		else if (assetdoc_json_strcmp(tokens+i, json_chunk, "version") == 0)
		{
			i = assetdoc_parse_json_string(options, tokens, i + 1, json_chunk, &out_asset->version);
		}
		else if (assetdoc_json_strcmp(tokens+i, json_chunk, "minVersion") == 0)
		{
			i = assetdoc_parse_json_string(options, tokens, i + 1, json_chunk, &out_asset->min_version);
		}
		else if (assetdoc_json_strcmp(tokens + i, json_chunk, "extras") == 0)
		{
			i = assetdoc_parse_json_extras(options, tokens, i + 1, json_chunk, &out_asset->extras);
		}
		else if (assetdoc_json_strcmp(tokens + i, json_chunk, "extensions") == 0)
		{
			i = assetdoc_parse_json_unprocessed_extensions(options, tokens, i, json_chunk, &out_asset->extensions_count, &out_asset->extensions);
		}
		else
		{
			i = assetdoc_skip_json(tokens, i+1);
		}

		if (i < 0)
		{
			return i;
		}
	}

	if (out_asset->version && ASSETDOC_ATOF(out_asset->version) < 2)
	{
		return ASSETDOC_ERROR_LEGACY;
	}

	return i;
}

assetdoc_size assetdoc_num_components(assetdoc_type type) {
	switch (type)
	{
	case assetdoc_type_vec2:
		return 2;
	case assetdoc_type_vec3:
		return 3;
	case assetdoc_type_vec4:
		return 4;
	case assetdoc_type_mat2:
		return 4;
	case assetdoc_type_mat3:
		return 9;
	case assetdoc_type_mat4:
		return 16;
	case assetdoc_type_invalid:
	case assetdoc_type_scalar:
	default:
		return 1;
	}
}

assetdoc_size assetdoc_component_size(assetdoc_component_type component_type) {
	switch (component_type)
	{
	case assetdoc_component_type_r_8:
	case assetdoc_component_type_r_8u:
		return 1;
	case assetdoc_component_type_r_16:
	case assetdoc_component_type_r_16u:
		return 2;
	case assetdoc_component_type_r_32u:
	case assetdoc_component_type_r_32f:
		return 4;
	case assetdoc_component_type_invalid:
	default:
		return 0;
	}
}

assetdoc_size assetdoc_calc_size(assetdoc_type type, assetdoc_component_type component_type)
{
	assetdoc_size component_size = assetdoc_component_size(component_type);
	if (type == assetdoc_type_mat2 && component_size == 1)
	{
		return 8 * component_size;
	}
	else if (type == assetdoc_type_mat3 && (component_size == 1 || component_size == 2))
	{
		return 12 * component_size;
	}
	return component_size * assetdoc_num_components(type);
}

static int assetdoc_fixup_pointers(assetdoc_data* out_data);

static int assetdoc_parse_json_root(assetdoc_options* options, jsmntok_t const* tokens, int i, const uint8_t* json_chunk, assetdoc_data* out_data)
{
	ASSETDOC_CHECK_TOKTYPE(tokens[i], JSMN_OBJECT);

	int size = tokens[i].size;
	++i;

	for (int j = 0; j < size; ++j)
	{
		ASSETDOC_CHECK_KEY(tokens[i]);

		if (assetdoc_json_strcmp(tokens + i, json_chunk, "asset") == 0)
		{
			i = assetdoc_parse_json_asset(options, tokens, i + 1, json_chunk, &out_data->asset);
		}
		else if (assetdoc_json_strcmp(tokens + i, json_chunk, "meshes") == 0)
		{
			i = assetdoc_parse_json_meshes(options, tokens, i + 1, json_chunk, out_data);
		}
		else if (assetdoc_json_strcmp(tokens + i, json_chunk, "accessors") == 0)
		{
			i = assetdoc_parse_json_accessors(options, tokens, i + 1, json_chunk, out_data);
		}
		else if (assetdoc_json_strcmp(tokens + i, json_chunk, "bufferViews") == 0)
		{
			i = assetdoc_parse_json_buffer_views(options, tokens, i + 1, json_chunk, out_data);
		}
		else if (assetdoc_json_strcmp(tokens + i, json_chunk, "buffers") == 0)
		{
			i = assetdoc_parse_json_buffers(options, tokens, i + 1, json_chunk, out_data);
		}
		else if (assetdoc_json_strcmp(tokens + i, json_chunk, "materials") == 0)
		{
			i = assetdoc_parse_json_materials(options, tokens, i + 1, json_chunk, out_data);
		}
		else if (assetdoc_json_strcmp(tokens + i, json_chunk, "images") == 0)
		{
			i = assetdoc_parse_json_images(options, tokens, i + 1, json_chunk, out_data);
		}
		else if (assetdoc_json_strcmp(tokens + i, json_chunk, "textures") == 0)
		{
			i = assetdoc_parse_json_textures(options, tokens, i + 1, json_chunk, out_data);
		}
		else if (assetdoc_json_strcmp(tokens + i, json_chunk, "samplers") == 0)
		{
			i = assetdoc_parse_json_samplers(options, tokens, i + 1, json_chunk, out_data);
		}
		else if (assetdoc_json_strcmp(tokens + i, json_chunk, "skins") == 0)
		{
			i = assetdoc_parse_json_skins(options, tokens, i + 1, json_chunk, out_data);
		}
		else if (assetdoc_json_strcmp(tokens + i, json_chunk, "cameras") == 0)
		{
			i = assetdoc_parse_json_cameras(options, tokens, i + 1, json_chunk, out_data);
		}
		else if (assetdoc_json_strcmp(tokens + i, json_chunk, "nodes") == 0)
		{
			i = assetdoc_parse_json_nodes(options, tokens, i + 1, json_chunk, out_data);
		}
		else if (assetdoc_json_strcmp(tokens + i, json_chunk, "scenes") == 0)
		{
			i = assetdoc_parse_json_scenes(options, tokens, i + 1, json_chunk, out_data);
		}
		else if (assetdoc_json_strcmp(tokens + i, json_chunk, "scene") == 0)
		{
			++i;
			out_data->scene = ASSETDOC_PTRINDEX(assetdoc_scene, assetdoc_json_to_int(tokens + i, json_chunk));
			++i;
		}
		else if (assetdoc_json_strcmp(tokens + i, json_chunk, "animations") == 0)
		{
			i = assetdoc_parse_json_animations(options, tokens, i + 1, json_chunk, out_data);
		}
		else if (assetdoc_json_strcmp(tokens+i, json_chunk, "extras") == 0)
		{
			i = assetdoc_parse_json_extras(options, tokens, i + 1, json_chunk, &out_data->extras);
		}
		else if (assetdoc_json_strcmp(tokens + i, json_chunk, "extensions") == 0)
		{
			++i;

			ASSETDOC_CHECK_TOKTYPE(tokens[i], JSMN_OBJECT);
			if(out_data->data_extensions)
			{
				return ASSETDOC_ERROR_JSON;
			}

			int extensions_size = tokens[i].size;
			out_data->data_extensions_count = 0;
			out_data->data_extensions = (assetdoc_extension*)assetdoc_calloc(options, sizeof(assetdoc_extension), extensions_size);

			if (!out_data->data_extensions)
			{
				return ASSETDOC_ERROR_NOMEM;
			}

			++i;

			for (int k = 0; k < extensions_size; ++k)
			{
				ASSETDOC_CHECK_KEY(tokens[i]);

				if (assetdoc_json_strcmp(tokens+i, json_chunk, "KHR_lights_punctual") == 0)
				{
					++i;

					ASSETDOC_CHECK_TOKTYPE(tokens[i], JSMN_OBJECT);

					int data_size = tokens[i].size;
					++i;

					for (int m = 0; m < data_size; ++m)
					{
						ASSETDOC_CHECK_KEY(tokens[i]);

						if (assetdoc_json_strcmp(tokens + i, json_chunk, "lights") == 0)
						{
							i = assetdoc_parse_json_lights(options, tokens, i + 1, json_chunk, out_data);
						}
						else
						{
							i = assetdoc_skip_json(tokens, i + 1);
						}

						if (i < 0)
						{
							return i;
						}
					}
				}
				else if (assetdoc_json_strcmp(tokens+i, json_chunk, "KHR_materials_variants") == 0)
				{
					++i;

					ASSETDOC_CHECK_TOKTYPE(tokens[i], JSMN_OBJECT);

					int data_size = tokens[i].size;
					++i;

					for (int m = 0; m < data_size; ++m)
					{
						ASSETDOC_CHECK_KEY(tokens[i]);

						if (assetdoc_json_strcmp(tokens + i, json_chunk, "variants") == 0)
						{
							i = assetdoc_parse_json_variants(options, tokens, i + 1, json_chunk, out_data);
						}
						else
						{
							i = assetdoc_skip_json(tokens, i + 1);
						}

						if (i < 0)
						{
							return i;
						}
					}
				}
				else
				{
					i = assetdoc_parse_json_unprocessed_extension(options, tokens, i, json_chunk, &(out_data->data_extensions[out_data->data_extensions_count++]));
				}

				if (i < 0)
				{
					return i;
				}
			}
		}
		else if (assetdoc_json_strcmp(tokens + i, json_chunk, "extensionsUsed") == 0)
		{
			i = assetdoc_parse_json_string_array(options, tokens, i + 1, json_chunk, &out_data->extensions_used, &out_data->extensions_used_count);
		}
		else if (assetdoc_json_strcmp(tokens + i, json_chunk, "extensionsRequired") == 0)
		{
			i = assetdoc_parse_json_string_array(options, tokens, i + 1, json_chunk, &out_data->extensions_required, &out_data->extensions_required_count);
		}
		else
		{
			i = assetdoc_skip_json(tokens, i + 1);
		}

		if (i < 0)
		{
			return i;
		}
	}

	return i;
}

assetdoc_result assetdoc_parse_json(assetdoc_options* options, const uint8_t* json_chunk, assetdoc_size size, assetdoc_data** out_data)
{
	jsmn_parser parser = { 0, 0, 0 };

	if (options->json_token_count == 0)
	{
		int token_count = jsmn_parse(&parser, (const char*)json_chunk, size, NULL, 0);

		if (token_count <= 0)
		{
			return assetdoc_result_invalid_json;
		}

		options->json_token_count = token_count;
	}

	jsmntok_t* tokens = (jsmntok_t*)options->memory.alloc_func(options->memory.user_data, sizeof(jsmntok_t) * (options->json_token_count + 1));

	if (!tokens)
	{
		return assetdoc_result_out_of_memory;
	}

	jsmn_init(&parser);

	int token_count = jsmn_parse(&parser, (const char*)json_chunk, size, tokens, options->json_token_count);

	if (token_count <= 0)
	{
		options->memory.free_func(options->memory.user_data, tokens);
		return assetdoc_result_invalid_json;
	}

	// this makes sure that we always have an UNDEFINED token at the end of the stream
	// for invalid JSON inputs this makes sure we don't perform out of bound reads of token data
	tokens[token_count].type = JSMN_UNDEFINED;

	assetdoc_data* data = (assetdoc_data*)options->memory.alloc_func(options->memory.user_data, sizeof(assetdoc_data));

	if (!data)
	{
		options->memory.free_func(options->memory.user_data, tokens);
		return assetdoc_result_out_of_memory;
	}

	memset(data, 0, sizeof(assetdoc_data));
	data->memory = options->memory;
	data->file = options->file;

	int i = assetdoc_parse_json_root(options, tokens, 0, json_chunk, data);

	options->memory.free_func(options->memory.user_data, tokens);

	if (i < 0)
	{
		assetdoc_free(data);

		switch (i)
		{
		case ASSETDOC_ERROR_NOMEM: return assetdoc_result_out_of_memory;
		case ASSETDOC_ERROR_LEGACY: return assetdoc_result_legacy_scene_doc;
		default: return assetdoc_result_invalid_scene_doc;
		}
	}

	if (assetdoc_fixup_pointers(data) < 0)
	{
		assetdoc_free(data);
		return assetdoc_result_invalid_scene_doc;
	}

	data->json = (const char*)json_chunk;
	data->json_size = size;

	*out_data = data;

	return assetdoc_result_success;
}

static int assetdoc_fixup_pointers(assetdoc_data* data)
{
	for (assetdoc_size i = 0; i < data->meshes_count; ++i)
	{
		for (assetdoc_size j = 0; j < data->meshes[i].primitives_count; ++j)
		{
			ASSETDOC_PTRFIXUP(data->meshes[i].primitives[j].indices, data->accessors, data->accessors_count);
			ASSETDOC_PTRFIXUP(data->meshes[i].primitives[j].material, data->materials, data->materials_count);

			for (assetdoc_size k = 0; k < data->meshes[i].primitives[j].attributes_count; ++k)
			{
				ASSETDOC_PTRFIXUP_REQ(data->meshes[i].primitives[j].attributes[k].data, data->accessors, data->accessors_count);
			}

			for (assetdoc_size k = 0; k < data->meshes[i].primitives[j].targets_count; ++k)
			{
				for (assetdoc_size m = 0; m < data->meshes[i].primitives[j].targets[k].attributes_count; ++m)
				{
					ASSETDOC_PTRFIXUP_REQ(data->meshes[i].primitives[j].targets[k].attributes[m].data, data->accessors, data->accessors_count);
				}
			}

			if (data->meshes[i].primitives[j].has_draco_mesh_compression)
			{
				ASSETDOC_PTRFIXUP_REQ(data->meshes[i].primitives[j].draco_mesh_compression.buffer_view, data->buffer_views, data->buffer_views_count);
				for (assetdoc_size m = 0; m < data->meshes[i].primitives[j].draco_mesh_compression.attributes_count; ++m)
				{
					ASSETDOC_PTRFIXUP_REQ(data->meshes[i].primitives[j].draco_mesh_compression.attributes[m].data, data->accessors, data->accessors_count);
				}
			}

			for (assetdoc_size k = 0; k < data->meshes[i].primitives[j].mappings_count; ++k)
			{
				ASSETDOC_PTRFIXUP_REQ(data->meshes[i].primitives[j].mappings[k].material, data->materials, data->materials_count);
			}
		}
	}

	for (assetdoc_size i = 0; i < data->accessors_count; ++i)
	{
		ASSETDOC_PTRFIXUP(data->accessors[i].buffer_view, data->buffer_views, data->buffer_views_count);

		if (data->accessors[i].is_sparse)
		{
			ASSETDOC_PTRFIXUP_REQ(data->accessors[i].sparse.indices_buffer_view, data->buffer_views, data->buffer_views_count);
			ASSETDOC_PTRFIXUP_REQ(data->accessors[i].sparse.values_buffer_view, data->buffer_views, data->buffer_views_count);
		}

		if (data->accessors[i].buffer_view)
		{
			data->accessors[i].stride = data->accessors[i].buffer_view->stride;
		}

		if (data->accessors[i].stride == 0)
		{
			data->accessors[i].stride = assetdoc_calc_size(data->accessors[i].type, data->accessors[i].component_type);
		}
	}

	for (assetdoc_size i = 0; i < data->textures_count; ++i)
	{
		ASSETDOC_PTRFIXUP(data->textures[i].image, data->images, data->images_count);
		ASSETDOC_PTRFIXUP(data->textures[i].basisu_image, data->images, data->images_count);
		ASSETDOC_PTRFIXUP(data->textures[i].webp_image, data->images, data->images_count);
		ASSETDOC_PTRFIXUP(data->textures[i].sampler, data->samplers, data->samplers_count);
	}

	for (assetdoc_size i = 0; i < data->images_count; ++i)
	{
		ASSETDOC_PTRFIXUP(data->images[i].buffer_view, data->buffer_views, data->buffer_views_count);
	}

	for (assetdoc_size i = 0; i < data->materials_count; ++i)
	{
		ASSETDOC_PTRFIXUP(data->materials[i].normal_texture.texture, data->textures, data->textures_count);
		ASSETDOC_PTRFIXUP(data->materials[i].emissive_texture.texture, data->textures, data->textures_count);
		ASSETDOC_PTRFIXUP(data->materials[i].occlusion_texture.texture, data->textures, data->textures_count);

		ASSETDOC_PTRFIXUP(data->materials[i].pbr_metallic_roughness.base_color_texture.texture, data->textures, data->textures_count);
		ASSETDOC_PTRFIXUP(data->materials[i].pbr_metallic_roughness.metallic_roughness_texture.texture, data->textures, data->textures_count);

		ASSETDOC_PTRFIXUP(data->materials[i].pbr_specular_glossiness.diffuse_texture.texture, data->textures, data->textures_count);
		ASSETDOC_PTRFIXUP(data->materials[i].pbr_specular_glossiness.specular_glossiness_texture.texture, data->textures, data->textures_count);

		ASSETDOC_PTRFIXUP(data->materials[i].clearcoat.clearcoat_texture.texture, data->textures, data->textures_count);
		ASSETDOC_PTRFIXUP(data->materials[i].clearcoat.clearcoat_roughness_texture.texture, data->textures, data->textures_count);
		ASSETDOC_PTRFIXUP(data->materials[i].clearcoat.clearcoat_normal_texture.texture, data->textures, data->textures_count);

		ASSETDOC_PTRFIXUP(data->materials[i].specular.specular_texture.texture, data->textures, data->textures_count);
		ASSETDOC_PTRFIXUP(data->materials[i].specular.specular_color_texture.texture, data->textures, data->textures_count);

		ASSETDOC_PTRFIXUP(data->materials[i].transmission.transmission_texture.texture, data->textures, data->textures_count);

		ASSETDOC_PTRFIXUP(data->materials[i].volume.thickness_texture.texture, data->textures, data->textures_count);

		ASSETDOC_PTRFIXUP(data->materials[i].sheen.sheen_color_texture.texture, data->textures, data->textures_count);
		ASSETDOC_PTRFIXUP(data->materials[i].sheen.sheen_roughness_texture.texture, data->textures, data->textures_count);

		ASSETDOC_PTRFIXUP(data->materials[i].iridescence.iridescence_texture.texture, data->textures, data->textures_count);
		ASSETDOC_PTRFIXUP(data->materials[i].iridescence.iridescence_thickness_texture.texture, data->textures, data->textures_count);

		ASSETDOC_PTRFIXUP(data->materials[i].diffuse_transmission.diffuse_transmission_texture.texture, data->textures, data->textures_count);
		ASSETDOC_PTRFIXUP(data->materials[i].diffuse_transmission.diffuse_transmission_color_texture.texture, data->textures, data->textures_count);

		ASSETDOC_PTRFIXUP(data->materials[i].anisotropy.anisotropy_texture.texture, data->textures, data->textures_count);
	}

	for (assetdoc_size i = 0; i < data->buffer_views_count; ++i)
	{
		ASSETDOC_PTRFIXUP_REQ(data->buffer_views[i].buffer, data->buffers, data->buffers_count);

		if (data->buffer_views[i].has_meshwork_compression)
		{
			ASSETDOC_PTRFIXUP_REQ(data->buffer_views[i].meshwork_compression.buffer, data->buffers, data->buffers_count);
		}
	}

	for (assetdoc_size i = 0; i < data->skins_count; ++i)
	{
		for (assetdoc_size j = 0; j < data->skins[i].joints_count; ++j)
		{
			ASSETDOC_PTRFIXUP_REQ(data->skins[i].joints[j], data->nodes, data->nodes_count);
		}

		ASSETDOC_PTRFIXUP(data->skins[i].skeleton, data->nodes, data->nodes_count);
		ASSETDOC_PTRFIXUP(data->skins[i].inverse_bind_matrices, data->accessors, data->accessors_count);
	}

	for (assetdoc_size i = 0; i < data->nodes_count; ++i)
	{
		for (assetdoc_size j = 0; j < data->nodes[i].children_count; ++j)
		{
			ASSETDOC_PTRFIXUP_REQ(data->nodes[i].children[j], data->nodes, data->nodes_count);

			if (data->nodes[i].children[j]->parent)
			{
				return ASSETDOC_ERROR_JSON;
			}

			data->nodes[i].children[j]->parent = &data->nodes[i];
		}

		ASSETDOC_PTRFIXUP(data->nodes[i].mesh, data->meshes, data->meshes_count);
		ASSETDOC_PTRFIXUP(data->nodes[i].skin, data->skins, data->skins_count);
		ASSETDOC_PTRFIXUP(data->nodes[i].camera, data->cameras, data->cameras_count);
		ASSETDOC_PTRFIXUP(data->nodes[i].light, data->lights, data->lights_count);

		if (data->nodes[i].has_mesh_gpu_instancing)
		{
			for (assetdoc_size m = 0; m < data->nodes[i].mesh_gpu_instancing.attributes_count; ++m)
			{
				ASSETDOC_PTRFIXUP_REQ(data->nodes[i].mesh_gpu_instancing.attributes[m].data, data->accessors, data->accessors_count);
			}
		}
	}

	for (assetdoc_size i = 0; i < data->scenes_count; ++i)
	{
		for (assetdoc_size j = 0; j < data->scenes[i].nodes_count; ++j)
		{
			ASSETDOC_PTRFIXUP_REQ(data->scenes[i].nodes[j], data->nodes, data->nodes_count);

			if (data->scenes[i].nodes[j]->parent)
			{
				return ASSETDOC_ERROR_JSON;
			}
		}
	}

	ASSETDOC_PTRFIXUP(data->scene, data->scenes, data->scenes_count);

	for (assetdoc_size i = 0; i < data->animations_count; ++i)
	{
		for (assetdoc_size j = 0; j < data->animations[i].samplers_count; ++j)
		{
			ASSETDOC_PTRFIXUP_REQ(data->animations[i].samplers[j].input, data->accessors, data->accessors_count);
			ASSETDOC_PTRFIXUP_REQ(data->animations[i].samplers[j].output, data->accessors, data->accessors_count);
		}

		for (assetdoc_size j = 0; j < data->animations[i].channels_count; ++j)
		{
			ASSETDOC_PTRFIXUP_REQ(data->animations[i].channels[j].sampler, data->animations[i].samplers, data->animations[i].samplers_count);
			ASSETDOC_PTRFIXUP(data->animations[i].channels[j].target_node, data->nodes, data->nodes_count);
		}
	}

	return 0;
}

/*
 * -- jsmn.c start --
 */

/**
 * Allocates a fresh unused token from the token pull.
 */
static jsmntok_t *jsmn_alloc_token(jsmn_parser *parser,
				   jsmntok_t *tokens, size_t num_tokens) {
	jsmntok_t *tok;
	if (parser->toknext >= num_tokens) {
		return NULL;
	}
	tok = &tokens[parser->toknext++];
	tok->start = tok->end = -1;
	tok->size = 0;
#ifdef JSMN_PARENT_LINKS
	tok->parent = -1;
#endif
	return tok;
}

/**
 * Fills token type and boundaries.
 */
static void jsmn_fill_token(jsmntok_t *token, jsmntype_t type,
				ptrdiff_t start, ptrdiff_t end) {
	token->type = type;
	token->start = start;
	token->end = end;
	token->size = 0;
}

/**
 * Fills next available token with JSON primitive.
 */
static int jsmn_parse_primitive(jsmn_parser *parser, const char *js,
				size_t len, jsmntok_t *tokens, size_t num_tokens) {
	jsmntok_t *token;
	ptrdiff_t start;

	start = parser->pos;

	for (; parser->pos < len && js[parser->pos] != '\0'; parser->pos++) {
		switch (js[parser->pos]) {
#ifndef JSMN_STRICT
		/* In strict mode primitive must be followed by "," or "}" or "]" */
		case ':':
#endif
		case '\t' : case '\r' : case '\n' : case ' ' :
		case ','  : case ']'  : case '}' :
			goto found;
		}
		if (js[parser->pos] < 32 || js[parser->pos] >= 127) {
			parser->pos = start;
			return JSMN_ERROR_INVAL;
		}
	}
#ifdef JSMN_STRICT
	/* In strict mode primitive must be followed by a comma/object/array */
	parser->pos = start;
	return JSMN_ERROR_PART;
#endif

found:
	if (tokens == NULL) {
		parser->pos--;
		return 0;
	}
	token = jsmn_alloc_token(parser, tokens, num_tokens);
	if (token == NULL) {
		parser->pos = start;
		return JSMN_ERROR_NOMEM;
	}
	jsmn_fill_token(token, JSMN_PRIMITIVE, start, parser->pos);
#ifdef JSMN_PARENT_LINKS
	token->parent = parser->toksuper;
#endif
	parser->pos--;
	return 0;
}

/**
 * Fills next token with JSON string.
 */
static int jsmn_parse_string(jsmn_parser *parser, const char *js,
				 size_t len, jsmntok_t *tokens, size_t num_tokens) {
	jsmntok_t *token;

	ptrdiff_t start = parser->pos;

	parser->pos++;

	/* Skip starting quote */
	for (; parser->pos < len && js[parser->pos] != '\0'; parser->pos++) {
		char c = js[parser->pos];

		/* Quote: end of string */
		if (c == '\"') {
			if (tokens == NULL) {
				return 0;
			}
			token = jsmn_alloc_token(parser, tokens, num_tokens);
			if (token == NULL) {
				parser->pos = start;
				return JSMN_ERROR_NOMEM;
			}
			jsmn_fill_token(token, JSMN_STRING, start+1, parser->pos);
#ifdef JSMN_PARENT_LINKS
			token->parent = parser->toksuper;
#endif
			return 0;
		}

		/* Backslash: Quoted symbol expected */
		if (c == '\\' && parser->pos + 1 < len) {
			int i;
			parser->pos++;
			switch (js[parser->pos]) {
			/* Allowed escaped symbols */
			case '\"': case '/' : case '\\' : case 'b' :
			case 'f' : case 'r' : case 'n'  : case 't' :
				break;
				/* Allows escaped symbol \uXXXX */
			case 'u':
				parser->pos++;
				for(i = 0; i < 4 && parser->pos < len && js[parser->pos] != '\0'; i++) {
					/* If it isn't a hex character we have an error */
					if(!((js[parser->pos] >= 48 && js[parser->pos] <= 57) || /* 0-9 */
						 (js[parser->pos] >= 65 && js[parser->pos] <= 70) || /* A-F */
						 (js[parser->pos] >= 97 && js[parser->pos] <= 102))) { /* a-f */
						parser->pos = start;
						return JSMN_ERROR_INVAL;
					}
					parser->pos++;
				}
				parser->pos--;
				break;
				/* Unexpected symbol */
			default:
				parser->pos = start;
				return JSMN_ERROR_INVAL;
			}
		}
	}
	parser->pos = start;
	return JSMN_ERROR_PART;
}

/**
 * Parse JSON string and fill tokens.
 */
static int jsmn_parse(jsmn_parser *parser, const char *js, size_t len,
		   jsmntok_t *tokens, size_t num_tokens) {
	int r;
	int i;
	jsmntok_t *token;
	int count = parser->toknext;

	for (; parser->pos < len && js[parser->pos] != '\0'; parser->pos++) {
		char c;
		jsmntype_t type;

		c = js[parser->pos];
		switch (c) {
		case '{': case '[':
			count++;
			if (tokens == NULL) {
				break;
			}
			token = jsmn_alloc_token(parser, tokens, num_tokens);
			if (token == NULL)
				return JSMN_ERROR_NOMEM;
			if (parser->toksuper != -1) {
				tokens[parser->toksuper].size++;
#ifdef JSMN_PARENT_LINKS
				token->parent = parser->toksuper;
#endif
			}
			token->type = (c == '{' ? JSMN_OBJECT : JSMN_ARRAY);
			token->start = parser->pos;
			parser->toksuper = parser->toknext - 1;
			break;
		case '}': case ']':
			if (tokens == NULL)
				break;
			type = (c == '}' ? JSMN_OBJECT : JSMN_ARRAY);
#ifdef JSMN_PARENT_LINKS
			if (parser->toknext < 1) {
				return JSMN_ERROR_INVAL;
			}
			token = &tokens[parser->toknext - 1];
			for (;;) {
				if (token->start != -1 && token->end == -1) {
					if (token->type != type) {
						return JSMN_ERROR_INVAL;
					}
					token->end = parser->pos + 1;
					parser->toksuper = token->parent;
					break;
				}
				if (token->parent == -1) {
					if(token->type != type || parser->toksuper == -1) {
						return JSMN_ERROR_INVAL;
					}
					break;
				}
				token = &tokens[token->parent];
			}
#else
			for (i = parser->toknext - 1; i >= 0; i--) {
				token = &tokens[i];
				if (token->start != -1 && token->end == -1) {
					if (token->type != type) {
						return JSMN_ERROR_INVAL;
					}
					parser->toksuper = -1;
					token->end = parser->pos + 1;
					break;
				}
			}
			/* Error if unmatched closing bracket */
			if (i == -1) return JSMN_ERROR_INVAL;
			for (; i >= 0; i--) {
				token = &tokens[i];
				if (token->start != -1 && token->end == -1) {
					parser->toksuper = i;
					break;
				}
			}
#endif
			break;
		case '\"':
			r = jsmn_parse_string(parser, js, len, tokens, num_tokens);
			if (r < 0) return r;
			count++;
			if (parser->toksuper != -1 && tokens != NULL)
				tokens[parser->toksuper].size++;
			break;
		case '\t' : case '\r' : case '\n' : case ' ':
			break;
		case ':':
			parser->toksuper = parser->toknext - 1;
			break;
		case ',':
			if (tokens != NULL && parser->toksuper != -1 &&
					tokens[parser->toksuper].type != JSMN_ARRAY &&
					tokens[parser->toksuper].type != JSMN_OBJECT) {
#ifdef JSMN_PARENT_LINKS
				parser->toksuper = tokens[parser->toksuper].parent;
#else
				for (i = parser->toknext - 1; i >= 0; i--) {
					if (tokens[i].type == JSMN_ARRAY || tokens[i].type == JSMN_OBJECT) {
						if (tokens[i].start != -1 && tokens[i].end == -1) {
							parser->toksuper = i;
							break;
						}
					}
				}
#endif
			}
			break;
#ifdef JSMN_STRICT
			/* In strict mode primitives are: numbers and booleans */
		case '-': case '0': case '1' : case '2': case '3' : case '4':
		case '5': case '6': case '7' : case '8': case '9':
		case 't': case 'f': case 'n' :
			/* And they must not be keys of the object */
			if (tokens != NULL && parser->toksuper != -1) {
				jsmntok_t *t = &tokens[parser->toksuper];
				if (t->type == JSMN_OBJECT ||
						(t->type == JSMN_STRING && t->size != 0)) {
					return JSMN_ERROR_INVAL;
				}
			}
#else
			/* In non-strict mode every unquoted value is a primitive */
		default:
#endif
			r = jsmn_parse_primitive(parser, js, len, tokens, num_tokens);
			if (r < 0) return r;
			count++;
			if (parser->toksuper != -1 && tokens != NULL)
				tokens[parser->toksuper].size++;
			break;

#ifdef JSMN_STRICT
			/* Unexpected char in strict mode */
		default:
			return JSMN_ERROR_INVAL;
#endif
		}
	}

	if (tokens != NULL) {
		for (i = parser->toknext - 1; i >= 0; i--) {
			/* Unmatched opened object or array */
			if (tokens[i].start != -1 && tokens[i].end == -1) {
				return JSMN_ERROR_PART;
			}
		}
	}

	return count;
}

/**
 * Creates a new parser based over a given  buffer with an array of tokens
 * available.
 */
static void jsmn_init(jsmn_parser *parser) {
	parser->pos = 0;
	parser->toknext = 0;
	parser->toksuper = -1;
}
/*
 * -- jsmn.c end --
 */

#endif /* #ifdef ASSETDOC_IMPLEMENTATION */

