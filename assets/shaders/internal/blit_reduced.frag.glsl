#version 310 es

#if defined(GL_ES) || defined(VULKAN)
	precision highp int;
	precision highp float;
#endif

#include "_fs_common.glsl"

layout(binding = REN_BASE0_TEX_SLOT) uniform sampler2D s_texture;

#if defined(VULKAN)
layout(push_constant) uniform PushConstants {
    layout(offset = 16) vec2 uOffset;
};
#else
layout(location = 4) uniform vec2 uOffset;
#endif

#if defined(VULKAN) || defined(GL_SPIRV)
layout(location = 0) in vec2 aVertexUVs_;
#else
in vec2 aVertexUVs_;
#endif

layout(location = 0) out vec4 outColor;

void main() {
    vec3 c0 = texture(s_texture, aVertexUVs_ + uOffset).xyz;
    outColor.r = dot(c0, vec3(0.299, 0.587, 0.114));
}
