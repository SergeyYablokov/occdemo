#version 310 es

#if defined(GL_ES) || defined(VULKAN)
	precision highp int;
	precision highp float;
#endif

#include "_fs_common.glsl"

layout(binding = REN_BASE0_TEX_SLOT) uniform sampler2D s_texture;

#if defined(VULKAN)
layout(push_constant) uniform PushConstants {
    layout(offset = 16) vec3 color;
						float near;
						float far;
};
#else
layout(location = 3) uniform vec3 color;
layout(location = 1) uniform float near;
layout(location = 2) uniform float far;
#endif

#if defined(VULKAN) || defined(GL_SPIRV)
layout(location = 0) in vec2 aVertexUVs_;
#else
in vec2 aVertexUVs_;
#endif

layout(location = 0) out vec4 outColor;

void main() {
    float depth = texelFetch(s_texture, ivec2(aVertexUVs_), 0).r;
    if (near > 0.0001) {
        // cam is not orthographic
        depth = (near * far) / (depth * (near - far) + far);
        depth /= far;
    }
    outColor = vec4(vec3(depth) * color, 1.0);
}
