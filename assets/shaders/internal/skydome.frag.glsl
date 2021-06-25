#version 310 es

#if defined(GL_ES) || defined(VULKAN)
	precision highp int;
	precision mediump float;
#endif

#include "_fs_common.glsl"

#if defined(VULKAN) || defined(GL_SPIRV)
layout (binding = REN_UB_SHARED_DATA_LOC, std140)
#else
layout (std140)
#endif
uniform SharedDataBlock {
    SharedData shrd_data;
};

layout(binding = REN_BASE0_TEX_SLOT) uniform samplerCube env_texture;

#if defined(VULKAN) || defined(GL_SPIRV)
layout(location = 0) in vec3 aVertexPos_;
#else
in vec3 aVertexPos_;
#endif

layout(location = REN_OUT_COLOR_INDEX) out vec4 outColor;
layout(location = REN_OUT_SPEC_INDEX) out vec4 outSpecular;

void main() {
    vec3 view_dir_ws = normalize(aVertexPos_ - shrd_data.uCamPosAndGamma.xyz);

    outColor.rgb = clamp(RGBMDecode(texture(env_texture, view_dir_ws)), vec3(0.0), vec3(16.0));
    outColor.a = 1.0;
    outSpecular = vec4(0.0, 0.0, 0.0, 0.0);
}

