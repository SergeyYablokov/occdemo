
#include "../../../src/Eng/Renderer/Renderer_GL_Defines.inl"

struct ShadowMapRegion {
    vec4 transform;
    mat4 clip_from_world;
};

struct ProbeItem {
    vec4 pos_and_radius;
    vec4 unused_and_layer;
    vec4 sh_coeffs[3];
};

struct EllipsItem {
    vec4 pos_and_radius;
    vec4 axis_and_perp;
};

struct SharedData {
    mat4 uViewMatrix, uProjMatrix, uViewProjMatrix, uViewProjPrevMatrix;
    mat4 uInvViewMatrix, uInvProjMatrix, uInvViewProjMatrix, uDeltaMatrix;
    ShadowMapRegion uShadowMapRegions[REN_MAX_SHADOWMAPS_TOTAL];
    vec4 uSunDir, uSunCol, uTaaInfo;
    vec4 uClipInfo, uCamPosAndGamma;
    vec4 uResAndFRes, uTranspParamsAndTime;
    vec4 uWindScroll, uWindScrollPrev;
    ProbeItem uProbes[REN_MAX_PROBES_TOTAL];
    EllipsItem uEllipsoids[REN_MAX_ELLIPSES_TOTAL];
};

struct MaterialData {
    uint texture_indices[5];
    uint _pad[3];
    vec4 params;
};

#if defined(GL_ARB_bindless_texture)
#define SAMPLER2D(x) sampler2D(x)
#else // GL_ARB_bindless_texture
#define SAMPLER2D
#endif // GL_ARB_bindless_texture

