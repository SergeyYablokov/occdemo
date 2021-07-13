#include "RpSkydome.h"

#include "../Renderer_Structs.h"

#include <Ren/Context.h>

namespace RpSkydomeInternal {
extern const float __skydome_positions[];
extern const int __skydome_vertices_count;
} // namespace RpSkydomeInternal

void RpSkydome::DrawSkydome(RpBuilder &builder, RpAllocTex &color_tex, RpAllocTex &spec_tex, RpAllocTex &depth_tex) {
    using namespace RpSkydomeInternal;

    Ren::RastState rast_state;
    rast_state.cull_face.enabled = true;
    rast_state.cull_face.face = Ren::eCullFace::Back;
    rast_state.depth_test.enabled = true;
    rast_state.depth_test.func = Ren::eTestFunc::Always;
    rast_state.blend.enabled = false;

    rast_state.stencil.enabled = true;
    rast_state.stencil.mask = 0xff;
    rast_state.stencil.pass = Ren::eStencilOp::Replace;

    rast_state.viewport[2] = view_state_->act_res[0];
    rast_state.viewport[3] = view_state_->act_res[1];
    // Draw skydome without multisampling (not sure if it helps)
    rast_state.multisample = false;

    rast_state.Apply();

    RpAllocBuf &unif_shared_data_buf = builder.GetReadBuffer(shared_data_buf_);
    // glBindBufferBase(GL_UNIFORM_BUFFER, REN_UB_SHARED_DATA_LOC, GLuint(unif_shared_data_buf.ref->id()));

#if defined(REN_DIRECT_DRAWING)
    // glBindFramebuffer(GL_FRAMEBUFFER, 0);
#else
    // glBindFramebuffer(GL_FRAMEBUFFER, GLuint(cached_fb_.id()));
#endif
    /*glUseProgram(skydome_prog_->id());

    glBindVertexArray(skydome_vao_.id());

    Ren::Mat4f translate_matrix;
    translate_matrix = Ren::Translate(translate_matrix, draw_cam_pos_);

    Ren::Mat4f scale_matrix;
    scale_matrix = Ren::Scale(scale_matrix, Ren::Vec3f{5000.0f, 5000.0f, 5000.0f});

    const Ren::Mat4f world_from_object = translate_matrix * scale_matrix;
    glUniformMatrix4fv(REN_U_M_MATRIX_LOC, 1, GL_FALSE, Ren::ValuePtr(world_from_object));

    ren_glBindTextureUnit_Comp(GL_TEXTURE_CUBE_MAP, REN_BASE0_TEX_SLOT, env_->env_map->id());

    glDrawElements(GL_TRIANGLES, GLsizei(skydome_mesh_->indices_buf().size / sizeof(uint32_t)), GL_UNSIGNED_INT,
                   (void *)uintptr_t(skydome_mesh_->indices_buf().offset));

    glDepthFunc(GL_LESS);

    glDisable(GL_STENCIL_TEST);

    glEnable(GL_MULTISAMPLE);*/

    Ren::Context &ctx = builder.ctx();
    Ren::ApiContext *api_ctx = ctx.api_ctx();

    {                                               // update descriptor set
        const VkDescriptorBufferInfo buf_infos[] = {// shared data
                                                    {unif_shared_data_buf.ref->handle().buf, 0, VK_WHOLE_SIZE}};

        const VkDescriptorImageInfo img_infos[] = {// environment texture
                                                   env_->env_map->sampler().handle(), env_->env_map->handle().view,
                                                   VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};

        VkWriteDescriptorSet descr_writes[2];
        descr_writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descr_writes[0].dstSet = desc_set_[api_ctx->backend_frame];
        descr_writes[0].dstBinding = REN_UB_SHARED_DATA_LOC;
        descr_writes[0].dstArrayElement = 0;
        descr_writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descr_writes[0].descriptorCount = 1;
        descr_writes[0].pBufferInfo = buf_infos;
        descr_writes[0].pImageInfo = nullptr;
        descr_writes[0].pTexelBufferView = nullptr;
        descr_writes[0].pNext = nullptr;

        descr_writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descr_writes[1].dstSet = desc_set_[api_ctx->backend_frame];
        descr_writes[1].dstBinding = REN_BASE0_TEX_SLOT;
        descr_writes[1].dstArrayElement = 0;
        descr_writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descr_writes[1].descriptorCount = 1;
        descr_writes[1].pBufferInfo = nullptr;
        descr_writes[1].pImageInfo = img_infos;
        descr_writes[1].pTexelBufferView = nullptr;
        descr_writes[1].pNext = nullptr;

        vkUpdateDescriptorSets(api_ctx->device, 2, descr_writes, 0, nullptr);
    }

    VkCommandBuffer cmd_buf = api_ctx->draw_cmd_buf[api_ctx->backend_frame];

    {
        VkPipelineStageFlags barrier_stages = 0;

        VkBufferMemoryBarrier unif_buf_barrier = {};
        unif_buf_barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        unif_buf_barrier.srcAccessMask = unif_shared_data_buf.ref->last_access_mask;
        unif_buf_barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        unif_buf_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        unif_buf_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        unif_buf_barrier.buffer = unif_shared_data_buf.ref->handle().buf;
        unif_buf_barrier.offset = 0;
        unif_buf_barrier.size = VK_WHOLE_SIZE;
        barrier_stages |= unif_shared_data_buf.ref->last_stage_mask;

        int img_barriers_count = 0;
        VkImageMemoryBarrier img_barriers[4] = {};
        img_barriers[img_barriers_count].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        img_barriers[img_barriers_count].oldLayout = env_->env_map->layout;
        img_barriers[img_barriers_count].newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        img_barriers[img_barriers_count].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        img_barriers[img_barriers_count].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        img_barriers[img_barriers_count].image = env_->env_map->handle().img;
        img_barriers[img_barriers_count].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        img_barriers[img_barriers_count].subresourceRange.baseMipLevel = 0;
        img_barriers[img_barriers_count].subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
        img_barriers[img_barriers_count].subresourceRange.baseArrayLayer = 0;
        img_barriers[img_barriers_count].subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
        img_barriers[img_barriers_count].srcAccessMask = env_->env_map->last_access_mask;
        img_barriers[img_barriers_count].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        barrier_stages |= env_->env_map->last_stage_mask;
        ++img_barriers_count;

        img_barriers[img_barriers_count].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        img_barriers[img_barriers_count].oldLayout = color_tex.ref->layout;
        img_barriers[img_barriers_count].newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        img_barriers[img_barriers_count].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        img_barriers[img_barriers_count].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        img_barriers[img_barriers_count].image = color_tex.ref->handle().img;
        img_barriers[img_barriers_count].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        img_barriers[img_barriers_count].subresourceRange.baseMipLevel = 0;
        img_barriers[img_barriers_count].subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
        img_barriers[img_barriers_count].subresourceRange.baseArrayLayer = 0;
        img_barriers[img_barriers_count].subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
        img_barriers[img_barriers_count].srcAccessMask = color_tex.ref->last_access_mask;
        img_barriers[img_barriers_count].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        barrier_stages |= color_tex.ref->last_stage_mask;
        ++img_barriers_count;

        img_barriers[img_barriers_count].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        img_barriers[img_barriers_count].oldLayout = spec_tex.ref->layout;
        img_barriers[img_barriers_count].newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        img_barriers[img_barriers_count].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        img_barriers[img_barriers_count].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        img_barriers[img_barriers_count].image = spec_tex.ref->handle().img;
        img_barriers[img_barriers_count].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        img_barriers[img_barriers_count].subresourceRange.baseMipLevel = 0;
        img_barriers[img_barriers_count].subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
        img_barriers[img_barriers_count].subresourceRange.baseArrayLayer = 0;
        img_barriers[img_barriers_count].subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
        img_barriers[img_barriers_count].srcAccessMask = spec_tex.ref->last_access_mask;
        img_barriers[img_barriers_count].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        barrier_stages |= spec_tex.ref->last_stage_mask;
        ++img_barriers_count;

        img_barriers[img_barriers_count].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        img_barriers[img_barriers_count].oldLayout = depth_tex.ref->layout;
        img_barriers[img_barriers_count].newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        img_barriers[img_barriers_count].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        img_barriers[img_barriers_count].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        img_barriers[img_barriers_count].image = depth_tex.ref->handle().img;
        img_barriers[img_barriers_count].subresourceRange.aspectMask =
            VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
        img_barriers[img_barriers_count].subresourceRange.baseMipLevel = 0;
        img_barriers[img_barriers_count].subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
        img_barriers[img_barriers_count].subresourceRange.baseArrayLayer = 0;
        img_barriers[img_barriers_count].subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
        img_barriers[img_barriers_count].srcAccessMask = depth_tex.ref->last_access_mask;
        img_barriers[img_barriers_count].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        barrier_stages |= depth_tex.ref->last_stage_mask;
        ++img_barriers_count;

        vkCmdPipelineBarrier(cmd_buf, barrier_stages,
                             VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
                                 VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                                 VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
                             0, 0, nullptr, 1, &unif_buf_barrier, img_barriers_count, img_barriers);

        unif_shared_data_buf.ref->last_access_mask = VK_ACCESS_SHADER_READ_BIT;
        unif_shared_data_buf.ref->last_stage_mask = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;

        env_->env_map->layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        env_->env_map->last_access_mask = VK_ACCESS_SHADER_READ_BIT;
        env_->env_map->last_stage_mask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;

        color_tex.ref->layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        color_tex.ref->last_access_mask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        color_tex.ref->last_stage_mask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

        spec_tex.ref->layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        spec_tex.ref->last_access_mask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        spec_tex.ref->last_stage_mask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

        depth_tex.ref->layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        depth_tex.ref->last_access_mask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        depth_tex.ref->last_stage_mask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    }

    VkRenderPassBeginInfo render_pass_begin_info = {};
    render_pass_begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    render_pass_begin_info.renderPass = render_pass_;
    render_pass_begin_info.framebuffer = cached_fb_.handle();
    render_pass_begin_info.renderArea = {0, 0, uint32_t(view_state_->act_res[0]), uint32_t(view_state_->act_res[1])};

    vkCmdBeginRenderPass(cmd_buf, &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);

    const VkViewport viewport = {0.0f, 0.0f, float(view_state_->act_res[0]), float(view_state_->act_res[1]),
                                 0.0f, 1.0f};
    vkCmdSetViewport(cmd_buf, 0, 1, &viewport);

    const VkRect2D scissor = {0, 0, uint32_t(view_state_->act_res[0]), uint32_t(view_state_->act_res[1])};
    vkCmdSetScissor(cmd_buf, 0, 1, &scissor);

    vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout_, 0, 1,
                            &desc_set_[api_ctx_->backend_frame], 0, nullptr);

    Ren::Mat4f translate_matrix;
    translate_matrix = Ren::Translate(translate_matrix, draw_cam_pos_);

    Ren::Mat4f scale_matrix;
    scale_matrix = Ren::Scale(scale_matrix, Ren::Vec3f{5000.0f, 5000.0f, 5000.0f});

    const Ren::Mat4f push_constant_data = translate_matrix * scale_matrix;
    vkCmdPushConstants(cmd_buf, pipeline_layout_, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(Ren::Mat4f),
                       &push_constant_data);

    VkBuffer vtx_buf = skydome_mesh_->attribs_buf1_handle().buf;

    VkDeviceSize offset = {};
    vkCmdBindVertexBuffers(cmd_buf, 0, 1, &vtx_buf, &offset);
    vkCmdBindIndexBuffer(cmd_buf, skydome_mesh_->indices_buf_handle().buf, 0, VK_INDEX_TYPE_UINT32);

    vkCmdDrawIndexed(cmd_buf, uint32_t(skydome_mesh_->indices_buf().size / sizeof(uint32_t)), // index count
                     1,                                                                       // instance count
                     uint32_t(skydome_mesh_->indices_buf().offset / sizeof(uint32_t)),        // first index
                     int32_t(skydome_mesh_->attribs_buf1().offset / 16),                      // vertex offset
                     0);                                                                      // first instance

    vkCmdEndRenderPass(cmd_buf);
}

