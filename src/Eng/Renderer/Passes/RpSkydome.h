#pragma once

#include "../Graph/GraphBuilder.h"
#include "../Renderer_DrawList.h"

#include <Ren/RastState.h>
#if defined(USE_GL_RENDER)
#include <Ren/VaoGL.h>
#endif

class RpSkydome : public RenderPassBase {
    bool initialized = false;
    Ren::ProgramRef skydome_prog_;

    // temp data (valid only between Setup and Execute calls)
    const ViewState *view_state_ = nullptr;
    const EnvironmentWeak *env_ = nullptr;

    Ren::Vec3f draw_cam_pos_;

    // lazily initialized data
    Ren::MeshRef skydome_mesh_;
#if defined(USE_VK_RENDER)
    Ren::ApiContext *api_ctx_ = nullptr;
    VkDescriptorSetLayout desc_set_layout_ = VK_NULL_HANDLE;
    VkDescriptorPool desc_pool_ = VK_NULL_HANDLE;
    VkDescriptorSet desc_set_[Ren::MaxFramesInFlight] = {};
    VkPipelineLayout pipeline_layout_ = VK_NULL_HANDLE;
    VkPipeline pipeline_ = VK_NULL_HANDLE;
#elif defined(USE_GL_RENDER)
    Ren::Vao skydome_vao_;
    Ren::Framebuffer cached_fb_;
#endif

    RpResource shared_data_buf_;

    RpResource color_tex_;
    RpResource spec_tex_;
    RpResource depth_tex_;

    void LazyInit(Ren::Context &ctx, ShaderLoader &sh, RpAllocTex &color_tex, RpAllocTex &spec_tex,
                  RpAllocTex &depth_tex);
    void DrawSkydome(RpBuilder &builder);

    bool InitPipeline(Ren::Context &ctx);
  public:
    ~RpSkydome();

    void Setup(RpBuilder &builder, const DrawList &list, const ViewState *view_state,
               const char shared_data_buf[], const char color_tex[], const char spec_tex[], const char depth_tex[]);
    void Execute(RpBuilder &builder) override;

    const char *name() const override { return "SKYDOME"; }
};