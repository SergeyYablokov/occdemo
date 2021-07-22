#include "PrimDraw.h"

#include <Ren/Context.h>
#include <Ren/VKCtx.h>

#include "Renderer_GL_Defines.inl"

namespace PrimDrawInternal {
extern const float fs_quad_positions[] = {-1.0f, -1.0f, 1.0f, -1.0f, 1.0f, 1.0f, -1.0f, 1.0f};
extern const float fs_quad_norm_uvs[] = {0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f};
extern const uint16_t fs_quad_indices[] = {0, 1, 2, 0, 2, 3};
const int TempBufSize = 256;
#include "__sphere_mesh.inl"
} // namespace PrimDrawInternal

PrimDraw::~PrimDraw() {
    if (ctx_) {
        Ren::ApiContext *api_ctx = ctx_->api_ctx();
        for (auto &pipeline : pipelines_) {
            vkDestroyPipelineLayout(api_ctx->device, pipeline.layout, nullptr);
            vkDestroyDescriptorPool(api_ctx->device, pipeline.desc_pool, nullptr);
            vkDestroyPipeline(api_ctx->device, pipeline.pipeline, nullptr);
        }
    }
}

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

        ctx_ = &ctx;
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

void PrimDraw::Reset() {}

void PrimDraw::DrawPrim(const ePrim prim, const Ren::Program *p, const Ren::Framebuffer &fb, const Ren::RenderPass &rp,
                        const Binding bindings[], const int bindings_count, const void *uniform_data,
                        const int uniform_data_len, const int uniform_data_offset) {
    using namespace PrimDrawInternal;

    Ren::ApiContext *api_ctx = ctx_->api_ctx();

    VkPipeline pipeline;
    VkPipelineLayout pipeline_layout;
    std::tie(pipeline, pipeline_layout) = FindOrCreatePipeline(p, rp, bindings, bindings_count);

    VkDescriptorSetLayout descr_set_layout = p->descr_set_layouts()[0];
    VkDescriptorSet descr_set = VK_NULL_HANDLE;

    { // allocate and update descriptor set
        VkDescriptorImageInfo img_infos[16];
        uint32_t img_infos_count = 0;
        VkDescriptorBufferInfo ubuf_infos[16];
        uint32_t ubuf_infos_count = 0;
        VkDescriptorBufferInfo sbuf_infos[16];
        uint32_t sbuf_infos_count = 0;

        Ren::SmallVector<VkWriteDescriptorSet, 48> descr_writes;

        for (int i = 0; i < bindings_count; ++i) {
            const auto &b = bindings[i];

            if (b.trg == Ren::eBindTarget::Tex2D) {
                auto &info = img_infos[img_infos_count++];
                info.sampler = b.handle.tex->handle().sampler;
                info.imageView = b.handle.tex->handle().view;
                info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

                auto &new_write = descr_writes.emplace_back();
                new_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                new_write.dstSet = {};
                new_write.dstBinding = b.loc;
                new_write.dstArrayElement = 0;
                new_write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                new_write.descriptorCount = 1;
                new_write.pBufferInfo = nullptr;
                new_write.pImageInfo = &info;
                new_write.pTexelBufferView = nullptr;
                new_write.pNext = nullptr;
            } else if (b.trg == Ren::eBindTarget::UBuf) {
                auto &ubuf = ubuf_infos[ubuf_infos_count++];
                ubuf.buffer = b.handle.buf->handle().buf;
                ubuf.offset = b.offset;
                ubuf.range = b.offset ? b.size : VK_WHOLE_SIZE;
            } // else if (b.trg == Ren::eBindTarget::TexBuf)
        }

        descr_set =
            ctx_->default_descr_alloc()->Alloc(img_infos_count, ubuf_infos_count, sbuf_infos_count, descr_set_layout);
        if (!descr_set) {
            ctx_->log()->Error("Failed to allocate descriptor set!");
            return;
        }

        for (auto &d : descr_writes) {
            d.dstSet = descr_set;
        }

        vkUpdateDescriptorSets(api_ctx->device, uint32_t(descr_writes.size()), descr_writes.data(), 0, nullptr);
    }

    VkCommandBuffer cmd_buf = api_ctx->draw_cmd_buf[api_ctx->backend_frame];

    {
        VkPipelineStageFlags barrier_stages = 0;

        Ren::SmallVector<VkImageMemoryBarrier, 16> img_barriers;

        for (int i = 0; i < bindings_count; ++i) {
            const auto &b = bindings[i];

            if (b.trg == Ren::eBindTarget::Tex2D) {
                auto &new_barrier = img_barriers.emplace_back();
                new_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                new_barrier.oldLayout = b.handle.tex->layout;
                new_barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                new_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                new_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                new_barrier.image = b.handle.tex->handle().img;
                new_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                new_barrier.subresourceRange.baseMipLevel = 0;
                new_barrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
                new_barrier.subresourceRange.baseArrayLayer = 0;
                new_barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
                new_barrier.srcAccessMask = b.handle.tex->last_access_mask;
                new_barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
                barrier_stages |= b.handle.tex->last_stage_mask;

                b.handle.tex->layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                b.handle.tex->last_access_mask = VK_ACCESS_SHADER_READ_BIT;
                b.handle.tex->last_stage_mask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            }
        }

        for (const auto &att : fb.color_attachments) {
            auto &new_barrier = img_barriers.emplace_back();
            new_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            new_barrier.oldLayout = att.ref->layout;
            new_barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            new_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            new_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            new_barrier.image = att.ref->handle().img;
            new_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            new_barrier.subresourceRange.baseMipLevel = 0;
            new_barrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
            new_barrier.subresourceRange.baseArrayLayer = 0;
            new_barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
            new_barrier.srcAccessMask = att.ref->last_access_mask;
            new_barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            barrier_stages |= att.ref->last_stage_mask;
        }

        if (fb.depth_attachment.ref) {
            auto &new_barrier = img_barriers.emplace_back();
            new_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            new_barrier.oldLayout = fb.depth_attachment.ref->layout;
            new_barrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            new_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            new_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            new_barrier.image = fb.depth_attachment.ref->handle().img;
            new_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
            new_barrier.subresourceRange.baseMipLevel = 0;
            new_barrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
            new_barrier.subresourceRange.baseArrayLayer = 0;
            new_barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
            new_barrier.srcAccessMask = fb.depth_attachment.ref->last_access_mask;
            new_barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            barrier_stages |= fb.depth_attachment.ref->last_stage_mask;
        }

        vkCmdPipelineBarrier(cmd_buf, barrier_stages,
                             VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
                                 VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                                 VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
                             0, 0, nullptr, 0, nullptr, uint32_t(img_barriers.size()), img_barriers.data());
    }

    VkRenderPassBeginInfo render_pass_begin_info = {};
    render_pass_begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    render_pass_begin_info.renderPass = rp.handle();
    render_pass_begin_info.framebuffer = fb.handle();
    render_pass_begin_info.renderArea = {0, 0, uint32_t(fb.w), uint32_t(fb.h)};

    vkCmdBeginRenderPass(cmd_buf, &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

    const VkViewport viewport = {0.0f, 0.0f, float(fb.w), float(fb.h), 0.0f, 1.0f};
    vkCmdSetViewport(cmd_buf, 0, 1, &viewport);

    const VkRect2D scissor = {0, 0, uint32_t(fb.w), uint32_t(fb.h)};
    vkCmdSetScissor(cmd_buf, 0, 1, &scissor);

    vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout, 0, 1, &descr_set, 0, nullptr);

    vkCmdPushConstants(cmd_buf, pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                       uniform_data_offset, uniform_data_len, uniform_data);

    VkBuffer vtx_buf = ctx_->default_vertex_buf1()->handle().buf;
    VkBuffer ndx_buf = ctx_->default_indices_buf()->handle().buf;

    if (prim == ePrim::Quad) {
        const VkDeviceSize offset = {quad_vtx1_offset_};
        vkCmdBindVertexBuffers(cmd_buf, 0, 1, &vtx_buf, &offset);
        vkCmdBindIndexBuffer(cmd_buf, ndx_buf, VkDeviceSize(quad_ndx_offset_), VK_INDEX_TYPE_UINT16);
        vkCmdDrawIndexed(cmd_buf, uint32_t(6), // index count
                         1,                    // instance count
                         0,                    // first index
                         0,                    // vertex offset
                         0);                   // first instance
    }

    vkCmdEndRenderPass(cmd_buf);
}

