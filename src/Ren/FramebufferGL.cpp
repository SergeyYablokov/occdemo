#include "Framebuffer.h"

#include "GL.h"

Ren::Framebuffer::~Framebuffer() {
    if (id_) {
        auto fb = GLuint(id_);
        glDeleteFramebuffers(1, &fb);
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

    if (color_attachments_count == 1 && color_attachments[0].id == 0) {
        // default backbuffer
        return true;
    }

    if (!id_) {
        GLuint fb;
        glGenFramebuffers(1, &fb);
        id_ = uint32_t(fb);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, GLuint(id_));

    const GLenum target = is_multisampled ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D;

    for (size_t i = 0; i < color_attachments_.size(); i++) {
        if (color_attachments_[i].id) {
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + GLenum(i), target, 0, 0);
            color_attachments_[i] = {};
        }
    }
    color_attachments_.clear();

    SmallVector<GLenum, 4> draw_buffers;
    for (int i = 0; i < color_attachments_count; i++) {
        if (color_attachments[i].id) {
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, target, GLuint(color_attachments[i].id),
                                   0);

            draw_buffers.push_back(GL_COLOR_ATTACHMENT0 + i);
        } else {
            draw_buffers.push_back(GL_NONE);
        }
        color_attachments_.push_back(color_attachments[i]);
    }

    glDrawBuffers(GLsizei(color_attachments_.size()), draw_buffers.data());

    if (depth_attachment.id) {
        if (depth_attachment == stencil_attachment) {
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, target, GLuint(depth_attachment.id), 0);
        } else {
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, target, GLuint(depth_attachment.id), 0);
        }
        depth_attachment_ = depth_attachment;
    } else {
        depth_attachment_ = {};
    }

    if (stencil_attachment.id) {
        if (depth_attachment != stencil_attachment) {
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, target, GLuint(stencil_attachment.id), 0);
        }
        stencil_attachment_ = stencil_attachment;
    } else {
        stencil_attachment_ = {};
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    const GLenum s = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    return (s == GL_FRAMEBUFFER_COMPLETE);
}