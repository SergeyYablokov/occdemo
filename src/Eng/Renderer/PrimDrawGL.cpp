#include "PrimDraw.h"

#include <Ren/Context.h>
#include <Ren/GL.h>

#include "Renderer_GL_Defines.inl"

namespace PrimDrawInternal {
extern const float fs_quad_positions[] = {-1.0f, -1.0f, 1.0f, -1.0f, 1.0f, 1.0f, -1.0f, 1.0f};
extern const float fs_quad_norm_uvs[] = {0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f};
extern const uint16_t fs_quad_indices[] = {0, 1, 2, 0, 2, 3};
const int TempBufSize = 256;
#include "__sphere_mesh.inl"
} // namespace PrimDrawInternal

PrimDraw::~PrimDraw() = default;

bool PrimDraw::LazyInit(Ren::Context &ctx) {
    using namespace PrimDrawInternal;

    Ren::BufferRef vtx_buf1 = ctx.default_vertex_buf1(), vtx_buf2 = ctx.default_vertex_buf2(),
                   ndx_buf = ctx.default_indices_buf();

    if (!initialized_) {
        ctx_ = &ctx;

        { // Allocate quad vertices
            uint32_t mem_required = sizeof(fs_quad_positions) + sizeof(fs_quad_norm_uvs);
            mem_required += (16 - mem_required % 16); // align to vertex stride

            Ren::BufferRef temp_stage_buf =
                ctx.CreateBuffer("Temp prim buf", Ren::eBufType::Stage, sizeof(__sphere_indices));
            { // copy quad vertices
                uint8_t *mapped_ptr = temp_stage_buf->Map(Ren::BufMapWrite);
                memcpy(mapped_ptr, fs_quad_positions, sizeof(fs_quad_positions));
                memcpy(mapped_ptr + sizeof(fs_quad_positions), fs_quad_norm_uvs, sizeof(fs_quad_norm_uvs));
                temp_stage_buf->FlushMappedRange(0, sizeof(fs_quad_positions) + sizeof(fs_quad_norm_uvs));
                temp_stage_buf->Unmap();
            }

            quad_vtx1_offset_ = vtx_buf1->AllocRegion(mem_required, "quad", temp_stage_buf.get());
            quad_vtx2_offset_ = vtx_buf2->AllocRegion(mem_required, "quad", nullptr);
            assert(quad_vtx1_offset_ == quad_vtx2_offset_ && "Offsets do not match!");
        }

        { // Allocate quad indices
            Ren::Buffer temp_stage_buf("Temp prim buf", ctx.api_ctx(), Ren::eBufType::Stage, 6 * sizeof(uint16_t));

            { // copy quad indices
                uint8_t *mapped_ptr = temp_stage_buf.Map(Ren::BufMapWrite);
                memcpy(mapped_ptr, fs_quad_indices, 6 * sizeof(uint16_t));
                temp_stage_buf.FlushMappedRange(0, 6 * sizeof(uint16_t));
                temp_stage_buf.Unmap();
            }

            quad_ndx_offset_ = ndx_buf->AllocRegion(6 * sizeof(uint16_t), "quad", &temp_stage_buf);
        }

        { // Allocate sphere positions
            // aligned to vertex stride
            const size_t sphere_vertices_size = sizeof(__sphere_positions) + (16 - sizeof(__sphere_positions) % 16);

            Ren::Buffer temp_stage_buf("Temp prim buf", ctx.api_ctx(), Ren::eBufType::Stage, sphere_vertices_size);

            { // copy sphere positions
                uint8_t *mapped_ptr = temp_stage_buf.Map(Ren::BufMapWrite);
                memcpy(mapped_ptr, __sphere_positions, sphere_vertices_size);
                temp_stage_buf.FlushMappedRange(0, sphere_vertices_size);
                temp_stage_buf.Unmap();
            }

            // Allocate sphere vertices
            sphere_vtx1_offset_ = vtx_buf1->AllocRegion(sphere_vertices_size, "sphere", &temp_stage_buf);
            sphere_vtx2_offset_ = vtx_buf2->AllocRegion(sphere_vertices_size, "sphere", nullptr);
            assert(sphere_vtx1_offset_ == sphere_vtx2_offset_ && "Offsets do not match!");
        }

        { // Allocate sphere indices
            Ren::Buffer temp_stage_buf("Temp prim buf", ctx.api_ctx(), Ren::eBufType::Stage, sizeof(__sphere_indices));

            { // copy sphere indices
                uint8_t *mapped_ptr = temp_stage_buf.Map(Ren::BufMapWrite);
                memcpy(mapped_ptr, __sphere_indices, sizeof(__sphere_indices));
                temp_stage_buf.FlushMappedRange(0, sizeof(__sphere_indices));
                temp_stage_buf.Unmap();
            }
            sphere_ndx_offset_ = ndx_buf->AllocRegion(sizeof(__sphere_indices), "sphere", &temp_stage_buf);

            // Allocate temporary buffer
            temp_buf1_vtx_offset_ = vtx_buf1->AllocRegion(TempBufSize, "temp");
            temp_buf2_vtx_offset_ = vtx_buf2->AllocRegion(TempBufSize, "temp");
            assert(temp_buf1_vtx_offset_ == temp_buf2_vtx_offset_ && "Offsets do not match!");
            temp_buf_ndx_offset_ = ndx_buf->AllocRegion(TempBufSize, "temp");
        }

        // TODO: make this non-gl specific!
        /*glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, vtx_buf1->id());
        glBufferSubData(GL_ARRAY_BUFFER, (GLintptr)quad_vtx1_offset_, sizeof(fs_quad_positions), fs_quad_positions);
        glBufferSubData(GL_ARRAY_BUFFER, (GLintptr)(quad_vtx1_offset_ + sizeof(fs_quad_positions)),
                        sizeof(fs_quad_norm_uvs), fs_quad_norm_uvs);*/

        initialized_ = true;
    }

    { // setup quad vao
        const Ren::VtxAttribDesc attribs[] = {
            {vtx_buf1->handle(), REN_VTX_POS_LOC, 2, Ren::eType::Float32, 0, uintptr_t(quad_vtx1_offset_)},
            {vtx_buf1->handle(), REN_VTX_UV1_LOC, 2, Ren::eType::Float32, 0,
             uintptr_t(quad_vtx1_offset_ + 8 * sizeof(float))}};

        fs_quad_vao_.Setup(attribs, 2, ndx_buf->handle());
    }

    { // setup sphere vao
        const Ren::VtxAttribDesc attribs[] = {
            {vtx_buf1->handle(), REN_VTX_POS_LOC, 3, Ren::eType::Float32, 0, uintptr_t(sphere_vtx1_offset_)}};
        sphere_vao_.Setup(attribs, 1, ndx_buf->handle());
    }

    return true;
}

