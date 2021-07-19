#include "PrimDraw.h"

#include <Ren/Context.h>
#include <Ren/VKCtx.h>

#include "Renderer_GL_Defines.inl"

namespace PrimDrawInternal {
extern const float fs_quad_positions[] = {-1.0f, -1.0f, 1.0f, -1.0f, 1.0f, 1.0f, -1.0f, 1.0f};
extern const float fs_quad_norm_uvs[] = {0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f};
extern const uint16_t fs_quad_indices[] = {0, 1, 2, 0, 2, 3};
const int TempBufSize = 256;
#include "__sphere_mesh.inl"
} // namespace PrimDrawInternal

bool PrimDraw::LazyInit(Ren::Context &ctx) {
    using namespace PrimDrawInternal;

    Ren::BufferRef vtx_buf1 = ctx.default_vertex_buf1(), vtx_buf2 = ctx.default_vertex_buf2(),
                   ndx_buf = ctx.default_indices_buf();

    if (!initialized_) {
        { // Allocate quad vertices
            uint32_t mem_required = sizeof(fs_quad_positions) + sizeof(fs_quad_norm_uvs);
            mem_required += (16 - mem_required % 16); // align to vertex stride

            Ren::Buffer temp_stage_buf("Temp prim buf", ctx.api_ctx(), Ren::eBufType::Stage, sizeof(__sphere_indices));

            { // copy quad vertices
                uint8_t *mapped_ptr = temp_stage_buf.Map(Ren::BufMapWrite);
                memcpy(mapped_ptr, fs_quad_positions, sizeof(fs_quad_positions));
                memcpy(mapped_ptr + sizeof(fs_quad_positions), fs_quad_norm_uvs, sizeof(fs_quad_norm_uvs));
                temp_stage_buf.FlushMappedRange(0, sizeof(fs_quad_positions) + sizeof(fs_quad_norm_uvs));
                temp_stage_buf.Unmap();
            }

            quad_vtx1_offset_ = vtx_buf1->AllocRegion(mem_required, "quad", &temp_stage_buf, ctx.current_cmd_buf());
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

            quad_ndx_offset_ =
                ndx_buf->AllocRegion(6 * sizeof(uint16_t), "quad", &temp_stage_buf, ctx.current_cmd_buf());
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
            sphere_vtx1_offset_ =
                vtx_buf1->AllocRegion(sphere_vertices_size, "sphere", &temp_stage_buf, ctx.current_cmd_buf());
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
            sphere_ndx_offset_ =
                ndx_buf->AllocRegion(sizeof(__sphere_indices), "sphere", &temp_stage_buf, ctx.current_cmd_buf());

            // Allocate temporary buffer
            temp_buf1_vtx_offset_ = vtx_buf1->AllocRegion(TempBufSize, "temp");
            temp_buf2_vtx_offset_ = vtx_buf2->AllocRegion(TempBufSize, "temp");
            assert(temp_buf1_vtx_offset_ == temp_buf2_vtx_offset_ && "Offsets do not match!");
            temp_buf_ndx_offset_ = ndx_buf->AllocRegion(TempBufSize, "temp");
        }

        api_ctx_ = ctx.api_ctx();
        initialized_ = true;
    }

#if 0
    { // setup quad vao
        const Ren::VtxAttribDesc attribs[] = {
            {vtx_buf1->handle(), REN_VTX_POS_LOC, 2, Ren::eType::Float32, 0,
             uintptr_t(quad_vtx1_offset_)},
            {vtx_buf1->handle(), REN_VTX_UV1_LOC, 2, Ren::eType::Float32, 0,
             uintptr_t(quad_vtx1_offset_ + 8 * sizeof(float))}};

        fs_quad_vao_.Setup(attribs, 2, ndx_buf->handle());
    }

    { // setup sphere vao
        const Ren::VtxAttribDesc attribs[] = {{vtx_buf1->handle(), REN_VTX_POS_LOC, 3,
                                               Ren::eType::Float32, 0,
                                               uintptr_t(sphere_vtx1_offset_)}};
        sphere_vao_.Setup(attribs, 1, ndx_buf->handle());
    }
#endif

    return true;
}

