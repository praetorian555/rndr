// PBR shader for Canvas API.
// Replaces pbr.vert + pbr.frag + pbr-shared.glsl + alpha-test.glsl.
// Uses a material_flags field to dynamically decide which textures to sample,
// eliminating the need for shader permutations.
//
// Per-instance data lives in an SSBO. The vertex shader reads all fields and
// passes material parameters to the fragment shader via flat-interpolated
// outputs, so only the vertex stage accesses the SSBO. This avoids cross-stage
// SSBO layout mismatches during OpenGL program linking.

static const float k_pi = 3.141592653589793;
static const uint k_max_light_count = 4;

// Material flag bits, matching PbrRenderer::k_flag_* values.
static const uint k_flag_albedo_texture = 1 << 0;
static const uint k_flag_emissive_texture = 1 << 1;
static const uint k_flag_metallic_roughness_texture = 1 << 2;
static const uint k_flag_normal_texture = 1 << 3;
static const uint k_flag_ambient_occlusion_texture = 1 << 4;
static const uint k_flag_opacity_texture = 1 << 5;

// Draw flags
static const uint k_flag_draw_unlit = 1 << 0;
static const uint k_flag_draw_normals = 1 << 1;

// ---------------------------------------------------------------------------
// Per-instance data (stored in an SSBO, only accessed by the vertex shader)
// ---------------------------------------------------------------------------

struct InstanceData
{
    float4x4 model_transform;
    float4x4 normal_transform;
    float4 albedo_color;
    float4 emissive_color;
    float4 roughness;
    float metallic_factor;
    float transparency_factor;
    float alpha_test;
    uint material_flags;
};

StructuredBuffer<InstanceData> instances;

// ---------------------------------------------------------------------------
// Vertex shader
// ---------------------------------------------------------------------------

struct VertexInput
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 tex_coord : TEXCOORD;
};

struct VertexOutput
{
    float4 sv_position : SV_POSITION;
    float3 normal_world : NORMAL;
    float2 tex_coord : TEXCOORD0;
    float3 position_world : TEXCOORD1;
    nointerpolation float4 albedo_color : TEXCOORD2;
    nointerpolation float4 emissive_color : TEXCOORD3;
    nointerpolation float4 roughness : TEXCOORD4;
    nointerpolation float metallic_factor : TEXCOORD5;
    nointerpolation float transparency_factor : TEXCOORD6;
    nointerpolation float alpha_test : TEXCOORD7;
    nointerpolation uint material_flags : TEXCOORD8;
};

// Per-frame uniforms shared across vertex and fragment stages. Grouped into a
// single cbuffer so they map to one UBO binding point instead of one per field.
cbuffer FrameConstants
{
    float4x4 view_projection;
    float3 camera_position;
    uint directional_light_count;
    float4 directional_light_directions[k_max_light_count];
    float4 directional_light_colors[k_max_light_count];
    uint point_light_count;
    float4 point_light_positions[k_max_light_count];
    float4 point_light_colors[k_max_light_count];
    uint draw_flags;
};

[shader("vertex")]
VertexOutput VertexMain(VertexInput vin, uint instance_id : SV_VulkanInstanceID)
{
    InstanceData inst = instances[instance_id];
    VertexOutput vertex_out;
    float4 world_pos = mul(inst.model_transform, float4(vin.position, 1.0));
    vertex_out.sv_position = mul(view_projection, world_pos);
    vertex_out.position_world = world_pos.xyz;
    vertex_out.normal_world = normalize(mul((float3x3)inst.normal_transform, vin.normal));
    vertex_out.tex_coord = vin.tex_coord;
    vertex_out.albedo_color = inst.albedo_color;
    vertex_out.emissive_color = inst.emissive_color;
    vertex_out.roughness = inst.roughness;
    vertex_out.metallic_factor = inst.metallic_factor;
    vertex_out.transparency_factor = inst.transparency_factor;
    vertex_out.alpha_test = inst.alpha_test;
    vertex_out.material_flags = inst.material_flags;
    return vertex_out;
}

// ---------------------------------------------------------------------------
// Fragment shader
// ---------------------------------------------------------------------------

// Texture samplers. Unused slots should be bound to a dummy 1x1 texture.
uniform Sampler2D albedo_texture;
uniform Sampler2D emissive_texture;
uniform Sampler2D metallic_roughness_texture;
uniform Sampler2D normal_texture;
uniform Sampler2D ambient_occlusion_texture;
uniform Sampler2D opacity_texture;

// ---------------------------------------------------------------------------
// PBR helpers (ported from pbr-shared.glsl)
// ---------------------------------------------------------------------------

struct PbrInfo
{
    float n_dot_l;
    float n_dot_v;
    float n_dot_h;
    float l_dot_h;
    float v_dot_h;
    float perceptual_roughness;
    float3 reflectance0;
    float3 reflectance90;
    float alpha_roughness;
    float3 diffuse_color;
    float3 specular_color;
    float3 normal;
    float3 vec_from_surface_to_camera;
};

