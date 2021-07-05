#pragma once

#include <cstdint>

#include "SmallVector.h"
#include "Texture.h"
#include "TextureParams.h"

#if defined(USE_VK_RENDER)
#include "VK.h"
#endif

namespace Ren {
class Framebuffer {
    ApiContext *api_ctx_ = nullptr;
#if defined(USE_VK_RENDER)
    VkFramebuffer handle_ = VK_NULL_HANDLE;
#elif defined(USE_GL_RENDER)
    uint32_t id_ = 0;
#endif
    SmallVector<TexHandle, 4> color_attachments_;
    TexHandle depth_attachment_, stencil_attachment_;

  public:
    Framebuffer() = default;
    ~Framebuffer();

    Framebuffer(const Framebuffer &rhs) = delete;
    Framebuffer(Framebuffer &&rhs) noexcept { (*this) = std::move(rhs); }
    Framebuffer &operator=(const Framebuffer &rhs) = delete;
    Framebuffer &operator=(Framebuffer &&rhs) noexcept;

#if defined(USE_VK_RENDER)
    VkFramebuffer handle() const { return handle_; }
#elif defined(USE_GL_RENDER)
    uint32_t id() const { return id_; }
#endif

    bool Setup(ApiContext *api_ctx, void *renderpass, int w, int h, const TexHandle color_attachments[],
               int color_attachments_count, TexHandle depth_attachment, TexHandle stencil_attachment,
               bool is_multisampled);
    bool Setup(ApiContext *api_ctx, void *renderpass, int w, int h, const TexHandle color_attachment,
               const TexHandle depth_attachment, const TexHandle stencil_attachment, const bool is_multisampled) {
        return Setup(api_ctx, renderpass, w, h, &color_attachment, 1, depth_attachment, stencil_attachment, is_multisampled);
    }
};
} // namespace Ren