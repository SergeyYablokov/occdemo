#version 310 es
#extension GL_EXT_texture_buffer : enable
#if !defined(VULKAN)
#extension GL_ARB_bindless_texture: enable
#endif

#include "_vs_common.glsl"

/*
PERM @TRANSPARENT_PERM
*/

layout(location = REN_VTX_POS_LOC) in vec3 aVertexPosition;
#ifdef TRANSPARENT_PERM
layout(location = REN_VTX_UV1_LOC) in vec2 aVertexUVs1;
#endif

layout(binding = REN_INST_BUF_SLOT) uniform highp samplerBuffer instances_buffer;

#if defined(VULKAN)
layout(push_constant) uniform PushConstants {
    ivec2 uInstanceIndices[REN_MAX_BATCH_SIZE];
	mat4 uShadowViewProjMatrix;
};
#else
layout(location = REN_U_INSTANCES_LOC) uniform ivec2 uInstanceIndices[REN_MAX_BATCH_SIZE];
layout(location = REN_U_M_MATRIX_LOC) uniform mat4 uShadowViewProjMatrix;
#endif

layout(binding = REN_MATERIALS_SLOT) readonly buffer Materials {
	MaterialData materials[];
};

#if defined(GL_ARB_bindless_texture)
layout(binding = REN_BINDLESS_TEX_SLOT) readonly buffer TextureHandles {
	uvec2 texture_handles[];
};
#endif

#if defined(VULKAN) || defined(GL_SPIRV)
	#ifdef TRANSPARENT_PERM
	layout(location = 0) out vec2 aVertexUVs1_;
	#if defined(GL_ARB_bindless_texture)
	layout(location = 1) out flat uvec2 alpha_texture;
	#endif // GL_ARB_bindless_texture
	#endif // TRANSPARENT_PERM
#else
	#ifdef TRANSPARENT_PERM
	out vec2 aVertexUVs1_;
	#if defined(GL_ARB_bindless_texture)
	out flat uvec2 alpha_texture;
	#endif // GL_ARB_bindless_texture
	#endif
#endif

void main() {
    ivec2 instance = uInstanceIndices[gl_InstanceIndex];
    mat4 MMatrix = FetchModelMatrix(instances_buffer, instance.x);

#ifdef TRANSPARENT_PERM
    aVertexUVs1_ = aVertexUVs1;
	
#if defined(GL_ARB_bindless_texture)
	MaterialData mat = materials[instance.y];
	alpha_texture = texture_handles[mat.texture_indices[0]];
#endif // GL_ARB_bindless_texture
#endif

    vec3 vertex_position_ws = (MMatrix * vec4(aVertexPosition, 1.0)).xyz;
    gl_Position = uShadowViewProjMatrix * vec4(vertex_position_ws, 1.0);
} 