float4 SrgbToLinear(float4 in_srgb)
{
    float3 out_lin = pow(in_srgb.xyz, float3(2.2));
    return float4(out_lin, in_srgb.a);
}

// Disney diffuse BRDF.
float3 DiffuseBurley(PbrInfo info)
{
    float f90 = 2.0 * info.l_dot_h * info.l_dot_h * info.alpha_roughness - 0.5;
    return (info.diffuse_color / k_pi)
        * (1.0 + f90 * pow(1.0 - info.n_dot_l, 5.0))
        * (1.0 + f90 * pow(1.0 - info.n_dot_v, 5.0));
}

// Fresnel-Schlick.
float3 SpecularReflection(PbrInfo info)
{
    return info.reflectance0
        + (info.reflectance90 - info.reflectance0) * pow(saturate(1.0 - info.v_dot_h), 5.0);
}

// Smith GGX geometric occlusion.
float GeometricOcclusion(PbrInfo info)
{
    float n_dot_l = info.n_dot_l;
    float n_dot_v = info.n_dot_v;
    float r_sqr = info.alpha_roughness * info.alpha_roughness;
    float attenuation_l = 2.0 * n_dot_l / (n_dot_l + sqrt(r_sqr + (1.0 - r_sqr) * (n_dot_l * n_dot_l)));
    float attenuation_v = 2.0 * n_dot_v / (n_dot_v + sqrt(r_sqr + (1.0 - r_sqr) * (n_dot_v * n_dot_v)));
    return attenuation_l * attenuation_v;
}

// GGX/Trowbridge-Reitz microfacet distribution.
float MicrofacetDistribution(PbrInfo info)
{
    float roughness_sq = info.alpha_roughness * info.alpha_roughness;
    float f = (info.n_dot_h * roughness_sq - info.n_dot_h) * info.n_dot_h + 1.0;
    return roughness_sq / (k_pi * f * f);
}

PbrInfo CalculatePBRInputs(float4 albedo, float3 n, float3 cam_pos, float3 pos_world, float4 mr_sample)
{
    float perceptual_roughness = clamp(mr_sample.g, 0.04, 1.0);
    float metallic = clamp(mr_sample.b, 0.0, 1.0);
    float alpha_r = perceptual_roughness * perceptual_roughness;

    float3 f0 = float3(0.04);
    float3 diff_color = albedo.rgb * (float3(1.0) - f0) * (1.0 - metallic);
    float3 spec_color = lerp(f0, albedo.rgb, metallic);
    float reflectance = max(max(spec_color.r, spec_color.g), spec_color.b);
    float reflectance90 = saturate(reflectance * 25.0);

    float3 v = normalize(cam_pos - pos_world);

    PbrInfo info;
    info.n_dot_l = 0;
    info.n_dot_h = 0;
    info.l_dot_h = 0;
    info.v_dot_h = 0;
    info.n_dot_v = clamp(abs(dot(n, v)), 0.001, 1.0);
    info.perceptual_roughness = perceptual_roughness;
    info.reflectance0 = spec_color;
    info.reflectance90 = float3(reflectance90);
    info.alpha_roughness = alpha_r;
    info.diffuse_color = diff_color;
    info.specular_color = spec_color;
    info.normal = n;
    info.vec_from_surface_to_camera = v;
    return info;
}

float3 CalculatePBRLightContribution(inout PbrInfo info, float3 light_dir, float3 light_color)
{
    float3 n = info.normal;
    float3 v = info.vec_from_surface_to_camera;
    float3 l = normalize(light_dir);
    float3 h = normalize(l + v);

    info.n_dot_l = clamp(dot(n, l), 0.001, 1.0);
    info.n_dot_h = saturate(dot(n, h));
    info.l_dot_h = saturate(dot(l, h));
    info.v_dot_h = saturate(dot(v, h));

    float3 f = SpecularReflection(info);
    float g = GeometricOcclusion(info);
    float d = MicrofacetDistribution(info);

    float3 diffuse_contrib = (1.0 - f) * DiffuseBurley(info);
    float3 spec_contrib = f * g * d / (4.0 * info.n_dot_l * info.n_dot_v);
    return info.n_dot_l * light_color * (diffuse_contrib + spec_contrib);
}

// ---------------------------------------------------------------------------
// Normal mapping helpers (ported from pbr-shared.glsl)
// ---------------------------------------------------------------------------

float3x3 CotangentFrame(float3 n, float3 pos, float2 uv)
{
    float3 dp1 = ddx(pos);
    float3 dp2 = ddy(pos);
    float2 duv1 = ddx(uv);
    float2 duv2 = ddy(uv);

    float3 dp2perp = cross(dp2, n);
    float3 dp1perp = cross(n, dp1);
    float3 t = dp2perp * duv1.x + dp1perp * duv2.x;
    float3 b = dp2perp * duv1.y + dp1perp * duv2.y;

    float inv_max = rsqrt(max(dot(t, t), dot(b, b)));
    float w = (dot(cross(n, t), b) < 0.0) ? -1.0 : 1.0;
    t = t * w;
    return float3x3(t * inv_max, b * inv_max, n);
}

