#version 310 es
#extension GL_ARB_texture_multisample : enable

#if defined(GL_ES) || defined(VULKAN)
	precision highp int;
	precision mediump float;
#endif

#include "_fs_common.glsl"

/*
UNIFORM_BLOCKS
    SharedDataBlock : $ubSharedDataLoc
PERM @MSAA_4
*/

#if defined(VULKAN) || defined(GL_SPIRV)
layout (binding = REN_UB_SHARED_DATA_LOC, std140)
#else
layout (std140)
#endif
uniform SharedDataBlock {
    SharedData shrd_data;
};

#if defined(VULKAN)
layout(push_constant) uniform PushConstants {
    layout(offset = 16) float uLinearize;
};
#else
layout(location = 1) uniform float uLinearize;
#endif

#if defined(MSAA_4)
layout(binding = REN_BASE0_TEX_SLOT) uniform highp sampler2DMS depth_texture;
#else
layout(binding = REN_BASE0_TEX_SLOT) uniform highp sampler2D depth_texture;
#endif

#if defined(VULKAN) || defined(GL_SPIRV)
layout(location = 0) in vec2 aVertexUVs_;
#else
in vec2 aVertexUVs_;
#endif

layout(location = 0) out float outColor;

void main() {
    highp ivec2 coord = ivec2(aVertexUVs_ - vec2(0.5));

    highp float d1 = texelFetch(depth_texture, coord + ivec2(0, 0), 0).r;
    highp float d2 = texelFetch(depth_texture, coord + ivec2(0, 1), 0).r;
    highp float d3 = texelFetch(depth_texture, coord + ivec2(1, 1), 0).r;
    highp float d4 = texelFetch(depth_texture, coord + ivec2(1, 0), 0).r;

    //highp float res_depth = max(max(d1, d2), max(d3, d4));
    highp float res_depth = min(min(d1, d2), min(d3, d4));
    if (uLinearize > 0.5) {
        outColor = LinearizeDepth(res_depth, shrd_data.uClipInfo);
    } else {
        outColor = res_depth;
    }
}
