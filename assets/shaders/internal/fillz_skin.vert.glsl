#version 310 es
#extension GL_EXT_texture_buffer : enable
#if !defined(VULKAN) && !defined(GL_SPIRV)
#extension GL_ARB_bindless_texture: enable
#endif

#include "_vs_common.glsl"

/*
UNIFORM_BLOCKS
    SharedDataBlock : $ubSharedDataLoc
PERM @TRANSPARENT_PERM
PERM @OUTPUT_VELOCITY
PERM @OUTPUT_VELOCITY;MOVING_PERM
PERM @TRANSPARENT_PERM;OUTPUT_VELOCITY
PERM @TRANSPARENT_PERM;OUTPUT_VELOCITY;MOVING_PERM
*/

layout(location = REN_VTX_POS_LOC) in vec3 aVertexPositionCurr;
#ifdef TRANSPARENT_PERM
layout(location = REN_VTX_UV1_LOC) in vec2 aVertexUVs1;
#endif
#ifdef OUTPUT_VELOCITY
layout(location = REN_VTX_PRE_LOC) in vec3 aVertexPositionPrev;
#endif

#if defined(VULKAN) || defined(GL_SPIRV)
layout (binding = REN_UB_SHARED_DATA_LOC, std140)
#else
layout (std140)
#endif
uniform SharedDataBlock {
    SharedData shrd_data;
};

layout(binding = REN_INST_BUF_SLOT) uniform mediump samplerBuffer instances_buffer;
layout(binding = REN_NOISE_TEX_SLOT) uniform sampler2D noise_texture;

#if defined(VULKAN)
layout(push_constant) uniform PushConstants {
    ivec2 uInstanceIndices[REN_MAX_BATCH_SIZE];
};
#else
layout(location = REN_U_INSTANCES_LOC) uniform ivec2 uInstanceIndices[REN_MAX_BATCH_SIZE];
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
    #ifdef HASHED_TRANSPARENCY
    layout(location = 1) out vec3 aVertexObjCoord_;
    #endif
	#if defined(GL_ARB_bindless_texture)
	layout(location = 4) out flat uvec2 alpha_texture;
	#endif // GL_ARB_bindless_texture
    #endif
    #ifdef OUTPUT_VELOCITY
    layout(location = 2) out vec3 aVertexCSCurr_;
    layout(location = 3) out vec3 aVertexCSPrev_;
    #endif
#else
    #ifdef TRANSPARENT_PERM
    out vec2 aVertexUVs1_;
    #ifdef HASHED_TRANSPARENCY
    out vec3 aVertexObjCoord_;
    #endif
	out flat uvec2 alpha_texture;
    #endif
    #ifdef OUTPUT_VELOCITY
    out vec3 aVertexCSCurr_;
    out vec3 aVertexCSPrev_;
    #endif
#endif

invariant gl_Position;

void main() {
    ivec2 instance = uInstanceIndices[gl_InstanceIndex];
    mat4 model_matrix_curr = FetchModelMatrix(instances_buffer, instance.x);

#ifdef MOVING_PERM
    mat4 model_matrix_prev = FetchModelMatrix(instances_buffer, instance.x + 1);
#endif

#ifdef TRANSPARENT_PERM
    aVertexUVs1_ = aVertexUVs1;
	
#if defined(GL_ARB_bindless_texture)
	MaterialData mat = materials[instance.y];
	alpha_texture = texture_handles[mat.texture_indices[0]];
#endif // GL_ARB_bindless_texture
#ifdef HASHED_TRANSPARENCY
    aVertexObjCoord_ = aVertexPositionCurr;
#endif
#endif

    vec3 vtx_pos_ws_curr = (model_matrix_curr * vec4(aVertexPositionCurr, 1.0)).xyz;
    gl_Position = shrd_data.uViewProjMatrix * vec4(vtx_pos_ws_curr, 1.0);

#ifdef OUTPUT_VELOCITY
    aVertexCSCurr_ = gl_Position.xyw;
#ifdef MOVING_PERM
    vec3 vtx_pos_ws_prev = (model_matrix_prev * vec4(aVertexPositionPrev, 1.0)).xyz;
#else
    vec3 vtx_pos_ws_prev = (model_matrix_curr * vec4(aVertexPositionPrev, 1.0)).xyz;
#endif
    aVertexCSPrev_ = (shrd_data.uViewProjPrevMatrix * vec4(vtx_pos_ws_prev, 1.0)).xyw;
#endif
} 

