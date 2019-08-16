#version 310 es
#if !defined(VULKAN) && !defined(GL_SPIRV)
#extension GL_ARB_bindless_texture: enable
#endif

#if defined(GL_ES) || defined(VULKAN)
	precision highp int;
	precision mediump float;
#endif

#include "_fs_common.glsl"

/*
PERM @TRANSPARENT_PERM
*/

#ifdef TRANSPARENT_PERM
#if !defined(GL_ARB_bindless_texture)
layout(binding = REN_MAT_TEX0_SLOT) uniform sampler2D alpha_texture;
#endif
#endif

#if defined(VULKAN) || defined(GL_SPIRV)
	#ifdef TRANSPARENT_PERM
	layout(location = 0) in vec2 aVertexUVs1_;
	#if defined(GL_ARB_bindless_texture)
	layout(location = 1) in flat uvec2 alpha_texture;
	#endif // GL_ARB_bindless_texture
	#endif // TRANSPARENT_PERM
#else
	#ifdef TRANSPARENT_PERM
	in vec2 aVertexUVs1_;
	#if defined(GL_ARB_bindless_texture)
	in flat uvec2 alpha_texture;
	#endif // GL_ARB_bindless_texture
	#endif // TRANSPARENT_PERM
#endif

void main() {
#ifdef TRANSPARENT_PERM
    float alpha = texture(SAMPLER2D(alpha_texture), aVertexUVs1_).a;
    if (alpha < 0.5) discard;
#endif
}
