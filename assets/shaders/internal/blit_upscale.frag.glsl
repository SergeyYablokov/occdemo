#version 310 es

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

#if defined(MSAA_4)
layout(binding = 0) uniform highp sampler2DMS depth_texture;
#else
layout(binding = 0) uniform highp sampler2D depth_texture;
#endif
layout(binding = 1) uniform mediump sampler2D depth_low_texture;
layout(binding = 2) uniform lowp sampler2D source_texture;

#if defined(VULKAN) || defined(GL_SPIRV)
layout(location = 0) in vec2 aVertexUVs_;
#else
in vec2 aVertexUVs_;
#endif

layout(location = 0) out vec4 outColor;

void main() {
    ivec2 icoord = ivec2(gl_FragCoord.xy);
    ivec2 icoord_low = ivec2(gl_FragCoord.xy) / 2;

    float d0 = LinearizeDepth(texelFetch(depth_texture, icoord, 0).r, shrd_data.uClipInfo);
 
    float d1 = abs(d0 - texelFetch(depth_low_texture, icoord_low + ivec2(0, 0), 0).r);
    float d2 = abs(d0 - texelFetch(depth_low_texture, icoord_low + ivec2(0, 1), 0).r);
    float d3 = abs(d0 - texelFetch(depth_low_texture, icoord_low + ivec2(1, 0), 0).r);
    float d4 = abs(d0 - texelFetch(depth_low_texture, icoord_low + ivec2(1, 1), 0).r);
 
    float dmin = min(min(d1, d2), min(d3, d4));
 
    if (dmin < 0.05) {
        outColor = texture(source_texture, aVertexUVs_ * shrd_data.uResAndFRes.xy / shrd_data.uResAndFRes.zw);
    } else {
        if (dmin == d1) {
            outColor = texelFetch(source_texture, icoord_low + ivec2(0, 0), 0);
        }else if (dmin == d2) {
            outColor = texelFetch(source_texture, icoord_low + ivec2(0, 1), 0);
        } else if (dmin == d3) {
            outColor = texelFetch(source_texture, icoord_low + ivec2(1, 0), 0);
        } else if (dmin == d4) {
            outColor = texelFetch(source_texture, icoord_low + ivec2(1, 1), 0);
        }
    }
}
