#include "Renderer.h"

#include "Utils.h"

#include <cassert>

#include <Ren/Context.h>
#include <Ren/GL.h>
#include <Ren/GLCtx.h>
#include <Sys/Json.h>

#include "shaders.inl"

Gui::Renderer::Renderer(Ren::Context &ctx, const JsObject &config) : ctx_(ctx) {
    const JsString &js_gl_defines = config.at(GL_DEFINES_KEY).as_str();

    { // Load main shader
        using namespace Ren;

        ShaderRef ui_vs_ref, ui_fs_ref;
        if (ctx.capabilities.spirv) {
            eShaderLoadStatus sh_status;
            ui_vs_ref =
                ctx_.LoadShaderSPIRV("__ui_vs__", ui_vert_spv_ogl, ui_vert_spv_ogl_size, eShaderType::Vert, &sh_status);
            assert(sh_status == eShaderLoadStatus::CreatedFromData || sh_status == eShaderLoadStatus::Found);
            ui_fs_ref =
                ctx_.LoadShaderSPIRV("__ui_fs__", ui_frag_spv_ogl, ui_frag_spv_ogl_size, eShaderType::Frag, &sh_status);
            assert(sh_status == eShaderLoadStatus::CreatedFromData || sh_status == eShaderLoadStatus::Found);
        } else {
            eShaderLoadStatus sh_status;
            ui_vs_ref = ctx_.LoadShaderGLSL("__ui_vs__", vs_source, eShaderType::Vert, &sh_status);
            assert(sh_status == eShaderLoadStatus::CreatedFromData || sh_status == eShaderLoadStatus::Found);
            ui_fs_ref = ctx_.LoadShaderGLSL("__ui_fs__", fs_source, eShaderType::Frag, &sh_status);
            assert(sh_status == eShaderLoadStatus::CreatedFromData || sh_status == eShaderLoadStatus::Found);
        }

        eProgLoadStatus status;
        ui_program_ = ctx_.LoadProgram("__ui_program__", ui_vs_ref, ui_fs_ref, {}, {}, &status);
        assert(status == eProgLoadStatus::CreatedFromData || status == eProgLoadStatus::Found);
    }

    const int instance_index = g_instance_count++;

    char name_buf[32];
    sprintf(name_buf, "UI_VertexBuffer [%i]", instance_index);

    vertex_buf_ =
        ctx_.CreateBuffer(name_buf, Ren::eBufType::VertexAttribs, Ren::eBufAccessType::Draw,
                          Ren::eBufAccessFreq::Dynamic, Ren::MaxFramesInFlight * MaxVerticesPerRange * sizeof(vertex_t));

    sprintf(name_buf, "UI_IndexBuffer [%i]", instance_index);

    index_buf_ =
        ctx_.CreateBuffer(name_buf, Ren::eBufType::VertexIndices, Ren::eBufAccessType::Draw,
                          Ren::eBufAccessFreq::Dynamic, Ren::MaxFramesInFlight * MaxIndicesPerRange * sizeof(uint16_t));

    if (ctx.capabilities.persistent_buf_mapping) {
        vtx_data_ = reinterpret_cast<vertex_t *>(vertex_buf_->Map(Ren::BufMapWrite, true /* persistent */));
        ndx_data_ = reinterpret_cast<uint16_t *>(index_buf_->Map(Ren::BufMapWrite, true /* persistent */));
    } else {
        stage_vtx_data_.reset(new vertex_t[MaxVerticesPerRange * Ren::MaxFramesInFlight]);
        stage_ndx_data_.reset(new uint16_t[MaxIndicesPerRange * Ren::MaxFramesInFlight]);
        vtx_data_ = stage_vtx_data_.get();
        ndx_data_ = stage_ndx_data_.get();
    }

    for (int i = 0; i < Ren::MaxFramesInFlight; i++) {
        vertex_count_[i] = 0;
        index_count_[i] = 0;
    }

    const Ren::VtxAttribDesc attribs[] = {{vertex_buf_->handle(), VTX_POS_LOC, 3, Ren::eType::Float32, sizeof(vertex_t),
                                           uintptr_t(offsetof(vertex_t, pos))},
                                          {vertex_buf_->handle(), VTX_COL_LOC, 4, Ren::eType::Uint8UNorm,
                                           sizeof(vertex_t), uintptr_t(offsetof(vertex_t, col))},
                                          {vertex_buf_->handle(), VTX_UVS_LOC, 4, Ren::eType::Uint16UNorm,
                                           sizeof(vertex_t), uintptr_t(offsetof(vertex_t, uvs))}};

    vao_.Setup(attribs, 3, index_buf_->handle());
}

Gui::Renderer::~Renderer() = default;

