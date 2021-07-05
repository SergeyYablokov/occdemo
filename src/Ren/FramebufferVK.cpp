#include "Framebuffer.h"

#include "VKCtx.h"

Ren::Framebuffer& Ren::Framebuffer::operator=(Framebuffer&& rhs) noexcept {
    if (&rhs == this) {
        return (*this);
    }

    if (handle_ != VK_NULL_HANDLE) {
        vkDestroyFramebuffer(api_ctx_->device, handle_, nullptr);
    }

    api_ctx_ = exchange(rhs.api_ctx_, nullptr);
    handle_ = exchange(rhs.handle_, VkFramebuffer{VK_NULL_HANDLE});
    color_attachments_ = std::move(rhs.color_attachments_);
    depth_attachment_ = exchange(rhs.depth_attachment_, {});
    stencil_attachment_ = exchange(rhs.stencil_attachment_, {});

    return (*this);
}

Ren::Framebuffer::~Framebuffer() {
    if (handle_ != VK_NULL_HANDLE) {
        vkDestroyFramebuffer(api_ctx_->device, handle_, nullptr);
    }
}

bool Ren::Framebuffer::Setup(ApiContext *api_ctx, void *renderpass, int w, int h, const TexHandle color_attachments[],
                             const int color_attachments_count, const TexHandle depth_attachment,
                             const TexHandle stencil_attachment, const bool is_multisampled) {
    if (color_attachments_count == color_attachments_.size() &&
        std::equal(color_attachments, color_attachments + color_attachments_count, color_attachments_.data()) &&
        depth_attachment == depth_attachment_ && stencil_attachment == stencil_attachment_) {
        // nothing has changed
        return true;
    }

    if (color_attachments_count == 1 && color_attachments[0].view == VK_NULL_HANDLE) {
        // default backbuffer
        return true;
    }

    if (handle_ != VK_NULL_HANDLE) {
        vkDestroyFramebuffer(api_ctx_->device, handle_, nullptr);
    }

    api_ctx_ = api_ctx;

    SmallVector<VkImageView, 4> image_views;
    for (int i = 0; i < color_attachments_count; i++) {
        image_views.push_back(color_attachments[i].view);
    }

    VkFramebufferCreateInfo framebuf_create_info = {};
    framebuf_create_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebuf_create_info.renderPass = reinterpret_cast<VkRenderPass>(renderpass);
    framebuf_create_info.attachmentCount = uint32_t(image_views.size());
    framebuf_create_info.pAttachments = image_views.data();
    framebuf_create_info.width = w;
    framebuf_create_info.height = h;
    framebuf_create_info.layers = 1;

    const VkResult res = vkCreateFramebuffer(api_ctx->device, &framebuf_create_info, nullptr, &handle_);
    return res == VK_SUCCESS;
}