float3 PerturbNormal(float3 n, float3 v, float3 normal_sample, float2 uv)
{
    float3 map = normalize(2.0 * normal_sample - float3(1.0));
    float3x3 tbn = CotangentFrame(n, v, uv);
    return normalize(mul(tbn, map));
}

// ---------------------------------------------------------------------------
// Alpha test (ported from alpha-test.glsl)
// ---------------------------------------------------------------------------

void RunAlphaTest(float alpha, float alpha_threshold, float2 frag_coord)
{
    if (alpha_threshold > 0.0)
    {
        // Screen-door transparency dithering.
        // http://alex-charlton.com/posts/Dithering_on_the_GPU/
        static const float threshold_matrix[4][4] = {
            { 1.0 / 17.0,  9.0 / 17.0,  3.0 / 17.0, 11.0 / 17.0},
            {13.0 / 17.0,  5.0 / 17.0, 15.0 / 17.0,  7.0 / 17.0},
            { 4.0 / 17.0, 12.0 / 17.0,  2.0 / 17.0, 10.0 / 17.0},
            {16.0 / 17.0,  8.0 / 17.0, 14.0 / 17.0,  6.0 / 17.0},
        };
        int ix = int(fmod(frag_coord.x, 4.0));
        int iy = int(fmod(frag_coord.y, 4.0));
        alpha = saturate(alpha - 0.5 * threshold_matrix[ix][iy]);
        if (alpha < alpha_threshold)
        {
            discard;
        }
    }
}

// ---------------------------------------------------------------------------
// Fragment entry point
// ---------------------------------------------------------------------------

[shader("fragment")]
float4 FragmentMain(VertexOutput pin)
{
    float2 uv = pin.tex_coord;
    uint flags = pin.material_flags;

    // Albedo.
    float4 frag_albedo = pin.albedo_color;
    if ((flags & k_flag_albedo_texture) != 0)
    {
        frag_albedo = albedo_texture.Sample(uv);
    }

    // Opacity.
    if ((flags & k_flag_opacity_texture) != 0)
    {
        frag_albedo.a = opacity_texture.Sample(uv).a;
    }

    // Alpha test.
    RunAlphaTest(frag_albedo.a, pin.alpha_test, pin.sv_position.xy);

    if ((draw_flags & k_flag_draw_unlit) != 0)
    {
         return frag_albedo;
    }

    // Normal.
    float3 n = normalize(pin.normal_world);
    if ((flags & k_flag_normal_texture) != 0)
    {
        float3 normal_sample = normal_texture.Sample(uv).rgb;
        if (length(normal_sample) > 0.5)
        {
            n = PerturbNormal(n, normalize(camera_position - pin.position_world), normal_sample, uv);
        }
    }

    if ((draw_flags & k_flag_draw_normals) != 0)
    {
        float3 normal_as_color = (n + 1.0) / 2;
        return float4(normal_as_color, frag_albedo.a);
    }

    // Metallic-roughness.
    float4 mr = pin.roughness;
    mr.b = pin.metallic_factor;
    if ((flags & k_flag_metallic_roughness_texture) != 0)
    {
        mr = metallic_roughness_texture.Sample(uv);
    }

    // PBR lighting.
    PbrInfo pbr = CalculatePBRInputs(frag_albedo, n, camera_position, pin.position_world, mr);
    float3 color = float3(0);

    for (uint i = 0; i < directional_light_count && i < k_max_light_count; ++i)
    {
        float3 dir = normalize(directional_light_directions[i].xyz);
        color += CalculatePBRLightContribution(pbr, dir, directional_light_colors[i].xyz);
    }
    for (uint i = 0; i < point_light_count && i < k_max_light_count; ++i)
    {
        float3 dir = normalize(point_light_positions[i].xyz - pin.position_world);
        color += CalculatePBRLightContribution(pbr, dir, point_light_colors[i].xyz);
    }

    // Ambient occlusion.
    if ((flags & k_flag_ambient_occlusion_texture) != 0)
    {
        float ao = ambient_occlusion_texture.Sample(uv).r;
        color = color * (ao < 0.01 ? 1.0 : ao);
    }

    // Emissive.
    float4 frag_emissive = pin.emissive_color;
    if ((flags & k_flag_emissive_texture) != 0)
    {
        frag_emissive = emissive_texture.Sample(uv);
    }
    frag_emissive.rgb = SrgbToLinear(frag_emissive).rgb;

    // Tone mapping (gamma correction).
    color = pow(frag_emissive.rgb + color, float3(1.0 / 2.2));

    return float4(color, frag_albedo.a);
}
