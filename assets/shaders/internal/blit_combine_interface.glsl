
#include "_interface_common.glsl"

INTERFACE_START(BlitCombine)

struct Params {
	VEC4_TYPE transform;
	VEC2_TYPE tex_size;
	float tonemap;
	float gamma;
	float exposure;
	float fade;
};

#define HDR_TEX_SLOT 0
#define BLURED_TEX_SLOT 1

INTERFACE_END