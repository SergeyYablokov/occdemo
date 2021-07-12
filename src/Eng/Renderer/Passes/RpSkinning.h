#pragma once

#include "../Graph/GraphBuilder.h"
#include "../Renderer_DrawList.h"

struct DrawList;

class RpSkinning : public RenderPassBase {
    // lazily initialized data
    bool initialized = false;
    Ren::ProgramRef skinning_prog_;

    // temp data (valid only between Setup and Execute calls)
    DynArrayConstRef<SkinRegion> skin_regions_;

    Ren::BufferRef vtx_buf1_, vtx_buf2_, delta_buf_, skin_vtx_buf_;

    RpResource skin_transforms_buf_;
    RpResource shape_keys_buf_;

#if defined(USE_VK_RENDER)
    VkDescriptorSetLayout desc_set_layout_ = VK_NULL_HANDLE;
    VkDescriptorPool desc_pool_ = VK_NULL_HANDLE;
    VkDescriptorSet desc_set_[Ren::MaxFramesInFlight] = {};
    VkPipelineLayout pipeline_layout_ = VK_NULL_HANDLE;
    VkPipeline pipeline_ = VK_NULL_HANDLE;
#endif

    void LazyInit(Ren::Context &ctx, ShaderLoader &sh);

    bool InitPipeline(Ren::Context &ctx);
  public:
    ~RpSkinning();

    void Setup(RpBuilder &builder, const DrawList &list, Ren::BufferRef vtx_buf1,
               Ren::BufferRef vtx_buf2, Ren::BufferRef delta_buf, Ren::BufferRef skin_vtx_buf,
               const char skin_transforms_buf[], const char shape_keys_buf[]);
    void Execute(RpBuilder &builder) override;

    const char *name() const override { return "SKINNING"; }
};