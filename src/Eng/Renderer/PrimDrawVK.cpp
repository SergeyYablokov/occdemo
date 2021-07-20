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

void PrimDraw::DrawPrim(const ePrim prim, const Ren::Program *p, const Ren::Framebuffer &fb, const Ren::RenderPass &rp,
                        const Binding bindings[], const int bindings_count, const void *uniform_data,
                        const int uniform_data_len, const int uniform_data_offset) {
    using namespace PrimDrawInternal;

    VkPipeline pipeline = FindOrCreatePipeline(p, rp, bindings, bindings_count);

    // VkCommandBuffer cmd_buf = api_ctx_->draw_cmd_buf[api_ctx_->backend_frame];

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

VkPipeline PrimDraw::FindOrCreatePipeline(const Ren::Program *p, const Ren::RenderPass &rp, const Binding bindings[],
                                          const int bindings_count) {
    for (size_t i = 0; i < pipelines_.size(); ++i) {
        if (pipelines_[i].p == p && pipelines_[i].rp == &rp) {
            return pipelines_[i].pipeline;
        }
    }

    CachedPipeline &new_pipeline = pipelines_.emplace_back();

    new_pipeline.p = p;
    new_pipeline.rp = &rp;

    { // create pipeline layout
        VkPipelineLayoutCreateInfo layout_create_info = {};
        layout_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layout_create_info.setLayoutCount = 1;
        layout_create_info.pSetLayouts = p->descr_set_layouts();

        layout_create_info.pushConstantRangeCount = p->pc_range_count();
        if (p->pc_range_count()) {
            layout_create_info.pPushConstantRanges = p->pc_ranges();
        }

        const VkResult res =
            vkCreatePipelineLayout(api_ctx_->device, &layout_create_info, nullptr, &new_pipeline.layout);
        if (res != VK_SUCCESS) {
            log_->Error("Failed to create pipeline layout!");
            pipelines_.pop_back();
            return VK_NULL_HANDLE;
        }
    }

    { // create graphics pipeline
        VkPipelineShaderStageCreateInfo shader_stage_create_info[2] = {};
        shader_stage_create_info[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shader_stage_create_info[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
        shader_stage_create_info[0].module = p->shader(Ren::eShaderType::Vert)->module();
        shader_stage_create_info[0].pName = "main";
        shader_stage_create_info[0].pSpecializationInfo = nullptr;

        shader_stage_create_info[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shader_stage_create_info[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        shader_stage_create_info[1].module = p->shader(Ren::eShaderType::Frag)->module();
        shader_stage_create_info[1].pName = "main";
        shader_stage_create_info[1].pSpecializationInfo = nullptr;

        const int buf1_stride = 16, buf2_stride = 16;

        VkVertexInputBindingDescription vtx_binding_desc[1] = {};
        vtx_binding_desc[0].binding = 0;
        vtx_binding_desc[0].stride = buf1_stride;
        vtx_binding_desc[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        VkVertexInputAttributeDescription vtx_attrib_desc[2] = {};
        vtx_attrib_desc[0].binding = 0;
        vtx_attrib_desc[0].location = REN_VTX_POS_LOC;
        vtx_attrib_desc[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        vtx_attrib_desc[0].offset = 0;

        vtx_attrib_desc[1].binding = 0;
        vtx_attrib_desc[1].location = REN_VTX_UV1_LOC;
        vtx_attrib_desc[1].format = VK_FORMAT_R32G32_SFLOAT;
        vtx_attrib_desc[1].offset = 8 * sizeof(float);

        VkPipelineVertexInputStateCreateInfo vtx_input_state_create_info = {};
        vtx_input_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vtx_input_state_create_info.vertexBindingDescriptionCount = 1;
        vtx_input_state_create_info.pVertexBindingDescriptions = vtx_binding_desc;
        vtx_input_state_create_info.vertexAttributeDescriptionCount = 2;
        vtx_input_state_create_info.pVertexAttributeDescriptions = vtx_attrib_desc;

        VkPipelineInputAssemblyStateCreateInfo input_assembly_state_create_info = {};
        input_assembly_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        input_assembly_state_create_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        input_assembly_state_create_info.primitiveRestartEnable = VK_FALSE;

        VkViewport viewport = {};
        viewport.x = 0;
        viewport.y = 0;
        viewport.width = 1.0f;
        viewport.height = 1.0f;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;

        VkRect2D scissors = {};
        scissors.offset = {0, 0};
        scissors.extent = VkExtent2D{uint32_t(1), uint32_t(1)};

        VkPipelineViewportStateCreateInfo viewport_state_ci = {};
        viewport_state_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewport_state_ci.viewportCount = 1;
        viewport_state_ci.pViewports = &viewport;
        viewport_state_ci.scissorCount = 1;
        viewport_state_ci.pScissors = &scissors;

        VkPipelineRasterizationStateCreateInfo rasterization_state_ci = {};
        rasterization_state_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterization_state_ci.depthClampEnable = VK_FALSE;
        rasterization_state_ci.rasterizerDiscardEnable = VK_FALSE;
        rasterization_state_ci.polygonMode = VK_POLYGON_MODE_FILL;
        rasterization_state_ci.cullMode = VK_CULL_MODE_BACK_BIT;
        rasterization_state_ci.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        rasterization_state_ci.depthBiasEnable = VK_FALSE;
        rasterization_state_ci.depthBiasConstantFactor = 0.0f;
        rasterization_state_ci.depthBiasClamp = 0.0f;
        rasterization_state_ci.depthBiasSlopeFactor = 0.0f;
        rasterization_state_ci.lineWidth = 1.0f;

        VkPipelineMultisampleStateCreateInfo multisample_state_ci = {};
        multisample_state_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisample_state_ci.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        multisample_state_ci.sampleShadingEnable = VK_FALSE;
        multisample_state_ci.minSampleShading = 0;
        multisample_state_ci.pSampleMask = nullptr;
        multisample_state_ci.alphaToCoverageEnable = VK_FALSE;
        multisample_state_ci.alphaToOneEnable = VK_FALSE;

        VkStencilOpState noop_stencil_state = {};
        noop_stencil_state.failOp = VK_STENCIL_OP_KEEP;
        noop_stencil_state.passOp = VK_STENCIL_OP_KEEP;
        noop_stencil_state.depthFailOp = VK_STENCIL_OP_KEEP;
        noop_stencil_state.compareOp = VK_COMPARE_OP_ALWAYS;
        noop_stencil_state.compareMask = 0;
        noop_stencil_state.writeMask = 0;
        noop_stencil_state.reference = 0;

        VkPipelineDepthStencilStateCreateInfo depth_state_ci = {};
        depth_state_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depth_state_ci.depthTestEnable = VK_FALSE;
        depth_state_ci.depthWriteEnable = VK_FALSE;
        depth_state_ci.depthCompareOp = VK_COMPARE_OP_ALWAYS;
        depth_state_ci.depthBoundsTestEnable = VK_FALSE;
        depth_state_ci.stencilTestEnable = VK_FALSE;
        depth_state_ci.front = noop_stencil_state;
        depth_state_ci.back = noop_stencil_state;
        depth_state_ci.minDepthBounds = 0.0f;
        depth_state_ci.maxDepthBounds = 1.0f;

        VkPipelineColorBlendAttachmentState color_blend_attachment_states[Ren::MaxRTAttachments] = {};
        for (size_t i = 0; i < rp.color_attachments_.size(); ++i) {
            color_blend_attachment_states[i].blendEnable = VK_FALSE;
            color_blend_attachment_states[i].colorWriteMask = 0xf;
        }

        VkPipelineColorBlendStateCreateInfo color_blend_state_ci = {};
        color_blend_state_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        color_blend_state_ci.logicOpEnable = VK_FALSE;
        color_blend_state_ci.logicOp = VK_LOGIC_OP_CLEAR;
        color_blend_state_ci.attachmentCount = uint32_t(rp.color_attachments_.size());
        color_blend_state_ci.pAttachments = color_blend_attachment_states;
        color_blend_state_ci.blendConstants[0] = 0.0f;
        color_blend_state_ci.blendConstants[1] = 0.0f;
        color_blend_state_ci.blendConstants[2] = 0.0f;
        color_blend_state_ci.blendConstants[3] = 0.0f;

        VkDynamicState dynamic_states[2] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo dynamic_state_ci = {};
        dynamic_state_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamic_state_ci.dynamicStateCount = 2;
        dynamic_state_ci.pDynamicStates = dynamic_states;

        VkGraphicsPipelineCreateInfo pipeline_create_info = {};
        pipeline_create_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipeline_create_info.stageCount = 2;
        pipeline_create_info.pStages = shader_stage_create_info;
        pipeline_create_info.pVertexInputState = &vtx_input_state_create_info;
        pipeline_create_info.pInputAssemblyState = &input_assembly_state_create_info;
        pipeline_create_info.pTessellationState = nullptr;
        pipeline_create_info.pViewportState = &viewport_state_ci;
        pipeline_create_info.pRasterizationState = &rasterization_state_ci;
        pipeline_create_info.pMultisampleState = &multisample_state_ci;
        pipeline_create_info.pDepthStencilState = &depth_state_ci;
        pipeline_create_info.pColorBlendState = &color_blend_state_ci;
        pipeline_create_info.pDynamicState = &dynamic_state_ci;
        pipeline_create_info.layout = new_pipeline.layout;
        pipeline_create_info.renderPass = rp.handle();
        pipeline_create_info.subpass = 0;
        pipeline_create_info.basePipelineHandle = VK_NULL_HANDLE;
        pipeline_create_info.basePipelineIndex = 0;

        const VkResult res = vkCreateGraphicsPipelines(api_ctx_->device, VK_NULL_HANDLE, 1, &pipeline_create_info,
                                                       nullptr, &new_pipeline.pipeline);
        if (res != VK_SUCCESS) {
            log_->Error("Failed to create graphics pipeline!");
            pipelines_.pop_back();
            return VK_NULL_HANDLE;
        }
    }

    return VK_NULL_HANDLE;
}