void PrimDraw::CleanUp() {
    Ren::BufferRef vtx_buf1 = ctx_->default_vertex_buf1(), vtx_buf2 = ctx_->default_vertex_buf2(),
                   ndx_buf = ctx_->default_indices_buf();

    if (quad_vtx1_offset_ != 0xffffffff) {
        vtx_buf1->FreeRegion(quad_vtx1_offset_);
        assert(quad_vtx2_offset_ != 0xffffffff);
        vtx_buf2->FreeRegion(quad_vtx2_offset_);
        assert(quad_ndx_offset_ != 0xffffffff);
        ndx_buf->FreeRegion(quad_ndx_offset_);
    }

    if (sphere_vtx1_offset_ != 0xffffffff) {
        vtx_buf1->FreeRegion(sphere_vtx1_offset_);
        assert(sphere_vtx2_offset_ != 0xffffffff);
        vtx_buf2->FreeRegion(sphere_vtx2_offset_);
        assert(sphere_ndx_offset_ != 0xffffffff);
        ndx_buf->FreeRegion(sphere_ndx_offset_);
    }

    if (temp_buf1_vtx_offset_ != 0xffffffff) {
        vtx_buf1->FreeRegion(temp_buf1_vtx_offset_);
        assert(temp_buf2_vtx_offset_ != 0xffffffff);
        vtx_buf2->FreeRegion(temp_buf2_vtx_offset_);
        assert(temp_buf_ndx_offset_ != 0xffffffff);
        ndx_buf->FreeRegion(temp_buf_ndx_offset_);
    }
}

