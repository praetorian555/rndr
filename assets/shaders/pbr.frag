#version 460 core

layout(std140, binding = 0) uniform PerFrameData
{
    mat4 view_projection_transform;
    vec3 camera_position_world;
    uint directional_light_count;
    vec4 light_direction_world[4];
    vec4 directional_light_color[4];
    vec4 light_position_world[4];
    vec4 point_light_color[4];
    uint point_light_count;
};

layout (binding = 0) uniform sampler2D albedo_texture;
layout (binding = 1) uniform sampler2D emissive_texture;
layout (binding = 2) uniform sampler2D metalic_roughness_texture;
layout (binding = 3) uniform sampler2D normal_texture;
layout (binding = 4) uniform sampler2D ambient_occlusion_texture;
layout (binding = 5) uniform sampler2D opacity_texture;

layout (location = 0) in vec3 in_normal_world;
layout (location = 1) in vec2 in_tex_coords;
layout (location = 2) in vec3 in_position_world;
layout (location = 3) in vec4 in_albedo_color;
layout (location = 4) in vec4 in_emissive_color;
layout (location = 5) in vec4 in_roughness;
layout (location = 6) in float in_transparency_factor;
layout (location = 7) in float in_alpha_test;
layout (location = 8) in float in_metalic_factor;
layout (location = 9) in flat uint in_flags;

layout (location = 0) out vec4 out_frag_color;

#include "alpha-test.glsl"
#include "pbr-shared.glsl"

vec4 NormalToColor(vec3 normal)
{
    return vec4((normal + 1) * 0.5f, 1.0f);
}

bool CompareVec3(vec3 expected, vec3 actual, float eps)
{
    return abs(expected.x - actual.x) <= eps && abs(expected.y - actual.y) <= eps && abs(expected.z - actual.z) <= eps;
}

void main()
{
    // Figure out the albedo color
    vec4 albedo_color = in_albedo_color;
#if defined(USE_ALBEDO_TEXTURE)
    albedo_color = texture(albedo_texture, in_tex_coords);
#endif

    // Load opacity of the pixel if opacity texture is used
#if defined(USE_OPACITY_TEXTURE)
    float opacity = texture(opacity_texture, in_tex_coords).a;
    albedo_color.a = opacity;
#endif

    // If alpha test fails, discard this fragment
    RunAlphaTest(albedo_color.a, in_alpha_test);

    // Load normal of the surface in the world corresponding to this pixel
    vec3 normal_world = normalize(in_normal_world);
#if defined(USE_NORMAL_TEXTURE)
    vec3 normal_sample = texture(normal_texture, in_tex_coords).rgb;
    if (length(normal_sample) > 0.5)
    {
        normal_world = PerturbNormal(normal_world, normalize(camera_position_world - in_position_world), normal_sample, in_tex_coords);
    }
#endif

    // Get metalic and roughness parameters
    vec4 mr = in_roughness;
    mr.b = in_metalic_factor;
#if defined(USE_METALLIC_ROUGHNESS_TEXTURE)
    mr = texture(metalic_roughness_texture, in_tex_coords);
#endif

    // Calculate the color based on the light
    PbrInfo pbr_inputs;
    CalculatePBRInputsMetallicRoughness(albedo_color, normal_world, camera_position_world.xyz, in_position_world, mr, pbr_inputs);
    vec3 color = vec3(0);
    for (int i = 0; i < directional_light_count; ++i)
    {
        vec3 direction = normalize(vec3(light_direction_world[i]));
        color += CalculatePBRLightContribution(pbr_inputs, direction, vec3(directional_light_color[i]));
    }
    for (int i = 0; i < point_light_count; ++i)
    {
        vec3 direction = vec3(light_position_world[i]) - in_position_world;
        direction = normalize(direction);
        color += CalculatePBRLightContribution(pbr_inputs, direction, vec3(point_light_color[i]));
    }

    // Modify output color using ambient occlusion
#if defined(USE_AMBIENT_OCCLUSION_TEXTURE)
    vec4 kao = texture(ambient_occlusion_texture, in_tex_coords);
    color = color * (kao.r < 0.01 ? 1.0 : kao.r);
#endif

    // Modify output color if there is emissive color set
    vec4 emissive_color = in_emissive_color;
#if defined(USE_EMISSIVE_TEXTURE)
    emissive_color = texture(emissive_texture, in_tex_coords);
#endif
    emissive_color.rgb = SrgbToLinear(emissive_color).rgb;
    color = pow(emissive_color.rgb + color, vec3(1.0 / 2.2));

    out_frag_color = vec4(color, albedo_color.a);
}
