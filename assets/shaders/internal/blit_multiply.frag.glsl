#version 310 es
#extension GL_EXT_texture_buffer : enable
#extension GL_EXT_texture_cube_map_array : enable

#if defined(GL_ES) || defined(VULKAN)
	precision highp int;
	precision mediump float;
#endif

/*
UNIFORM_BLOCKS
    SharedDataBlock : $ubSharedDataLoc
PERM @MSAA_4
*/
        
#include "_fs_common.glsl"

#if defined(VULKAN) || defined(GL_SPIRV)
layout (binding = REN_UB_SHARED_DATA_LOC, std140)
#else
layout (std140)
#endif
uniform SharedDataBlock {
    SharedData shrd_data;
};

layout(binding = REN_REFL_SSR_TEX_SLOT) uniform sampler2D s_texture;
#if defined(MSAA_4)
layout(binding = REN_REFL_SPEC_TEX_SLOT) uniform mediump sampler2DMS s_mul_texture;
layout(binding = REN_REFL_DEPTH_TEX_SLOT) uniform mediump sampler2DMS s_depth_texture;
layout(binding = REN_REFL_NORM_TEX_SLOT) uniform mediump sampler2DMS s_norm_texture;
#else
layout(binding = REN_REFL_SPEC_TEX_SLOT) uniform mediump sampler2D s_mul_texture;
layout(binding = REN_REFL_DEPTH_TEX_SLOT) uniform mediump sampler2DMS s_depth_texture;
layout(binding = REN_REFL_NORM_TEX_SLOT) uniform mediump sampler2D s_norm_texture;
#endif
layout(binding = REN_REFL_PREV_TEX_SLOT) uniform mediump sampler2D prev_texture;
layout(binding = REN_REFL_BRDF_TEX_SLOT) uniform sampler2D brdf_lut_texture;
layout(binding = REN_ENV_TEX_SLOT) uniform mediump samplerCubeArray env_texture;
layout(binding = REN_CELLS_BUF_SLOT) uniform highp usamplerBuffer cells_buffer;
layout(binding = REN_ITEMS_BUF_SLOT) uniform highp usamplerBuffer items_buffer;

#if defined(VULKAN) || defined(GL_SPIRV)
layout(location = 0) in vec2 aVertexUVs_;
#else
in vec2 aVertexUVs_;
#endif

layout(location = 0) out vec4 outColor;

float LinearDepthTexelFetch(ivec2 hit_pixel) {
    float depth = texelFetch(s_depth_texture, hit_pixel, 0).r;
    return shrd_data.uClipInfo[0] / (depth * (shrd_data.uClipInfo[1] - shrd_data.uClipInfo[2]) + shrd_data.uClipInfo[2]);
}

