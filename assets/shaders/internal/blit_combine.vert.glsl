#version 310 es

#include "_vs_common.glsl"
#include "blit_combine_interface.glsl"

/*
UNIFORM_BLOCKS
    UniformParams : $ubUnifParamLoc
*/

layout(location = REN_VTX_POS_LOC) in vec2 aVertexPosition;
layout(location = REN_VTX_UV1_LOC) in vec2 aVertexUVs;

LAYOUT_PARAMS uniform UniformParams {
    Params params;
};

#if defined(VULKAN) || defined(GL_SPIRV)
layout(location = 0)
#endif
out vec2 aVertexUVs_;


void main() {
    aVertexUVs_ = params.transform.xy + aVertexUVs * params.transform.zw;
    gl_Position = vec4(aVertexPosition, 0.5, 1.0);
} 
