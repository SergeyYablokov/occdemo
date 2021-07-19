#include "Framebuffer.h"

#include "VKCtx.h"

Ren::Framebuffer &Ren::Framebuffer::operator=(Framebuffer &&rhs) noexcept {
    if (&rhs == this) {
        return (*this);
    }

    if (handle_ != VK_NULL_HANDLE) {
        vkDestroyFramebuffer(api_ctx_->device, handle_, nullptr);
    }

    api_ctx_ = exchange(rhs.api_ctx_, nullptr);
    handle_ = exchange(rhs.handle_, VkFramebuffer{VK_NULL_HANDLE});
    renderpass_ = exchange(rhs.renderpass_, VkRenderPass{VK_NULL_HANDLE});
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

bool Ren::Framebuffer::Setup(ApiContext *api_ctx, void *renderpass, int w, int h,
                             const WeakTex2DRef color_attachments[], const int color_attachments_count,
                             const WeakTex2DRef depth_attachment, const WeakTex2DRef stencil_attachment,
                             const bool is_multisampled) {
    if (renderpass_ == renderpass && color_attachments_count == color_attachments_.size() &&
        std::equal(color_attachments, color_attachments + color_attachments_count, color_attachments_.data(),
                   [](const WeakTex2DRef &lhs, const Attachment &rhs) {
                       return (!lhs && !rhs.ref) || (lhs && lhs->handle() == rhs.handle);
                   }) &&
        ((!depth_attachment && !depth_attachment_.ref) ||
         (depth_attachment && depth_attachment->handle() == depth_attachment_.handle)) &&
        ((!stencil_attachment && !stencil_attachment_.ref) ||
         (stencil_attachment && stencil_attachment->handle() == stencil_attachment_.handle))) {
        // nothing has changed
        return true;
    }

    if (color_attachments_count == 1 && !color_attachments[0]) {
        // default backbuffer
        return true;
    }

    if (handle_ != VK_NULL_HANDLE) {
        vkDestroyFramebuffer(api_ctx_->device, handle_, nullptr);
    }

    api_ctx_ = api_ctx;
    color_attachments_.clear();
    depth_attachment_ = {};
    stencil_attachment_ = {};

    SmallVector<VkImageView, 4> image_views;
    for (int i = 0; i < color_attachments_count; i++) {
        if (color_attachments[i]) {
            image_views.push_back(color_attachments[i]->handle().view);
            color_attachments_.push_back({color_attachments[i], color_attachments[i]->handle()});
        } else {
            color_attachments_.emplace_back();
        }
    }

    if (depth_attachment) {
        image_views.push_back(depth_attachment->handle().view);
        depth_attachment_ = {depth_attachment, depth_attachment->handle()};
    }

    if (stencil_attachment) {
        stencil_attachment_ = {stencil_attachment, stencil_attachment->handle()};
        if (stencil_attachment->handle().view != depth_attachment->handle().view) {
            image_views.push_back(stencil_attachment->handle().view);
        }
    }

    renderpass_ = reinterpret_cast<VkRenderPass>(renderpass);

    VkFramebufferCreateInfo framebuf_create_info = {};
    framebuf_create_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebuf_create_info.renderPass = renderpass_;
    framebuf_create_info.attachmentCount = uint32_t(image_views.size());
    framebuf_create_info.pAttachments = image_views.data();
    framebuf_create_info.width = w;
    framebuf_create_info.height = h;
    framebuf_create_info.layers = 1;

    const VkResult res = vkCreateFramebuffer(api_ctx->device, &framebuf_create_info, nullptr, &handle_);
    return res == VK_SUCCESS;
}