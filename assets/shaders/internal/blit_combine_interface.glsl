
#include "_interface_common.glsl"

INTERFACE_START(BlitCombine)

struct UniformParams {
	VEC4_TYPE uTransform;
	VEC2_TYPE uTexSize;
	float tonemap;
	float gamma;
	float exposure;
	float fade;
};

INTERFACE_END