bool RpSkydome::InitPipeline(Ren::Context &ctx, RpAllocTex &color_tex, RpAllocTex &spec_tex, RpAllocTex &depth_tex) {
    Ren::ApiContext *api_ctx = ctx.api_ctx();

    { // create descriptor set layout
        const VkDescriptorSetLayoutBinding layout_bindings[] = {
            // shared data
            {REN_UB_SHARED_DATA_LOC, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1,
             VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
            // environemnt cubemap
            {REN_BASE0_TEX_SLOT, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr}};

        VkDescriptorSetLayoutCreateInfo layout_info = {};
        layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layout_info.bindingCount = 2;
        layout_info.pBindings = layout_bindings;

        const VkResult res = vkCreateDescriptorSetLayout(api_ctx->device, &layout_info, nullptr, &desc_set_layout_);
        if (res != VK_SUCCESS) {
            ctx.log()->Error("Failed to create descriptor set layout!");
            return false;
        }
    }

    { // create descriptor pool
        VkDescriptorPoolSize pool_sizes[2];
        pool_sizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        pool_sizes[0].descriptorCount = 1;

        pool_sizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        pool_sizes[1].descriptorCount = 1;

        VkDescriptorPoolCreateInfo pool_info = {};
        pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pool_info.poolSizeCount = 2;
        pool_info.pPoolSizes = pool_sizes;
        pool_info.maxSets = Ren::MaxFramesInFlight;

        const VkResult res = vkCreateDescriptorPool(api_ctx->device, &pool_info, nullptr, &desc_pool_);
        if (res != VK_SUCCESS) {
            ctx.log()->Error("Failed to create descriptor pool!");
            return false;
        }
    }

    { // create descriptor set
        VkDescriptorSetAllocateInfo alloc_info = {};
        alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        alloc_info.descriptorPool = desc_pool_;
        alloc_info.descriptorSetCount = Ren::MaxFramesInFlight;

        VkDescriptorSetLayout desc_set_layouts[Ren::MaxFramesInFlight];
        for (int i = 0; i < Ren::MaxFramesInFlight; i++) {
            desc_set_layouts[i] = desc_set_layout_;
        }
        alloc_info.pSetLayouts = desc_set_layouts;

        const VkResult res = vkAllocateDescriptorSets(api_ctx->device, &alloc_info, desc_set_);
        if (res != VK_SUCCESS) {
            ctx.log()->Error("Failed to allocate descriptor sets!");
            return false;
        }
    }

    { // create pipeline layout
        VkPipelineLayoutCreateInfo layout_create_info = {};
        layout_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layout_create_info.setLayoutCount = 1;
        layout_create_info.pSetLayouts = &desc_set_layout_;

        VkPushConstantRange pc_ranges[1] = {};
        pc_ranges[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        pc_ranges[0].offset = 0;
        pc_ranges[0].size = sizeof(Ren::Mat4f);

        layout_create_info.pushConstantRangeCount = 1;
        layout_create_info.pPushConstantRanges = pc_ranges;

        const VkResult res = vkCreatePipelineLayout(api_ctx->device, &layout_create_info, nullptr, &pipeline_layout_);
        if (res != VK_SUCCESS) {
            ctx.log()->Error("Failed to create pipeline layout!");
            return false;
        }
    }

    { // create renderpass
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
    }

    { // create graphics pipeline
        VkPipelineShaderStageCreateInfo shader_stage_create_info[2] = {};
        shader_stage_create_info[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shader_stage_create_info[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
        shader_stage_create_info[0].module = skydome_prog_->shader(Ren::eShaderType::Vert)->module();
        shader_stage_create_info[0].pName = "main";
        shader_stage_create_info[0].pSpecializationInfo = nullptr;

        shader_stage_create_info[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shader_stage_create_info[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        shader_stage_create_info[1].module = skydome_prog_->shader(Ren::eShaderType::Frag)->module();
        shader_stage_create_info[1].pName = "main";
        shader_stage_create_info[1].pSpecializationInfo = nullptr;

        const int buf1_stride = 16, buf2_stride = 16;

        VkVertexInputBindingDescription vtx_binding_desc[1] = {};
        vtx_binding_desc[0].binding = 0;
        vtx_binding_desc[0].stride = buf1_stride;
        vtx_binding_desc[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        VkVertexInputAttributeDescription vtx_attrib_desc[1] = {};
        vtx_attrib_desc[0].binding = 0;
        vtx_attrib_desc[0].location = REN_VTX_POS_LOC;
        vtx_attrib_desc[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        vtx_attrib_desc[0].offset = 0;

        VkPipelineVertexInputStateCreateInfo vtx_input_state_create_info = {};
        vtx_input_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vtx_input_state_create_info.vertexBindingDescriptionCount = 1;
        vtx_input_state_create_info.pVertexBindingDescriptions = vtx_binding_desc;
        vtx_input_state_create_info.vertexAttributeDescriptionCount = 1;
        vtx_input_state_create_info.pVertexAttributeDescriptions = vtx_attrib_desc;

        VkPipelineInputAssemblyStateCreateInfo input_assembly_state_create_info = {};
        input_assembly_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        input_assembly_state_create_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        input_assembly_state_create_info.primitiveRestartEnable = VK_FALSE;

        VkViewport viewport = {};
        viewport.x = 0;
        viewport.y = 0;
        viewport.width = float(color_tex.desc.w);
        viewport.height = float(color_tex.desc.h);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;

        VkRect2D scissors = {};
        scissors.offset = {0, 0};
        scissors.extent = VkExtent2D{uint32_t(ctx.w()), uint32_t(ctx.h())};

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
        depth_state_ci.depthWriteEnable = VK_TRUE;
        depth_state_ci.depthCompareOp = VK_COMPARE_OP_ALWAYS;
        depth_state_ci.depthBoundsTestEnable = VK_FALSE;
        depth_state_ci.stencilTestEnable = VK_FALSE;
        depth_state_ci.front = noop_stencil_state;
        depth_state_ci.back = noop_stencil_state;
        depth_state_ci.minDepthBounds = 0.0f;
        depth_state_ci.maxDepthBounds = 1.0f;

        VkPipelineColorBlendAttachmentState color_blend_attachment_states[REN_MAX_ATTACHMENTS] = {};
        for (int i = 0; i < REN_MAX_ATTACHMENTS; ++i) {
            color_blend_attachment_states[i].blendEnable = VK_FALSE;
            color_blend_attachment_states[i].colorWriteMask = 0xf;
        }

        VkPipelineColorBlendStateCreateInfo color_blend_state_ci = {};
        color_blend_state_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        color_blend_state_ci.logicOpEnable = VK_FALSE;
        color_blend_state_ci.logicOp = VK_LOGIC_OP_CLEAR;
        color_blend_state_ci.attachmentCount = REN_MAX_ATTACHMENTS;
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
        pipeline_create_info.layout = pipeline_layout_;
        pipeline_create_info.renderPass = render_pass_;
        pipeline_create_info.subpass = 0;
        pipeline_create_info.basePipelineHandle = VK_NULL_HANDLE;
        pipeline_create_info.basePipelineIndex = 0;

        const VkResult res =
            vkCreateGraphicsPipelines(api_ctx->device, VK_NULL_HANDLE, 1, &pipeline_create_info, nullptr, &pipeline_);
        if (res != VK_SUCCESS) {
            ctx.log()->Error("Failed to create graphics pipeline!");
            return false;
        }
    }

    api_ctx_ = api_ctx;

    return true;
}

RpSkydome::~RpSkydome() {
    if (desc_set_layout_) {
        vkDestroyDescriptorSetLayout(api_ctx_->device, desc_set_layout_, nullptr);
    }
    if (desc_pool_) {
        vkDestroyDescriptorPool(api_ctx_->device, desc_pool_, nullptr);
    }
    if (render_pass_) {
        vkDestroyRenderPass(api_ctx_->device, render_pass_, nullptr);
    }
    if (pipeline_layout_) {
        vkDestroyPipelineLayout(api_ctx_->device, pipeline_layout_, nullptr);
    }
    if (pipeline_) {
        vkDestroyPipeline(api_ctx_->device, pipeline_, nullptr);
    }
}