void PrimDraw::CleanUp(Ren::Context &ctx) {
    Ren::BufferRef vtx_buf1 = ctx.default_vertex_buf1(), vtx_buf2 = ctx.default_vertex_buf2(),
                   ndx_buf = ctx.default_indices_buf();

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
                        const int bindings_count, const void *uniform_data, const int uniform_data_len,
                        const int uniform_data_offset) {
    using namespace PrimDrawInternal;

    VkPipeline pipeline = FindOrCreatePipeline(p, bindings, bindings_count);
#if 0
    glBindFramebuffer(GL_FRAMEBUFFER, rt.fb);
    // glViewport(rt.viewport[0], rt.viewport[1], rt.viewport[2], rt.viewport[3]);

    for (int i = 0; i < bindings_count; i++) {
        const auto &b = bindings[i];
        if (b.trg == Ren::eBindTarget::UBuf) {
            if (b.offset) {
                assert(b.size != 0);
                glBindBufferRange(GL_UNIFORM_BUFFER, b.loc, b.handle.id, b.offset,
                                  b.size);
            } else {
                glBindBufferBase(GL_UNIFORM_BUFFER, b.loc, b.handle.id);
            }
        } else {
            ren_glBindTextureUnit_Comp(Ren::GLBindTarget(b.trg), GLuint(b.loc),
                                       GLuint(b.handle.id));
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
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT,
                       (const GLvoid *)uintptr_t(quad_ndx_offset_));
    } else if (prim == ePrim::Sphere) {
        glBindVertexArray(sphere_vao_.id());
        glDrawElements(GL_TRIANGLES, GLsizei(__sphere_indices_count), GL_UNSIGNED_SHORT,
                       (void *)uintptr_t(sphere_ndx_offset_));
    }

#ifndef NDEBUG
    Ren::ResetGLState();
#endif
#endif
}

VkPipeline PrimDraw::FindOrCreatePipeline(Ren::Program *p, const Binding bindings[], const int bindings_count) {
    for (size_t i = 0; i < pipelines_.size(); ++i) {
        if (pipelines_[i].p == p) {
            return pipelines_[i].pipeline;
        }
    }

    VkPipelineLayout new_layout;

    { // create pipeline layout
        VkPipelineLayoutCreateInfo layout_create_info = {};
        layout_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layout_create_info.setLayoutCount = 1;
        layout_create_info.pSetLayouts = p->descr_set_layouts();

        layout_create_info.pushConstantRangeCount = p->pc_range_count();
        if (p->pc_range_count()) {
            layout_create_info.pPushConstantRanges = p->pc_ranges();
        }

        const VkResult res = vkCreatePipelineLayout(api_ctx_->device, &layout_create_info, nullptr, &new_layout);
        if (res != VK_SUCCESS) {
            log_->Error("Failed to create pipeline layout!");
            return VK_NULL_HANDLE;
        }
    }

    /*{ // create renderpass
        VkAttachmentDescription pass_attachments[3] = {};
        pass_attachments[0].format = Ren::VKFormatFromTexFormat(color_tex.desc.format);
        pass_attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
        pass_attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        pass_attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        pass_attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        pass_attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        pass_attachments[0].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        pass_attachments[0].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        pass_attachments[1].format = Ren::VKFormatFromTexFormat(spec_tex.desc.format);
        pass_attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
        pass_attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        pass_attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        pass_attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        pass_attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        pass_attachments[1].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        pass_attachments[1].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        pass_attachments[2].format = Ren::VKFormatFromTexFormat(depth_tex.desc.format);
        pass_attachments[2].samples = VK_SAMPLE_COUNT_1_BIT;
        pass_attachments[2].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        pass_attachments[2].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        pass_attachments[2].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        pass_attachments[2].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        pass_attachments[2].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        pass_attachments[2].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkAttachmentReference color_attachment_refs[REN_MAX_ATTACHMENTS];
        for (int i = 0; i < REN_MAX_ATTACHMENTS; i++) {
            color_attachment_refs[i] = {VK_ATTACHMENT_UNUSED, VK_IMAGE_LAYOUT_UNDEFINED};
        }

        color_attachment_refs[REN_OUT_COLOR_INDEX].attachment = 0;
        color_attachment_refs[REN_OUT_COLOR_INDEX].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        color_attachment_refs[REN_OUT_SPEC_INDEX].attachment = 1;
        color_attachment_refs[REN_OUT_SPEC_INDEX].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkAttachmentReference depth_attachment_ref;
        depth_attachment_ref.attachment = 2;
        depth_attachment_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass = {};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = REN_MAX_ATTACHMENTS;
        subpass.pColorAttachments = color_attachment_refs;
        subpass.pDepthStencilAttachment = &depth_attachment_ref;

        VkRenderPassCreateInfo render_pass_create_info = {};
        render_pass_create_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        render_pass_create_info.attachmentCount = 3;
        render_pass_create_info.pAttachments = pass_attachments;
        render_pass_create_info.subpassCount = 1;
        render_pass_create_info.pSubpasses = &subpass;

        const VkResult res = vkCreateRenderPass(api_ctx->device, &render_pass_create_info, nullptr, &render_pass_);
        if (res != VK_SUCCESS) {
            ctx.log()->Error("Failed to create render pass!");
            return false;
        }
    }*/

    return VK_NULL_HANDLE;
}