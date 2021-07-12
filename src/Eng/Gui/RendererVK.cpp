#include "Renderer.h"

#include "Utils.h"

#include <cassert>
#include <fstream>

#include <Ren/Context.h>
#include <Ren/VKCtx.h>
#include <Sys/Json.h>

#include "shaders.inl"

namespace UIRendererConstants {
const int MaxVerticesPerRange = 64 * 1024;
const int MaxIndicesPerRange = 128 * 1024;
} // namespace UIRendererConstants

Gui::Renderer::Renderer(Ren::Context &ctx, const JsObject &config) : ctx_(ctx) {
    using namespace UIRendererConstants;

    const JsString &js_gl_defines = config.at(GL_DEFINES_KEY).as_str();

#if 0
    { // dump shaders into files
        std::ofstream vs_out("ui.vert.glsl", std::ios::binary);
        vs_out.write(vs_source, strlen(vs_source));

        std::ofstream fs_out("ui.frag.glsl", std::ios::binary);
        fs_out.write(fs_source, strlen(fs_source));
    }

    system("src\\libs\\spirv\\win32\\glslangValidator.exe -V ui.vert.glsl -o ui.vert.spv");
    system("src\\libs\\spirv\\win32\\glslangValidator.exe -V ui.frag.glsl -o ui.frag.spv");
    system("src\\libs\\spirv\\win32\\glslangValidator.exe -G ui.vert.glsl -o ui.vert.spv_ogl");
    system("src\\libs\\spirv\\win32\\glslangValidator.exe -G ui.frag.glsl -o ui.frag.spv_ogl");
    system("bin2c.exe -o temp.h ui.vert.spv ui.frag.spv ui.vert.spv_ogl ui.frag.spv_ogl");
#endif

    { // Load main shader
        using namespace Ren;

        eShaderLoadStatus sh_status;
        ShaderRef ui_vs_ref =
            ctx_.LoadShaderSPIRV("__ui_vs__", ui_vert_spv, ui_vert_spv_size, eShaderType::Vert, &sh_status);
        assert(sh_status == eShaderLoadStatus::CreatedFromData || sh_status == eShaderLoadStatus::Found);
        ShaderRef ui_fs_ref =
            ctx_.LoadShaderSPIRV("__ui_fs__", ui_frag_spv, ui_frag_spv_size, eShaderType::Frag, &sh_status);
        assert(sh_status == eShaderLoadStatus::CreatedFromData || sh_status == eShaderLoadStatus::Found);

        eProgLoadStatus status;
        ui_program_ = ctx_.LoadProgram("__ui_program__", ui_vs_ref, ui_fs_ref, {}, {}, &status);
        assert(status == eProgLoadStatus::CreatedFromData || status == eProgLoadStatus::Found);
    }

    const int instance_index = g_instance_count++;

    char name_buf[32];

    sprintf(name_buf, "UI_VertexBuffer [%i]", instance_index);
    vertex_buf_ = ctx_.CreateBuffer(name_buf, Ren::eBufType::VertexAttribs, MaxVerticesPerRange * sizeof(vertex_t));

    sprintf(name_buf, "UI_VertexStageBuffer [%i]", instance_index);
    vertex_stage_buf_ = ctx_.CreateBuffer(name_buf, Ren::eBufType::Stage,
                                          Ren::MaxFramesInFlight * MaxVerticesPerRange * sizeof(vertex_t));

    sprintf(name_buf, "UI_IndexBuffer [%i]", instance_index);
    index_buf_ = ctx_.CreateBuffer(name_buf, Ren::eBufType::VertexIndices, MaxIndicesPerRange * sizeof(uint16_t));

    sprintf(name_buf, "UI_IndexStageBuffer [%i]", instance_index);
    index_stage_buf_ = ctx_.CreateBuffer(name_buf, Ren::eBufType::Stage,
                                         Ren::MaxFramesInFlight * MaxIndicesPerRange * sizeof(uint16_t));

    vtx_stage_data_ = reinterpret_cast<vertex_t *>(vertex_stage_buf_->Map(Ren::BufMapWrite, true /* persistent */));
    ndx_stage_data_ = reinterpret_cast<uint16_t *>(index_stage_buf_->Map(Ren::BufMapWrite, true /* persistent */));

    for (int i = 0; i < Ren::MaxFramesInFlight; i++) {
        vtx_count_[i] = 0;
        ndx_count_[i] = 0;
    }

    Ren::ApiContext *api_ctx = ctx_.api_ctx();

    for (int i = 0; i < Ren::MaxFramesInFlight; ++i) {
        VkFenceCreateInfo fence_info = {};
        fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        VkFence new_fence;
        const VkResult res = vkCreateFence(api_ctx->device, &fence_info, nullptr, &new_fence);
        if (res != VK_SUCCESS) {
            ctx_.log()->Error("Failed to create fence!");
        }

        buf_range_fences_[i] = Ren::SyncFence{api_ctx->device, new_fence};
    }

    { // create descriptor set layout
        const VkDescriptorSetLayoutBinding layout_bindings[] = {
            // texture atlas
            {TEX_ATLAS_SLOT, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr}};

        VkDescriptorSetLayoutCreateInfo layout_info = {};
        layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layout_info.bindingCount = 1;
        layout_info.pBindings = layout_bindings;

        const VkResult res = vkCreateDescriptorSetLayout(api_ctx->device, &layout_info, nullptr, &desc_set_layout_);
        assert(res == VK_SUCCESS && "Failed to create descriptor set layout!");
    }

    { // create descriptor pool
        VkDescriptorPoolSize pool_size;
        pool_size.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        pool_size.descriptorCount = 1;

        VkDescriptorPoolCreateInfo pool_info = {};
        pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pool_info.poolSizeCount = 1;
        pool_info.pPoolSizes = &pool_size;
        pool_info.maxSets = 1;

        const VkResult res = vkCreateDescriptorPool(api_ctx->device, &pool_info, nullptr, &desc_pool_);
        assert(res == VK_SUCCESS && "Failed to create descriptor pool!");
    }

    { // create descriptor set
        VkDescriptorSetAllocateInfo alloc_info = {};
        alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        alloc_info.descriptorPool = desc_pool_;
        alloc_info.descriptorSetCount = 1;
        alloc_info.pSetLayouts = &desc_set_layout_;

        const VkResult res = vkAllocateDescriptorSets(api_ctx->device, &alloc_info, &desc_set_);
        assert(res == VK_SUCCESS && "Failed to allocate descriptor sets!");
    }

    { // update descriptor set
        VkDescriptorImageInfo img_info = {};
        img_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        img_info.imageView = ctx.texture_atlas().img_view();
        img_info.sampler = ctx.texture_atlas().sampler().handle();

        VkWriteDescriptorSet descr_write;
        descr_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descr_write.dstSet = desc_set_;
        descr_write.dstBinding = TEX_ATLAS_SLOT;
        descr_write.dstArrayElement = 0;
        descr_write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descr_write.descriptorCount = 1;
        descr_write.pBufferInfo = nullptr;
        descr_write.pImageInfo = &img_info;
        descr_write.pTexelBufferView = nullptr;
        descr_write.pNext = nullptr;

        vkUpdateDescriptorSets(api_ctx->device, 1, &descr_write, 0, nullptr);
    }

    { // create pipeline layout
        VkPipelineLayoutCreateInfo layout_create_info = {};
        layout_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layout_create_info.setLayoutCount = 1;
        layout_create_info.pSetLayouts = &desc_set_layout_;

        const VkResult res = vkCreatePipelineLayout(api_ctx->device, &layout_create_info, nullptr, &pipeline_layout_);
        assert(res == VK_SUCCESS && "Failed to create pipeline layout!");
    }

    { // create renderpass
        VkAttachmentDescription pass_attachment = {};
        pass_attachment.format = api_ctx->surface_format.format;
        pass_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
        pass_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
        pass_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        pass_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        pass_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        pass_attachment.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        pass_attachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkAttachmentReference color_attach_ref = {};
        color_attach_ref.attachment = 0;
        color_attach_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass = {};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &color_attach_ref;

        VkRenderPassCreateInfo render_pass_create_info = {};
        render_pass_create_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        render_pass_create_info.attachmentCount = 1;
        render_pass_create_info.pAttachments = &pass_attachment;
        render_pass_create_info.subpassCount = 1;
        render_pass_create_info.pSubpasses = &subpass;

        const VkResult res = vkCreateRenderPass(api_ctx->device, &render_pass_create_info, nullptr, &render_pass_);
        assert(res == VK_SUCCESS && "Failed to create render pass!");
    }

    { // create graphics pipeline
        VkPipelineShaderStageCreateInfo shader_stage_create_info[2] = {};
        shader_stage_create_info[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shader_stage_create_info[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
        shader_stage_create_info[0].module = ui_program_->shader(Ren::eShaderType::Vert)->module();
        shader_stage_create_info[0].pName = "main";
        shader_stage_create_info[0].pSpecializationInfo = nullptr;

        shader_stage_create_info[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shader_stage_create_info[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        shader_stage_create_info[1].module = ui_program_->shader(Ren::eShaderType::Frag)->module();
        shader_stage_create_info[1].pName = "main";
        shader_stage_create_info[1].pSpecializationInfo = nullptr;

        VkVertexInputBindingDescription vtx_binding_desc[1] = {};
        vtx_binding_desc[0].binding = 0;
        vtx_binding_desc[0].stride = sizeof(vertex_t);
        vtx_binding_desc[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        VkVertexInputAttributeDescription vtx_attrib_desc[3] = {};
        vtx_attrib_desc[0].binding = 0;
        vtx_attrib_desc[0].location = VTX_POS_LOC;
        vtx_attrib_desc[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        vtx_attrib_desc[0].offset = 0;

        vtx_attrib_desc[1].binding = 0;
        vtx_attrib_desc[1].location = VTX_COL_LOC;
        vtx_attrib_desc[1].format = VK_FORMAT_R8G8B8A8_UNORM;
        vtx_attrib_desc[1].offset = offsetof(vertex_t, col);

        vtx_attrib_desc[2].binding = 0;
        vtx_attrib_desc[2].location = VTX_UVS_LOC;
        vtx_attrib_desc[2].format = VK_FORMAT_R16G16B16A16_UNORM;
        vtx_attrib_desc[2].offset = offsetof(vertex_t, uvs);

        VkPipelineVertexInputStateCreateInfo vtx_input_state_create_info = {};
        vtx_input_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vtx_input_state_create_info.vertexBindingDescriptionCount = 1;
        vtx_input_state_create_info.pVertexBindingDescriptions = vtx_binding_desc;
        vtx_input_state_create_info.vertexAttributeDescriptionCount = 3;
        vtx_input_state_create_info.pVertexAttributeDescriptions = vtx_attrib_desc;

        VkPipelineInputAssemblyStateCreateInfo input_assembly_state_create_info = {};
        input_assembly_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        input_assembly_state_create_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        input_assembly_state_create_info.primitiveRestartEnable = VK_FALSE;

        VkViewport viewport = {};
        viewport.x = 0;
        viewport.y = 0;
        viewport.width = float(ctx.w());
        viewport.height = float(ctx.h());
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
        depth_state_ci.depthWriteEnable = VK_FALSE;
        depth_state_ci.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
        depth_state_ci.depthBoundsTestEnable = VK_FALSE;
        depth_state_ci.stencilTestEnable = VK_FALSE;
        depth_state_ci.front = noop_stencil_state;
        depth_state_ci.back = noop_stencil_state;
        depth_state_ci.minDepthBounds = 0.0f;
        depth_state_ci.maxDepthBounds = 1.0f;

        VkPipelineColorBlendAttachmentState color_blend_attachment_state = {};
        color_blend_attachment_state.blendEnable = VK_TRUE;
        color_blend_attachment_state.colorBlendOp = VK_BLEND_OP_ADD;
        color_blend_attachment_state.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        color_blend_attachment_state.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        color_blend_attachment_state.alphaBlendOp = VK_BLEND_OP_ADD;
        color_blend_attachment_state.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        color_blend_attachment_state.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        color_blend_attachment_state.colorWriteMask = 0xf;

        VkPipelineColorBlendStateCreateInfo color_blend_state_ci = {};
        color_blend_state_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        color_blend_state_ci.logicOpEnable = VK_FALSE;
        color_blend_state_ci.logicOp = VK_LOGIC_OP_CLEAR;
        color_blend_state_ci.attachmentCount = 1;
        color_blend_state_ci.pAttachments = &color_blend_attachment_state;
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

        const VkResult result =
            vkCreateGraphicsPipelines(api_ctx->device, VK_NULL_HANDLE, 1, &pipeline_create_info, nullptr, &pipeline_);
        assert(result == VK_SUCCESS && "Failed to create graphics pipeline!");
    }
}

Gui::Renderer::~Renderer() {
    Ren::ApiContext *api_ctx = ctx_.api_ctx();

    vkDeviceWaitIdle(api_ctx->device);

    vertex_stage_buf_->Unmap();
    index_stage_buf_->Unmap();

    vkDestroyDescriptorSetLayout(api_ctx->device, desc_set_layout_, nullptr);
    vkDestroyDescriptorPool(api_ctx->device, desc_pool_, nullptr);

    vkDestroyRenderPass(api_ctx->device, render_pass_, nullptr);
    vkDestroyPipelineLayout(api_ctx->device, pipeline_layout_, nullptr);
    vkDestroyPipeline(api_ctx->device, pipeline_, nullptr);
}

void Gui::Renderer::Draw(const int w, const int h) {
    using namespace UIRendererConstants;

    Ren::ApiContext *api_ctx = ctx_.api_ctx();
    VkCommandBuffer cmd_buf = api_ctx->draw_cmd_buf[api_ctx->backend_frame];

    //
    // Update buffers
    //
    if (vtx_count_[api_ctx->backend_frame]) {
        const uint32_t data_offset = uint32_t(api_ctx->backend_frame) * MaxVerticesPerRange * sizeof(vertex_t);
        const uint32_t data_size = uint32_t(vtx_count_[api_ctx->backend_frame]) * sizeof(vertex_t);

        vertex_stage_buf_->FlushMappedRange(data_offset, data_size);

        int barriers_count = 0;
        VkBufferMemoryBarrier barriers[2] = {};
        barriers[0].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        barriers[0].srcAccessMask = vertex_stage_buf_->last_access_mask;
        barriers[0].dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        barriers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barriers[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barriers[0].buffer = vertex_stage_buf_->handle().buf;
        barriers[0].offset = data_offset;
        barriers[0].size = data_size;
        ++barriers_count;

        if (vertex_buf_->last_access_mask) {
            barriers[1].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
            barriers[1].srcAccessMask = vertex_buf_->last_access_mask;
            barriers[1].dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barriers[1].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barriers[1].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barriers[1].buffer = vertex_buf_->handle().buf;
            barriers[1].offset = 0;
            barriers[1].size = data_size;
            ++barriers_count;
        }

        vkCmdPipelineBarrier(cmd_buf, vertex_stage_buf_->last_stage_mask | vertex_buf_->last_stage_mask,
                             VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, barriers_count, barriers, 0, nullptr);

        VkBufferCopy region_to_copy = {};
        region_to_copy.srcOffset = data_offset;
        region_to_copy.dstOffset = 0;
        region_to_copy.size = data_size;

        vkCmdCopyBuffer(cmd_buf, vertex_stage_buf_->handle().buf, vertex_buf_->handle().buf, 1, &region_to_copy);

        vertex_stage_buf_->last_access_mask = VK_ACCESS_TRANSFER_READ_BIT;
        vertex_stage_buf_->last_stage_mask = VK_PIPELINE_STAGE_TRANSFER_BIT;

        vertex_buf_->last_access_mask = VK_ACCESS_TRANSFER_WRITE_BIT;
        vertex_buf_->last_stage_mask = VK_PIPELINE_STAGE_TRANSFER_BIT;
    }

    if (ndx_count_[api_ctx->backend_frame]) {
        const uint32_t data_offset = uint32_t(api_ctx->backend_frame) * MaxIndicesPerRange * sizeof(uint16_t);
        const uint32_t data_size = uint32_t(ndx_count_[api_ctx->backend_frame]) * sizeof(uint16_t);

        index_stage_buf_->FlushMappedRange(data_offset, data_size);

        int barriers_count = 0;
        VkBufferMemoryBarrier barriers[2] = {};
        barriers[0].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        barriers[0].srcAccessMask = index_stage_buf_->last_access_mask;
        barriers[0].dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        barriers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barriers[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barriers[0].buffer = index_stage_buf_->handle().buf;
        barriers[0].offset = data_offset;
        barriers[0].size = data_size;
        ++barriers_count;

        if (index_buf_->last_access_mask) {
            barriers[1].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
            barriers[1].srcAccessMask = index_buf_->last_access_mask;
            barriers[1].dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barriers[1].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barriers[1].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barriers[1].buffer = index_buf_->handle().buf;
            barriers[1].offset = 0;
            barriers[1].size = data_size;
            ++barriers_count;
        }

        vkCmdPipelineBarrier(cmd_buf, index_stage_buf_->last_stage_mask | index_buf_->last_stage_mask,
                             VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, barriers_count, barriers, 0, nullptr);

        VkBufferCopy region_to_copy = {};
        region_to_copy.srcOffset = data_offset;
        region_to_copy.dstOffset = 0;
        region_to_copy.size = data_size;

        vkCmdCopyBuffer(cmd_buf, index_stage_buf_->handle().buf, index_buf_->handle().buf, 1, &region_to_copy);

        index_stage_buf_->last_access_mask = VK_ACCESS_TRANSFER_READ_BIT;
        index_stage_buf_->last_stage_mask = VK_PIPELINE_STAGE_TRANSFER_BIT;

        index_buf_->last_access_mask = VK_ACCESS_TRANSFER_WRITE_BIT;
        index_buf_->last_stage_mask = VK_PIPELINE_STAGE_TRANSFER_BIT;
    }

    auto &atlas = ctx_.texture_atlas();

    //
    // Insert layout transition (if necessary)
    //
    if (atlas.layout != VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        VkImageMemoryBarrier barrier = {};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = atlas.layout;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = atlas.img();
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = atlas.mip_count();
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = atlas.layer_count();

        barrier.srcAccessMask = atlas.last_access_mask;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        vkCmdPipelineBarrier(cmd_buf, atlas.last_stage_mask, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0,
                             nullptr, 1, &barrier);

        atlas.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        atlas.last_access_mask = atlas.last_stage_mask = 0;
    }

    atlas.last_access_mask |= VK_ACCESS_SHADER_READ_BIT;
    atlas.last_stage_mask |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;

    //
    // (Re)create framebuffer
    //

    Ren::TexHandle color_attachment;
    color_attachment.view = api_ctx->present_image_views[api_ctx->active_present_image];

    if (!framebuffers_[api_ctx->backend_frame].Setup(api_ctx, render_pass_, w, h, color_attachment, {}, {}, false)) {
        ctx_.log()->Error("Failed to create framebuffer!");
    }

    //
    // Submit draw call
    //

    VkRenderPassBeginInfo render_pass_begin_info = {};
    render_pass_begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    render_pass_begin_info.renderPass = render_pass_;
    render_pass_begin_info.framebuffer = framebuffers_[api_ctx->backend_frame].handle();
    render_pass_begin_info.renderArea = {0, 0, uint32_t(w), uint32_t(h)};

    vkCmdBeginRenderPass(cmd_buf, &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);

    const VkViewport viewport = {0.0f, 0.0f, float(w), float(h), 0.0f, 1.0f};
    vkCmdSetViewport(cmd_buf, 0, 1, &viewport);

    const VkRect2D scissor = {0, 0, uint32_t(w), uint32_t(h)};
    vkCmdSetScissor(cmd_buf, 0, 1, &scissor);

    vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout_, 0, 1, &desc_set_, 0, nullptr);

    VkBuffer vtx_buf = vertex_buf_->vk_handle();

    VkDeviceSize offset = {};
    vkCmdBindVertexBuffers(cmd_buf, 0, 1, &vtx_buf, &offset);
    vkCmdBindIndexBuffer(cmd_buf, index_buf_->vk_handle(), 0, VK_INDEX_TYPE_UINT16);

    vkCmdDrawIndexed(cmd_buf,
                     ndx_count_[api_ctx->backend_frame], // index count
                     1,                                  // instance count
                     0,                                  // first index
                     0,                                  // vertex offset
                     0);                                 // first instance

    vkCmdEndRenderPass(cmd_buf);

    vtx_count_[api_ctx->backend_frame] = 0;
    ndx_count_[api_ctx->backend_frame] = 0;
}

#undef VTX_POS_LOC
#undef VTX_COL_LOC
#undef VTX_UVS_LOC

#undef TEX_ATLAS_SLOT
