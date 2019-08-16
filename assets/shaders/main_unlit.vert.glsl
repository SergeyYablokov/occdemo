#version 310 es
#extension GL_EXT_texture_buffer : enable
#extension GL_OES_texture_buffer : enable
#if !defined(VULKAN) && !defined(GL_SPIRV)
#extension GL_ARB_bindless_texture: enable
#endif
//#extension GL_EXT_control_flow_attributes : enable

$ModifyWarning

#include "internal/_vs_common.glsl"

/*
UNIFORM_BLOCKS
    SharedDataBlock : $ubSharedDataLoc
*/

layout(location = REN_VTX_POS_LOC) in vec3 aVertexPosition;
layout(location = REN_VTX_UV1_LOC) in vec2 aVertexUVs1;

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
    ivec2 uInstanceIndices[REN_MAX_BATCH_SIZE];
};
#else
layout(location = REN_U_INSTANCES_LOC) uniform ivec2 uInstanceIndices[REN_MAX_BATCH_SIZE];
#endif

layout(binding = REN_INST_BUF_SLOT) uniform highp samplerBuffer instances_buffer;

layout(binding = REN_MATERIALS_SLOT) readonly buffer Materials {
	MaterialData materials[];
};

#if defined(GL_ARB_bindless_texture)
layout(binding = REN_BINDLESS_TEX_SLOT) readonly buffer TextureHandles {
	uvec2 texture_handles[];
};
#endif

#if defined(VULKAN) || defined(GL_SPIRV)
layout(location = 4) out vec2 aVertexUVs1_;
#if defined(GL_ARB_bindless_texture)
layout(location = 8) out flat uvec2 mat0_texture;
layout(location = 9) out flat uvec2 mat1_texture;
#endif // GL_ARB_bindless_texture
#else
out vec2 aVertexUVs1_;
#if defined(GL_ARB_bindless_texture)
out flat uvec2 mat0_texture;
out flat uvec2 mat1_texture;
#endif // GL_ARB_bindless_texture
#endif

invariant gl_Position;

void main(void) {
    ivec2 instance = uInstanceIndices[gl_InstanceIndex];

    mat4 model_matrix = FetchModelMatrix(instances_buffer, instance.x);

    aVertexUVs1_ = aVertexUVs1;
    
#if defined(GL_ARB_bindless_texture)
	MaterialData mat = materials[instance.y];
	mat0_texture = texture_handles[mat.texture_indices[0]];
	mat1_texture = texture_handles[mat.texture_indices[1]];
#endif // GL_ARB_bindless_texture
	
    vec3 vtx_pos_ws = (model_matrix * vec4(aVertexPosition, 1.0)).xyz;
    gl_Position = shrd_data.uViewProjMatrix * vec4(vtx_pos_ws, 1.0);
} 
