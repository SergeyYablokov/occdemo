#version 310 es
#extension GL_EXT_texture_buffer : enable
#extension GL_OES_texture_buffer : enable
#extension GL_EXT_texture_cube_map_array : enable
#extension GL_ARB_bindless_texture: enable
//#extension GL_EXT_control_flow_attributes : enable

$ModifyWarning

#ifdef GL_ES
    precision mediump float;
    precision mediump sampler2DShadow;
#endif

#include "internal/_fs_common.glsl"

#define LIGHT_ATTEN_CUTOFF 0.004

#if !defined(GL_ARB_bindless_texture)
layout(binding = REN_MAT_TEX0_SLOT) uniform sampler2D diff_texture;
layout(binding = REN_MAT_TEX1_SLOT) uniform sampler2D norm_texture;
layout(binding = REN_MAT_TEX2_SLOT) uniform sampler2D spec_texture;
#endif // GL_ARB_bindless_texture
layout(binding = REN_SHAD_TEX_SLOT) uniform sampler2DShadow shadow_texture;
layout(binding = REN_DECAL_TEX_SLOT) uniform sampler2D decals_texture;
layout(binding = REN_SSAO_TEX_SLOT) uniform sampler2D ao_texture;
layout(binding = REN_ENV_TEX_SLOT) uniform mediump samplerCubeArray env_texture;
layout(binding = REN_BRDF_TEX_SLOT) uniform sampler2D brdf_lut_texture;
layout(binding = REN_LIGHT_BUF_SLOT) uniform mediump samplerBuffer lights_buffer;
layout(binding = REN_DECAL_BUF_SLOT) uniform mediump samplerBuffer decals_buffer;
layout(binding = REN_CELLS_BUF_SLOT) uniform highp usamplerBuffer cells_buffer;
layout(binding = REN_ITEMS_BUF_SLOT) uniform highp usamplerBuffer items_buffer;

#if defined(VULKAN) || defined(GL_SPIRV)
layout (binding = 0, std140)
#else
layout (std140)
#endif
uniform SharedDataBlock {
    SharedData shrd_data;
};

#if defined(VULKAN) || defined(GL_SPIRV)
layout(location = 0) in highp vec3 aVertexPos_;
layout(location = 1) in mediump vec2 aVertexUVs_;
layout(location = 2) in mediump vec3 aVertexNormal_;
layout(location = 3) in mediump vec3 aVertexTangent_;
layout(location = 4) in highp vec3 aVertexShUVs_[4];
#if defined(GL_ARB_bindless_texture)
layout(location = 8) in flat uvec2 diff_texture;
layout(location = 9) in flat uvec2 norm_texture;
layout(location = 10) in flat uvec2 spec_texture;
#endif // GL_ARB_bindless_texture
#else
in highp vec3 aVertexPos_;
in mediump vec2 aVertexUVs_;
in mediump vec3 aVertexNormal_;
in mediump vec3 aVertexTangent_;
in highp vec3 aVertexShUVs_[4];
#if defined(GL_ARB_bindless_texture)
in flat uvec2 diff_texture;
in flat uvec2 norm_texture;
in flat uvec2 spec_texture;
#endif // GL_ARB_bindless_texture
#endif

layout(location = REN_OUT_COLOR_INDEX) out vec4 outColor;
layout(location = REN_OUT_NORM_INDEX) out vec4 outNormal;
layout(location = REN_OUT_SPEC_INDEX) out vec4 outSpecular;