void main() {
#if defined(MSAA_4)
    vec4 specular = 0.25 * (texelFetch(s_mul_texture, ivec2(aVertexUVs_), 0) +
                            texelFetch(s_mul_texture, ivec2(aVertexUVs_), 1) +
                            texelFetch(s_mul_texture, ivec2(aVertexUVs_), 2) +
                            texelFetch(s_mul_texture, ivec2(aVertexUVs_), 3));
#else
    vec4 specular = texelFetch(s_mul_texture, ivec2(aVertexUVs_), 0);
#endif
    if ((specular.r + specular.g + specular.b) < 0.0001) return;

    ivec2 pix_uvs = ivec2(aVertexUVs_ / 2.0) * 2;

    const ivec2 offsets[] = ivec2[4](
        ivec2(0, 0), ivec2(1, 0), ivec2(0, 1), ivec2(1, 1)
    );

    float norm_weights[4];
    vec3 normal = 2.0 * texelFetch(s_norm_texture, ivec2(aVertexUVs_), 0).xyz - 1.0;
    for (int i = 0; i < 4; i++) {
        vec3 norm_coarse = 2.0 * texelFetch(s_norm_texture, pix_uvs + 2 * offsets[i], 0).xyz - 1.0;
        norm_weights[i] = 0.0 + step(0.8, dot(norm_coarse, normal));
    }

    float depth_weights[4];
    float depth = texelFetch(s_depth_texture, ivec2(aVertexUVs_), 0).r;
    float lin_depth = shrd_data.uClipInfo[0] / (depth * (shrd_data.uClipInfo[1] - shrd_data.uClipInfo[2]) + shrd_data.uClipInfo[2]);
    for (int i = 0; i < 4; i++) {
        float depth_coarse = LinearDepthTexelFetch(pix_uvs + 2 * offsets[i]);
        depth_weights[i] = 1.0 / (0.01 + abs(lin_depth - depth_coarse));
    }

    vec2 sample_coord = fract((aVertexUVs_ - vec2(0.5)) / 2.0);

    float sample_weights[4];
    sample_weights[0] = (1.0 - sample_coord.x) * (1.0 - sample_coord.y);
    sample_weights[1] = (sample_coord.x) * (1.0 - sample_coord.y);
    sample_weights[2] = (1.0 - sample_coord.x) * sample_coord.y;
    sample_weights[3] = (sample_coord.x) * sample_coord.y;

    vec3 ssr_uvs = vec3(0.0);
    float weight_sum = 0.0, weight_sum1 = 0.0;
    for (int i = 0; i < 4; i++) {
        float weight = sample_weights[i] * norm_weights[i] * depth_weights[i];
        vec3 uvs = texelFetch(s_texture, ivec2((aVertexUVs_ - vec2(0.5)) / 2.0) + offsets[i], 0).xyz;

        ssr_uvs.b += uvs.b * weight;
        weight_sum1 += weight;
        
        if (uvs.b > 0.0001) {
            ssr_uvs.rg += uvs.rg * weight;
            weight_sum += weight;
        }
    }

    if (weight_sum > 0.0001) {
        ssr_uvs.rg /= weight_sum;
        ssr_uvs.b /= weight_sum1;
    }

    float tex_lod = 6.0 * specular.a;
    float N_dot_V;

    vec3 c0 = vec3(0.0);
    vec2 brdf;

    {   // apply cubemap contribution
        vec4 ray_origin_cs = vec4(aVertexUVs_.xy / shrd_data.uResAndFRes.xy, 2.0 * depth - 1.0, 1.0);
        ray_origin_cs.xy = 2.0 * ray_origin_cs.xy - 1.0;

        vec4 ray_origin_vs = shrd_data.uInvProjMatrix * ray_origin_cs;
        ray_origin_vs /= ray_origin_vs.w;

        vec3 view_ray_ws = normalize((shrd_data.uInvViewMatrix * vec4(ray_origin_vs.xyz, 0.0)).xyz);
        vec3 refl_ray_ws = reflect(view_ray_ws, normal);

        vec4 ray_origin_ws = shrd_data.uInvViewMatrix * ray_origin_vs;
        ray_origin_ws /= ray_origin_ws.w;

        highp float k = log2(lin_depth / shrd_data.uClipInfo[1]) / shrd_data.uClipInfo[3];
        int slice = int(floor(k * float(REN_GRID_RES_Z)));
    
        int ix = int(aVertexUVs_.x), iy = int(aVertexUVs_.y);
        int cell_index = slice * REN_GRID_RES_X * REN_GRID_RES_Y + (iy * REN_GRID_RES_Y / int(shrd_data.uResAndFRes.y)) * REN_GRID_RES_X + (ix * REN_GRID_RES_X / int(shrd_data.uResAndFRes.x));
        
        highp uvec2 cell_data = texelFetch(cells_buffer, cell_index).xy;
        highp uint offset = bitfieldExtract(cell_data.x, 0, 24);
        highp uint pcount = bitfieldExtract(cell_data.y, 8, 8);

        float total_fade = 0.0;

        for (uint i = offset; i < offset + pcount; i++) {
            highp uint item_data = texelFetch(items_buffer, int(i)).x;
            int pi = int(bitfieldExtract(item_data, 24, 8));

            float dist = distance(shrd_data.uProbes[pi].pos_and_radius.xyz, ray_origin_ws.xyz);
            float fade = 1.0 - smoothstep(0.9, 1.0, dist / shrd_data.uProbes[pi].pos_and_radius.w);
            c0 += fade * RGBMDecode(textureLod(env_texture, vec4(refl_ray_ws, shrd_data.uProbes[pi].unused_and_layer.w), tex_lod));
            total_fade += fade;
        }

        c0 /= max(total_fade, 1.0);

        N_dot_V = clamp(dot(normal, -view_ray_ws), 0.0, 1.0);
        brdf = texture(brdf_lut_texture, vec2(N_dot_V, specular.a)).xy;
    }

    vec3 kS = FresnelSchlickRoughness(N_dot_V, specular.rgb, specular.a);

    c0 = mix(c0, textureLod(prev_texture, ssr_uvs.rg, /*tex_lod*/ 0.0).xyz, ssr_uvs.b);
    outColor = vec4(c0 * (kS * brdf.x + brdf.y), 1.0);
}