std::pair<VkPipeline, VkPipelineLayout> PrimDraw::FindOrCreatePipeline(const Ren::Program *p, const Ren::RenderPass &rp,
                                                                       const Binding bindings[],
                                                                       const int bindings_count) {
    for (size_t i = 0; i < pipelines_.size(); ++i) {
        if (pipelines_[i].p == p && pipelines_[i].rp == &rp) {
            return std::make_pair(pipelines_[i].pipeline, pipelines_[i].layout);
        }
    }

    Ren::ApiContext *api_ctx = ctx_->api_ctx();
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
            vkCreatePipelineLayout(api_ctx->device, &layout_create_info, nullptr, &new_pipeline.layout);
        if (res != VK_SUCCESS) {
            log_->Error("Failed to create pipeline layout!");
            pipelines_.pop_back();
            return std::make_pair(VkPipeline{}, VkPipelineLayout{});
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
        vtx_binding_desc[0].stride = 2 * sizeof(float);
        vtx_binding_desc[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        VkVertexInputAttributeDescription vtx_attrib_desc[2] = {};
        vtx_attrib_desc[0].binding = 0;
        vtx_attrib_desc[0].location = REN_VTX_POS_LOC;
        vtx_attrib_desc[0].format = VK_FORMAT_R32G32_SFLOAT;
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
        for (size_t i = 0; i < rp.color_rts.size(); ++i) {
            color_blend_attachment_states[i].blendEnable = VK_FALSE;
            color_blend_attachment_states[i].colorWriteMask = 0xf;
        }

        VkPipelineColorBlendStateCreateInfo color_blend_state_ci = {};
        color_blend_state_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        color_blend_state_ci.logicOpEnable = VK_FALSE;
        color_blend_state_ci.logicOp = VK_LOGIC_OP_CLEAR;
        color_blend_state_ci.attachmentCount = uint32_t(rp.color_rts.size());
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

        const VkResult res = vkCreateGraphicsPipelines(api_ctx->device, VK_NULL_HANDLE, 1, &pipeline_create_info,
                                                       nullptr, &new_pipeline.pipeline);
        if (res != VK_SUCCESS) {
            log_->Error("Failed to create graphics pipeline!");
            pipelines_.pop_back();
            return std::make_pair(VkPipeline{}, VkPipelineLayout{});
        }
    }

    return std::make_pair(new_pipeline.pipeline, new_pipeline.layout);
}