void main(void) {
    highp float lin_depth = LinearizeDepth(gl_FragCoord.z, shrd_data.uClipInfo);
    highp float k = log2(lin_depth / shrd_data.uClipInfo[1]) / shrd_data.uClipInfo[3];
    int slice = int(floor(k * float(REN_GRID_RES_Z)));
    
    int ix = int(gl_FragCoord.x), iy = int(gl_FragCoord.y);
    int cell_index = GetCellIndex(ix, iy, slice, shrd_data.uResAndFRes.xy);
    
    highp uvec2 cell_data = texelFetch(cells_buffer, cell_index).xy;
    highp uvec2 offset_and_lcount = uvec2(bitfieldExtract(cell_data.x, 0, 24),
                                          bitfieldExtract(cell_data.x, 24, 8));
    highp uvec2 dcount_and_pcount = uvec2(bitfieldExtract(cell_data.y, 0, 8),
                                          bitfieldExtract(cell_data.y, 8, 8));
    
    vec4 diff_color = texture(SAMPLER2D(diff_texture), aVertexUVs_);
    
    vec2 duv_dx = dFdx(aVertexUVs_), duv_dy = dFdy(aVertexUVs_);
    vec3 normal_color = texture(SAMPLER2D(norm_texture), aVertexUVs_).wyz;
    vec4 specular_color = texture(SAMPLER2D(spec_texture), aVertexUVs_);
    
    vec3 normal = normal_color * 2.0 - 1.0;
    normal = normalize(mat3(cross(aVertexTangent_, aVertexNormal_), aVertexTangent_,
                            aVertexNormal_) * normal);
                            
    if (!gl_FrontFacing) {
        normal = -normal;
    }
    
    vec3 additional_light = vec3(0.0, 0.0, 0.0);
    
    for (uint i = offset_and_lcount.x; i < offset_and_lcount.x + offset_and_lcount.y; i++) {
        highp uint item_data = texelFetch(items_buffer, int(i)).x;
        int li = int(bitfieldExtract(item_data, 0, 12));

        vec4 pos_and_radius = texelFetch(lights_buffer, li * 3 + 0);
        highp vec4 col_and_index = texelFetch(lights_buffer, li * 3 + 1);
        vec4 dir_and_spot = texelFetch(lights_buffer, li * 3 + 2);
        
        vec3 L = pos_and_radius.xyz - aVertexPos_;
        float dist = length(L);
        float d = max(dist - pos_and_radius.w, 0.0);
        L /= dist;
        
        highp float denom = d / pos_and_radius.w + 1.0;
        highp float atten = 1.0 / (denom * denom);
        
        highp float brightness = max(col_and_index.x, max(col_and_index.y, col_and_index.z));
        
        highp float factor = LIGHT_ATTEN_CUTOFF / brightness;
        atten = (atten - factor) / (1.0 - LIGHT_ATTEN_CUTOFF);
        atten = max(atten, 0.0);
        
        float _dot1 = max(dot(L, normal), 0.0);
        float _dot2 = dot(L, dir_and_spot.xyz);
        
        atten = _dot1 * atten;
        if (_dot2 > dir_and_spot.w && (brightness * atten) > FLT_EPS) {
            int shadowreg_index = floatBitsToInt(col_and_index.w);
            if (shadowreg_index != -1) {
                vec4 reg_tr = shrd_data.uShadowMapRegions[shadowreg_index].transform;
                
                highp vec4 pp =
                    shrd_data.uShadowMapRegions[shadowreg_index].clip_from_world * vec4(aVertexPos_, 1.0);
                pp /= pp.w;
                pp.xyz = pp.xyz * 0.5 + vec3(0.5);
                pp.xy = reg_tr.xy + pp.xy * reg_tr.zw;
                
                atten *= SampleShadowPCF5x5(shadow_texture, pp.xyz);
            }
            
            additional_light += col_and_index.xyz * atten *
                smoothstep(dir_and_spot.w, dir_and_spot.w + 0.2, _dot2);
        }
    }
	
	vec3 view_ray_ws = normalize(aVertexPos_ - shrd_data.uCamPosAndGamma.xyz);
	vec3 refl_ray_ws = reflect(view_ray_ws, normal);
	
	float refl_lod = 6.0 * specular_color.a;
    
    vec3 indirect_col = vec3(0.0);
	vec3 reflected_col = vec3(0.0);
    float total_fade = 0.0;
    
    for (uint i = offset_and_lcount.x; i < offset_and_lcount.x + dcount_and_pcount.y; i++) {
        highp uint item_data = texelFetch(items_buffer, int(i)).x;
        int pi = int(bitfieldExtract(item_data, 24, 8));
        
        float dist = distance(shrd_data.uProbes[pi].pos_and_radius.xyz, aVertexPos_);
        float fade = 1.0 - smoothstep(0.9, 1.0, dist / shrd_data.uProbes[pi].pos_and_radius.w);
        
        indirect_col += fade * EvalSHIrradiance_NonLinear(normal,
                                                          shrd_data.uProbes[pi].sh_coeffs[0],
                                                          shrd_data.uProbes[pi].sh_coeffs[1],
                                                          shrd_data.uProbes[pi].sh_coeffs[2]);
		reflected_col += fade * RGBMDecode(
                textureLod(env_texture, vec4(refl_ray_ws,
                                             shrd_data.uProbes[pi].unused_and_layer.w), refl_lod));
        total_fade += fade;
    }
    
    indirect_col /= max(total_fade, 1.0);
    indirect_col = max(indirect_col, vec3(0.0));
    reflected_col /= max(total_fade, 1.0);
	
    float lambert = clamp(dot(normal, shrd_data.uSunDir.xyz), 0.0, 1.0);
    float visibility = 0.0;
    if (lambert > 0.00001) {
        visibility = GetSunVisibility(lin_depth, shadow_texture, aVertexShUVs_);
    }
    
    vec2 ao_uvs = vec2(ix, iy) / shrd_data.uResAndFRes.zw;
    float ambient_occlusion = textureLod(ao_texture, ao_uvs, 0.0).r;
    vec3 diffuse_color = diff_color.rgb * (shrd_data.uSunCol.xyz * lambert * visibility +
                                             ambient_occlusion * ambient_occlusion * indirect_col +
                                             additional_light);
    
    float N_dot_V = clamp(dot(normal, -view_ray_ws), 0.0, 1.0);
    
    vec3 kS = FresnelSchlickRoughness(N_dot_V, specular_color.rgb, specular_color.a);
	vec2 brdf = texture(brdf_lut_texture, vec2(N_dot_V, specular_color.a)).xy;

    outColor = vec4(diffuse_color * (1.0 - kS) + reflected_col * (kS * brdf.x + brdf.y), diff_color.a);
    //outNormal = vec4(normal * 0.5 + 0.5, 1.0);
    outSpecular = vec4(0.0);
}
