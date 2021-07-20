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
    color_attachments = std::move(rhs.color_attachments);
    depth_attachment = exchange(rhs.depth_attachment, {});
    stencil_attachment = exchange(rhs.stencil_attachment, {});

    return (*this);
}

Ren::Framebuffer::~Framebuffer() {
    if (handle_ != VK_NULL_HANDLE) {
        vkDestroyFramebuffer(api_ctx_->device, handle_, nullptr);
    }
}

bool Ren::Framebuffer::Setup(ApiContext *api_ctx, void *renderpass, int w, int h,
                             const WeakTex2DRef _color_attachments[], const int _color_attachments_count,
                             const WeakTex2DRef _depth_attachment, const WeakTex2DRef _stencil_attachment,
                             const bool is_multisampled) {
    if (renderpass_ == renderpass && _color_attachments_count == color_attachments.size() &&
        std::equal(_color_attachments, _color_attachments + _color_attachments_count, color_attachments.data(),
                   [](const WeakTex2DRef &lhs, const Attachment &rhs) {
                       return (!lhs && !rhs.ref) || (lhs && lhs->handle() == rhs.handle);
                   }) &&
        ((!_depth_attachment && !depth_attachment.ref) ||
         (_depth_attachment && _depth_attachment->handle() == depth_attachment.handle)) &&
        ((!_stencil_attachment && !stencil_attachment.ref) ||
         (_stencil_attachment && _stencil_attachment->handle() == stencil_attachment.handle))) {
        // nothing has changed
        return true;
    }

    /*if (_color_attachments_count == 1 && !_color_attachments[0]) {
        // default backbuffer
        return true;
    }*/

    if (handle_ != VK_NULL_HANDLE) {
        vkDestroyFramebuffer(api_ctx_->device, handle_, nullptr);
    }

    api_ctx_ = api_ctx;
    color_attachments.clear();
    depth_attachment = {};
    stencil_attachment = {};

    SmallVector<VkImageView, 4> image_views;
    for (int i = 0; i < _color_attachments_count; i++) {
        if (_color_attachments[i]) {
            image_views.push_back(_color_attachments[i]->handle().view);
            color_attachments.push_back({_color_attachments[i], _color_attachments[i]->handle()});
        } else {
            color_attachments.emplace_back();
        }
    }

    if (_depth_attachment) {
        image_views.push_back(_depth_attachment->handle().view);
        depth_attachment = {_depth_attachment, _depth_attachment->handle()};
    }

    if (_stencil_attachment) {
        stencil_attachment = {_stencil_attachment, _stencil_attachment->handle()};
        if (_stencil_attachment->handle().view != _depth_attachment->handle().view) {
            image_views.push_back(_stencil_attachment->handle().view);
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