void Gui::Renderer::Draw(int w, int h) {
#ifndef NDEBUG
    if (buf_range_fences_[ctx_.backend_frame]) {
        const Ren::WaitResult res = buf_range_fences_[ctx_.backend_frame].ClientWaitSync(0);
        if (res != Ren::WaitResult::Success) {
            ctx_.log()->Error("[Gui::Renderer::Draw]: Buffers are still in use!");
        }
        buf_range_fences_[ctx_.backend_frame] = {};
    }
#endif

    glViewport(0, 0, w, h);

    //
    // Update buffers
    //
    const GLbitfield BufferRangeBindFlags = GLbitfield(GL_MAP_WRITE_BIT) | GLbitfield(GL_MAP_INVALIDATE_RANGE_BIT) |
                                            GLbitfield(GL_MAP_UNSYNCHRONIZED_BIT) |
                                            GLbitfield(GL_MAP_FLUSH_EXPLICIT_BIT);

    if (vertex_count_[ctx_.backend_frame]) {
        glBindBuffer(GL_ARRAY_BUFFER, vertex_buf_->id());

        const size_t vertex_buf_mem_size = vertex_count_[ctx_.backend_frame] * sizeof(vertex_t);
        if (ctx_.capabilities.persistent_buf_mapping) {
            glFlushMappedBufferRange(GL_ARRAY_BUFFER,
                                     GLintptr(ctx_.backend_frame) * MaxVerticesPerRange * sizeof(vertex_t),
                                     vertex_buf_mem_size);
        } else {
            void *pinned_mem =
                glMapBufferRange(GL_ARRAY_BUFFER, GLintptr(ctx_.backend_frame) * MaxVerticesPerRange * sizeof(vertex_t),
                                 MaxVerticesPerRange * sizeof(vertex_t), BufferRangeBindFlags);
            if (pinned_mem) {
                memcpy(pinned_mem, vtx_data_ + size_t(ctx_.backend_frame) * MaxVerticesPerRange, vertex_buf_mem_size);
                glFlushMappedBufferRange(GL_ARRAY_BUFFER, 0, vertex_buf_mem_size);
                glUnmapBuffer(GL_ARRAY_BUFFER);
            } else {
                ctx_.log()->Error("[Gui::Renderer::SwapBuffers]: Failed to map vertex buffer!");
            }
        }
    }

    if (index_count_[ctx_.backend_frame]) {
        glBindBuffer(GL_ARRAY_BUFFER, index_buf_->id());

        const size_t index_buf_mem_size = index_count_[ctx_.backend_frame] * sizeof(uint16_t);
        if (ctx_.capabilities.persistent_buf_mapping) {
            glFlushMappedBufferRange(GL_ARRAY_BUFFER,
                                     GLintptr(ctx_.backend_frame) * MaxIndicesPerRange * sizeof(uint16_t),
                                     index_buf_mem_size);
        } else {
            void *pinned_mem =
                glMapBufferRange(GL_ARRAY_BUFFER, GLintptr(ctx_.backend_frame) * MaxIndicesPerRange * sizeof(uint16_t),
                                 MaxIndicesPerRange * sizeof(uint16_t), BufferRangeBindFlags);
            if (pinned_mem) {
                memcpy(pinned_mem, ndx_data_ + size_t(ctx_.backend_frame) * MaxIndicesPerRange, index_buf_mem_size);
                glFlushMappedBufferRange(GL_ARRAY_BUFFER, 0, index_buf_mem_size);
                glUnmapBuffer(GL_ARRAY_BUFFER);
            } else {
                ctx_.log()->Error("[Gui::Renderer::SwapBuffers]: Failed to map index buffer!");
            }
        }
    }

    //
    // Submit draw call
    //
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
#if !defined(__ANDROID__)
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
#endif

    glBindVertexArray(vao_.id());
    glUseProgram(ui_program_->id());

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D_ARRAY, GLuint(ctx_.texture_atlas().tex_id()));

    const size_t index_buf_mem_offset = size_t(ctx_.backend_frame) * MaxIndicesPerRange * sizeof(uint16_t);

    glDrawElementsBaseVertex(GL_TRIANGLES, index_count_[ctx_.backend_frame], GL_UNSIGNED_SHORT,
                             reinterpret_cast<const GLvoid *>(uintptr_t(index_buf_mem_offset)),
                             (ctx_.backend_frame * MaxVerticesPerRange));

    glBindVertexArray(0);
    glUseProgram(0);

    vertex_count_[ctx_.backend_frame] = 0;
    index_count_[ctx_.backend_frame] = 0;

#ifndef NDEBUG
    assert(!buf_range_fences_[ctx_.backend_frame]);
    buf_range_fences_[ctx_.backend_frame] = Ren::MakeFence();
#endif
}

#undef VTX_POS_LOC
#undef VTX_COL_LOC
#undef VTX_UVS_LOC

#undef TEX_ATLAS_SLOT
