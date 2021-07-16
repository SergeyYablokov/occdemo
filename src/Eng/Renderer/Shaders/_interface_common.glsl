R"(
#ifndef INTERFACE_COMMON_GLSL
#define INTERFACE_COMMON_GLSL

#ifdef __cplusplus
#define VEC2_TYPE Ren::Vec2f
#define VEC3_TYPE Ren::Vec3f
#define VEC4_TYPE Ren::Vec4f

#define INTERFACE_START(name) namespace #name {
#define INTERFACE_END }
#else // __cplusplus
#define VEC2_TYPE vec2
#define VEC3_TYPE vec3
#define VEC4_TYPE vec4

#define INTERFACE_START(name)
#define INTERFACE_END
#endif // __cplusplus

#endif // INTERFACE_COMMON_GLSL
)"