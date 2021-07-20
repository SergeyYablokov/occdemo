#include "Framebuffer.h"

#include "GL.h"

Ren::Framebuffer::~Framebuffer() {
    if (id_) {
        auto fb = GLuint(id_);
        glDeleteFramebuffers(1, &fb);
    }
}

bool Ren::Framebuffer::Setup(ApiContext *api_ctx, void *renderpass, int w, int h,
                             const WeakTex2DRef _color_attachments[], const int _color_attachments_count,
                             const WeakTex2DRef _depth_attachment, const WeakTex2DRef _stencil_attachment,
                             const bool is_multisampled) {
    if (_color_attachments_count == color_attachments.size() &&
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

    if (_color_attachments_count == 1 && !_color_attachments[0]) {
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

    for (size_t i = 0; i < color_attachments.size(); i++) {
        if (color_attachments[i].ref) {
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + GLenum(i), target, 0, 0);
            color_attachments[i] = {};
        }
    }
    color_attachments.clear();

    SmallVector<GLenum, 4> draw_buffers;
    for (int i = 0; i < _color_attachments_count; i++) {
        if (_color_attachments[i]) {
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, target,
                                   GLuint(_color_attachments[i]->id()), 0);

            draw_buffers.push_back(GL_COLOR_ATTACHMENT0 + i);

            color_attachments.push_back({_color_attachments[i], _color_attachments[i]->handle()});
        } else {
            draw_buffers.push_back(GL_NONE);

            color_attachments.emplace_back();
        }
    }

    glDrawBuffers(GLsizei(color_attachments.size()), draw_buffers.data());

    if (_depth_attachment) {
        if (_depth_attachment == _stencil_attachment) {
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, target, GLuint(_depth_attachment->id()),
                                   0);
        } else {
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, target, GLuint(_depth_attachment->id()), 0);
        }
        depth_attachment = {_depth_attachment, _depth_attachment->handle()};
    } else {
        depth_attachment = {};
    }

    if (_stencil_attachment) {
        if (!_depth_attachment || _depth_attachment->handle() != _stencil_attachment->handle()) {
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, target, GLuint(_stencil_attachment->id()), 0);
        }
        stencil_attachment = {_stencil_attachment, _stencil_attachment->handle()};
    } else {
        stencil_attachment = {};
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    const GLenum s = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    return (s == GL_FRAMEBUFFER_COMPLETE);
}