
#ifndef INTERFACE_COMMON_GLSL
#define INTERFACE_COMMON_GLSL

#ifdef __cplusplus
#define VEC2_TYPE Ren::Vec2f
#define VEC3_TYPE Ren::Vec3f
#define VEC4_TYPE Ren::Vec4f

#define INTERFACE_START(name) namespace name {
#define INTERFACE_END }
#else // __cplusplus
#define VEC2_TYPE vec2
#define VEC3_TYPE vec3
#define VEC4_TYPE vec4

#define INTERFACE_START(name)
#define INTERFACE_END

#if defined(VULKAN)
#define LAYOUT_PARAMS layout(push_constant)
#elif defined(GL_SPIRV)
#define LAYOUT_PARAMS layout(binding = REN_UB_UNIF_PARAM_LOC, std140)
#else
#define LAYOUT_PARAMS layout(std140)
#endif
#endif // __cplusplus

#endif // INTERFACE_COMMON_GLSL