void PrimDraw::DrawPrim(const ePrim prim, const RenderTarget &rt, Ren::Program *p, const Binding bindings[],
                        int bindings_count, const Uniform uniforms[], int uniforms_count) {
    using namespace PrimDrawInternal;

    glBindFramebuffer(GL_FRAMEBUFFER, rt.fb->id());
    // glViewport(rt.viewport[0], rt.viewport[1], rt.viewport[2], rt.viewport[3]);

    for (int i = 0; i < bindings_count; i++) {
        const auto &b = bindings[i];
        if (b.trg == Ren::eBindTarget::UBuf) {
            if (b.offset) {
                assert(b.size != 0);
                glBindBufferRange(GL_UNIFORM_BUFFER, b.loc, b.handle.buf->id(), b.offset, b.size);
            } else {
                glBindBufferBase(GL_UNIFORM_BUFFER, b.loc, b.handle.buf->id());
            }
        } else if (b.trg == Ren::eBindTarget::TexCubeArray) {
            ren_glBindTextureUnit_Comp(Ren::GLBindTarget(b.trg), GLuint(b.loc),
                                       GLuint(b.handle.cube_arr ? b.handle.cube_arr->handle().id : 0));
        } else if (b.trg == Ren::eBindTarget::TexBuf) {
            ren_glBindTextureUnit_Comp(Ren::GLBindTarget(b.trg), GLuint(b.loc), GLuint(b.handle.tex_buf->id()));
        } else {
            ren_glBindTextureUnit_Comp(Ren::GLBindTarget(b.trg), GLuint(b.loc), GLuint(b.handle.tex->id()));
        }
    }

    glUseProgram(p->id());

    for (int i = 0; i < uniforms_count; i++) {
        const auto &u = uniforms[i];
        if (u.type == Ren::eType::Float32) {
            if (u.size == 1) {
                glUniform1f(GLint(u.loc), u.fdata[0]);
            } else if (u.size == 2) {
                glUniform2f(GLint(u.loc), u.fdata[0], u.fdata[1]);
            } else if (u.size == 3) {
                glUniform3f(GLint(u.loc), u.fdata[0], u.fdata[1], u.fdata[2]);
            } else if (u.size == 4) {
                glUniform4f(GLint(u.loc), u.fdata[0], u.fdata[1], u.fdata[2], u.fdata[3]);
            } else {
                assert(u.size % 4 == 0);
                glUniformMatrix4fv(GLint(u.loc), 1, GL_FALSE, u.pfdata);
            }
        } else if (u.type == Ren::eType::Int32) {
            if (u.size == 1) {
                glUniform1i(GLint(u.loc), u.idata[0]);
            } else if (u.size == 2) {
                glUniform2i(GLint(u.loc), u.idata[0], u.idata[1]);
            } else if (u.size == 3) {
                glUniform3i(GLint(u.loc), u.idata[0], u.idata[1], u.idata[2]);
            } else if (u.size == 4) {
                glUniform4i(GLint(u.loc), u.idata[0], u.idata[1], u.idata[2], u.idata[3]);
            } else {
                assert(u.size % 4 == 0);
                glUniform4iv(GLint(u.loc), GLsizei(u.size / 4), u.pidata);
            }
        }
    }

    if (prim == ePrim::Quad) {
        glBindVertexArray(fs_quad_vao_.id());
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, (const GLvoid *)uintptr_t(quad_ndx_offset_));
    } else if (prim == ePrim::Sphere) {
        glBindVertexArray(sphere_vao_.id());
        glDrawElements(GL_TRIANGLES, GLsizei(__sphere_indices_count), GL_UNSIGNED_SHORT,
                       (void *)uintptr_t(sphere_ndx_offset_));
    }

#ifndef NDEBUG
    Ren::ResetGLState();
#endif
}

void PrimDraw::DrawPrim(const ePrim prim, const Ren::Program *p, const Ren::Framebuffer &fb, const Ren::RenderPass &rp,
                        const Binding bindings[], const int bindings_count, const void *uniform_data,
                        const int uniform_data_len, const int uniform_data_offset) {
    using namespace PrimDrawInternal;

    glBindFramebuffer(GL_FRAMEBUFFER, fb.id());
    // glViewport(rt.viewport[0], rt.viewport[1], rt.viewport[2], rt.viewport[3]);

    for (int i = 0; i < bindings_count; i++) {
        const auto &b = bindings[i];
        if (b.trg == Ren::eBindTarget::UBuf) {
            if (b.offset) {
                assert(b.size != 0);
                glBindBufferRange(GL_UNIFORM_BUFFER, b.loc, b.handle.buf->id(), b.offset, b.size);
            } else {
                glBindBufferBase(GL_UNIFORM_BUFFER, b.loc, b.handle.buf->id());
            }
        } else if (b.trg == Ren::eBindTarget::TexCubeArray) {
            ren_glBindTextureUnit_Comp(Ren::GLBindTarget(b.trg), GLuint(b.loc), GLuint(b.handle.cube_arr->handle().id));
        } else if (b.trg == Ren::eBindTarget::TexBuf) {
            ren_glBindTextureUnit_Comp(Ren::GLBindTarget(b.trg), GLuint(b.loc), GLuint(b.handle.tex_buf->id()));
        } else {
            ren_glBindTextureUnit_Comp(Ren::GLBindTarget(b.trg), GLuint(b.loc), GLuint(b.handle.tex->id()));
        }
    }

    glUseProgram(p->id());

    Ren::Buffer temp_stage_buffer, temp_unif_buffer;
    if (uniform_data) {
        temp_stage_buffer = Ren::Buffer("Temp stage buf", ctx_->api_ctx(), Ren::eBufType::Stage, uniform_data_len);
        {
            uint8_t *stage_data = temp_stage_buffer.Map(Ren::BufMapWrite);
            memcpy(stage_data, uniform_data, uniform_data_len);
            temp_stage_buffer.Unmap();
        }
        temp_unif_buffer = Ren::Buffer("Temp uniform buf", ctx_->api_ctx(), Ren::eBufType::Uniform, uniform_data_len);
        Ren::CopyBufferToBuffer(temp_stage_buffer, 0, temp_unif_buffer, 0, uniform_data_len, nullptr);

        glBindBufferBase(GL_UNIFORM_BUFFER, REN_UB_UNIF_PARAM_LOC, temp_unif_buffer.id());
    }

    if (prim == ePrim::Quad) {
        glBindVertexArray(fs_quad_vao_.id());
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, (const GLvoid *)uintptr_t(quad_ndx_offset_));
    } else if (prim == ePrim::Sphere) {
        glBindVertexArray(sphere_vao_.id());
        glDrawElements(GL_TRIANGLES, GLsizei(__sphere_indices_count), GL_UNSIGNED_SHORT,
                       (void *)uintptr_t(sphere_ndx_offset_));
    }

#ifndef NDEBUG
    Ren::ResetGLState();
#endif
}