#pragma once

#include "Fwd.h"
#include "VK.h"

namespace Ren {
struct ApiContext;

const int MaxRTAttachments = 4;

enum class eLoadOp : uint8_t { Load, Clear, DontCare, _Count };
enum class eStoreOp : uint8_t { Store, DontCare, _Count };

struct RenderTarget {
    Tex2DRef ref;
    eLoadOp load = eLoadOp::DontCare;
    eStoreOp store = eStoreOp::DontCare;
    eLoadOp stencil_load = eLoadOp::DontCare;
    eStoreOp stencil_store = eStoreOp::DontCare;
};

class RenderPass {
    ApiContext *api_ctx_ = nullptr;
#if defined(USE_VK_RENDER)
    VkRenderPass render_pass_ = VK_NULL_HANDLE;
#endif
    SmallVector<TexHandle, MaxRTAttachments> color_attachments_;
    TexHandle depth_attachment_;

    void Destroy();

  public:
    RenderPass() = default;
    ~RenderPass() { Destroy(); }

#if defined(USE_VK_RENDER)
    VkRenderPass handle() const { return render_pass_; }
#else
    void *handle() const { return nullptr; }
#endif

    bool Setup(ApiContext *api_ctx, const RenderTarget rts[], int color_rts_count, RenderTarget depth_rt, ILog *log);
};
} // namespace Ren