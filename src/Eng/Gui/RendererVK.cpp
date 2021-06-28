#include "Renderer.h"

#include "Utils.h"

#include <cassert>
#include <fstream>

#include <Ren/Context.h>
#include <Ren/VKCtx.h>
#include <Sys/Json.h>

#include "shaders.inl"

namespace UIRendererConstants {

/*inline void BindTexture(int slot, uint32_t tex) {
    glActiveTexture((GLenum)(GL_TEXTURE0 + slot));
    glBindTexture(GL_TEXTURE_2D, (GLuint)tex);
}*/

const int MaxVerticesPerRange = 64 * 1024;
const int MaxIndicesPerRange = 128 * 1024;
} // namespace UIRendererConstants

Gui::Renderer::Renderer(Ren::Context &ctx, const JsObject &config) : ctx_(ctx) {
    using namespace UIRendererConstants;

    const JsString &js_gl_defines = config.at(GL_DEFINES_KEY).as_str();

    /*{ // dump shaders into files
        std::ofstream vs_out("ui.vert.glsl", std::ios::binary);
        vs_out.write(vs_source, strlen(vs_source));

        std::ofstream fs_out("ui.frag.glsl", std::ios::binary);
        fs_out.write(fs_source, strlen(fs_source));
    }*/

    { // Load main shader
        using namespace Ren;

        eShaderLoadStatus sh_status;
        ShaderRef ui_vs_ref =
            ctx_.LoadShaderSPIRV("__ui_vs__", __ui_vert_spirv, __ui_vert_spirv_size, eShaderType::Vert, &sh_status);
        assert(sh_status == eShaderLoadStatus::CreatedFromData || sh_status == eShaderLoadStatus::Found);
        ShaderRef ui_fs_ref =
            ctx_.LoadShaderSPIRV("__ui_fs__", __ui_frag_spirv, __ui_frag_spirv_size, eShaderType::Frag, &sh_status);
        assert(sh_status == eShaderLoadStatus::CreatedFromData || sh_status == eShaderLoadStatus::Found);

        eProgLoadStatus status;
        ui_program_ = ctx_.LoadProgram("__ui_program__", ui_vs_ref, ui_fs_ref, {}, {}, &status);
        assert(status == eProgLoadStatus::CreatedFromData || status == eProgLoadStatus::Found);
    }

    const int instance_index = g_instance_count++;

    char name_buf[32];

    sprintf(name_buf, "UI_VertexBuffer [%i]", instance_index);
    vertex_buf_ =
        ctx_.CreateBuffer(name_buf, Ren::eBufType::VertexAttribs, Ren::eBufAccessType::Draw,
                          Ren::eBufAccessFreq::Dynamic, FrameSyncWindow * MaxVerticesPerRange * sizeof(vertex_t));

    sprintf(name_buf, "UI_VertexStageBuffer [%i]", instance_index);
    vertex_stage_buf_ =
        ctx_.CreateBuffer(name_buf, Ren::eBufType::Stage, Ren::eBufAccessType::Copy, Ren::eBufAccessFreq::Stream,
                          FrameSyncWindow * MaxVerticesPerRange * sizeof(vertex_t));

    sprintf(name_buf, "UI_IndexBuffer [%i]", instance_index);
    index_buf_ =
        ctx_.CreateBuffer(name_buf, Ren::eBufType::VertexIndices, Ren::eBufAccessType::Draw,
                          Ren::eBufAccessFreq::Dynamic, FrameSyncWindow * MaxIndicesPerRange * sizeof(uint16_t));

    sprintf(name_buf, "UI_IndexStageBuffer [%i]", instance_index);
    index_stage_buf_ =
        ctx_.CreateBuffer(name_buf, Ren::eBufType::Stage, Ren::eBufAccessType::Copy, Ren::eBufAccessFreq::Stream,
                          FrameSyncWindow * MaxIndicesPerRange * sizeof(uint16_t));

    vtx_data_ = reinterpret_cast<vertex_t *>(vertex_stage_buf_->Map(Ren::BufMapWrite, true /* persistent */));
    ndx_data_ = reinterpret_cast<uint16_t *>(index_stage_buf_->Map(Ren::BufMapWrite, true /* persistent */));

    for (int i = 0; i < FrameSyncWindow; i++) {
        vertex_count_[i] = 0;
        index_count_[i] = 0;
    }

    Ren::VkContext *vk_ctx = ctx_.vk_ctx();

    { // init command buffers
        VkCommandBufferAllocateInfo alloc_info = {};
        alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        alloc_info.commandPool = vk_ctx->command_pool;
        alloc_info.commandBufferCount = FrameSyncWindow;

        const VkResult res = vkAllocateCommandBuffers(vk_ctx->device, &alloc_info, &cmd_bufs_[0]);
        if (res != VK_SUCCESS) {
            ctx_.log()->Error("Failed to allocate command buffers!");
        }
    }

    for (int i = 0; i < FrameSyncWindow; ++i) {
        VkFenceCreateInfo fence_info = {};
        fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        VkFence new_fence;
        const VkResult res = vkCreateFence(vk_ctx->device, &fence_info, nullptr, &new_fence);
        if (res != VK_SUCCESS) {
            ctx_.log()->Error("Failed to create fence!");
        }

        buf_range_fences_[i] = Ren::SyncFence{vk_ctx->device, new_fence};
    }

#if 0
    { // create descriptor set layout
        VkDescriptorSetLayoutBinding layout_binding;

        // sampler layout binding
        layout_binding.binding = 1;
        layout_binding.descriptorCount = 1;
        layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        layout_binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        layout_binding.pImmutableSamplers = nullptr;

        VkDescriptorSetLayoutCreateInfo layout_info = {};
        layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layout_info.bindingCount = 1;
        layout_info.pBindings = &layout_binding;

        const VkResult res = vkCreateDescriptorSetLayout(vk_ctx->device, &layout_info, nullptr, &desc_set_layout_);
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

        const VkResult res = vkCreateDescriptorPool(vk_ctx->device, &pool_info, nullptr, &descriptor_pool_);
        assert(res == VK_SUCCESS && "Failed to create descriptor pool!");
    }

    { // create descriptor set
        VkDescriptorSetAllocateInfo alloc_info = {};
        alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        alloc_info.descriptorPool = descriptor_pool_;
        alloc_info.descriptorSetCount = 1;
        alloc_info.pSetLayouts = &desc_set_layout_;

        const VkResult res = vkAllocateDescriptorSets(vk_ctx->device, &alloc_info, &desc_set_);
        assert(res == VK_SUCCESS && "Failed to allocate descriptor sets!");
    }

    { // update descriptor set
        VkDescriptorImageInfo img_info = {};
        img_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        img_info.imageView = VK_NULL_HANDLE; // ctx.tex_image_view;
        img_info.sampler = VK_NULL_HANDLE;   // ctx.tex_image_sampler;

        VkWriteDescriptorSet descr_write;
        descr_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descr_write.dstSet = desc_set_;
        descr_write.dstBinding = 1;
        descr_write.dstArrayElement = 0;
        descr_write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descr_write.descriptorCount = 1;
        descr_write.pBufferInfo = nullptr;
        descr_write.pImageInfo = &img_info;
        descr_write.pTexelBufferView = nullptr;
        descr_write.pNext = nullptr;

        vkUpdateDescriptorSets(vk_ctx->device, 1, &descr_write, 0, nullptr);
    }

    { // create pipeline layout
        VkPipelineLayoutCreateInfo layout_create_info = {};
        layout_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layout_create_info.setLayoutCount = 1;
        layout_create_info.pSetLayouts = &desc_set_layout_;

        const VkResult res = vkCreatePipelineLayout(vk_ctx->device, &layout_create_info, nullptr, &pipeline_layout_);
        assert(res == VK_SUCCESS && "Failed to create pipeline layout!");
    }

    { // create renderpass
        VkAttachmentDescription pass_attachment = {};
        pass_attachment.format = vk_ctx->surface_format.format;
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

        const VkResult res = vkCreateRenderPass(vk_ctx->device, &render_pass_create_info, nullptr, &render_pass_);
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
        vtx_attrib_desc[0].format = VK_FORMAT_R32G32B32A32_SFLOAT;
        vtx_attrib_desc[0].offset = 0;

        vtx_attrib_desc[1].binding = 0;
        vtx_attrib_desc[1].location = VTX_COL_LOC;
        vtx_attrib_desc[1].format = VK_FORMAT_R8G8B8A8_UNORM;
        vtx_attrib_desc[1].offset = offsetof(vertex_t, col);

        vtx_attrib_desc[2].binding = 0;
        vtx_attrib_desc[2].location = VTX_UVS_LOC;
        vtx_attrib_desc[2].format = VK_FORMAT_R16G16_UNORM;
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
        rasterization_state_ci.frontFace = VK_FRONT_FACE_CLOCKWISE;
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

        VkDynamicState dynamic_state = VK_DYNAMIC_STATE_VIEWPORT;
        VkPipelineDynamicStateCreateInfo dynamic_state_ci = {};
        dynamic_state_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamic_state_ci.dynamicStateCount = 1;
        dynamic_state_ci.pDynamicStates = &dynamic_state;

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
            vkCreateGraphicsPipelines(vk_ctx->device, VK_NULL_HANDLE, 1, &pipeline_create_info, nullptr, &pipeline_);
        assert(result == VK_SUCCESS && "Failed to create graphics pipeline!");
    }
#endif

#if 0
    const Ren::VtxAttribDesc attribs[] = {
        {vertex_buf_->handle(), VTX_POS_LOC, 3, Ren::eType::Float32, sizeof(vertex_t),
         uintptr_t(offsetof(vertex_t, pos))},
        {vertex_buf_->handle(), VTX_COL_LOC, 4, Ren::eType::Uint8UNorm, sizeof(vertex_t),
         uintptr_t(offsetof(vertex_t, col))},
        {vertex_buf_->handle(), VTX_UVS_LOC, 4, Ren::eType::Uint16UNorm, sizeof(vertex_t),
         uintptr_t(offsetof(vertex_t, uvs))}};

    vao_.Setup(attribs, 3, index_buf_->handle());
#endif

    draw_range_index_ = 0;
    fill_range_index_ = (draw_range_index_ + (FrameSyncWindow - 1)) % FrameSyncWindow;
}

Gui::Renderer::~Renderer() {
    Ren::VkContext *vk_ctx = ctx_.vk_ctx();

    for (int i = 0; i < FrameSyncWindow; i++) {
        if (buf_range_fences_[i]) {
            buf_range_fences_[i].ClientWaitSync();
            buf_range_fences_[i].Reset();
        }
    }
    vkFreeCommandBuffers(vk_ctx->device, vk_ctx->command_pool, FrameSyncWindow, &cmd_bufs_[0]);

#if 0
    for (int i = 0; i < FrameSyncWindow; i++) {
        if (buf_range_fences_[i]) {
            auto sync = reinterpret_cast<GLsync>(buf_range_fences_[i]);
            const GLenum res = glClientWaitSync(sync, 0, 1000000000);
            if (res != GL_ALREADY_SIGNALED && res != GL_CONDITION_SATISFIED) {
                ctx_.log()->Error("[Gui::Renderer::~Renderer]: Wait failed!");
            }
            glDeleteSync(sync);
            buf_range_fences_[i] = nullptr;
        }
    }
#endif
}

void Gui::Renderer::PushClipArea(const Vec2f dims[2]) {
    clip_area_stack_[clip_area_stack_size_][0] = dims[0];
    clip_area_stack_[clip_area_stack_size_][1] = dims[0] + dims[1];
    if (clip_area_stack_size_) {
        clip_area_stack_[clip_area_stack_size_][0] =
            Max(clip_area_stack_[clip_area_stack_size_ - 1][0], clip_area_stack_[clip_area_stack_size_][0]);
        clip_area_stack_[clip_area_stack_size_][1] =
            Min(clip_area_stack_[clip_area_stack_size_ - 1][1], clip_area_stack_[clip_area_stack_size_][1]);
    }
    ++clip_area_stack_size_;
}

void Gui::Renderer::PopClipArea() { --clip_area_stack_size_; }

const Gui::Vec2f *Gui::Renderer::GetClipArea() const {
    if (clip_area_stack_size_) {
        return clip_area_stack_[clip_area_stack_size_ - 1];
    }
    return nullptr;
}

int Gui::Renderer::AcquireVertexData(vertex_t **vertex_data, int *vertex_avail, uint16_t **index_data,
                                     int *index_avail) {
    using namespace UIRendererConstants;

    (*vertex_data) = vtx_data_ + size_t(fill_range_index_) * MaxVerticesPerRange + vertex_count_[fill_range_index_];
    (*vertex_avail) = MaxVerticesPerRange - vertex_count_[fill_range_index_];

    (*index_data) = ndx_data_ + size_t(fill_range_index_) * MaxIndicesPerRange + index_count_[fill_range_index_];
    (*index_avail) = MaxIndicesPerRange - index_count_[fill_range_index_];

    return vertex_count_[fill_range_index_];
}

void Gui::Renderer::SubmitVertexData(const int vertex_count, const int index_count) {
    using namespace UIRendererConstants;

    assert((vertex_count_[fill_range_index_] + vertex_count) <= MaxVerticesPerRange &&
           (index_count_[fill_range_index_] + index_count) <= MaxIndicesPerRange);

    vertex_count_[fill_range_index_] += vertex_count;
    index_count_[fill_range_index_] += index_count;
}

void Gui::Renderer::SwapBuffers() {
    draw_range_index_ = (draw_range_index_ + 1) % FrameSyncWindow;
    fill_range_index_ = (draw_range_index_ + (FrameSyncWindow - 1)) % FrameSyncWindow;
}

void Gui::Renderer::Draw(int w, int h) {
    using namespace UIRendererConstants;

    // glViewport(0, 0, w, h);

    //
    // Synchronize with previous draw
    //
    if (buf_range_fences_[draw_range_index_]) {
        const Ren::WaitResult res = buf_range_fences_[draw_range_index_].ClientWaitSync();
        if (res != Ren::WaitResult::Success) {
            ctx_.log()->Error("[Gui::Renderer::Draw]: Wait failed!");
        }
        if (!buf_range_fences_[draw_range_index_].Reset()) {
            ctx_.log()->Error("[Gui::Renderer::Draw]: Reset failed!");
        }
    }

    Ren::VkContext *vk_ctx = ctx_.vk_ctx();
    VkCommandBuffer cmd_buf = cmd_bufs_[draw_range_index_];

    vkResetCommandBuffer(cmd_buf, 0);

    VkCommandBufferBeginInfo begin_info = {};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(cmd_buf, &begin_info);

    //
    // Update buffers
    //
    if (vertex_count_[draw_range_index_]) {
        const uint32_t data_offset = uint32_t(draw_range_index_) * MaxVerticesPerRange * sizeof(vertex_t);
        const uint32_t data_size = uint32_t{vertex_count_[draw_range_index_] * sizeof(vertex_t)};

        vertex_stage_buf_->FlushRange(data_offset, data_size);

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
            barriers[1].offset = data_offset;
            barriers[1].size = data_size;
            ++barriers_count;
        }

        vkCmdPipelineBarrier(cmd_buf, vertex_stage_buf_->last_stage_mask | vertex_buf_->last_stage_mask,
                             VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, barriers_count, barriers, 0, nullptr);

        VkBufferCopy region_to_copy = {};
        region_to_copy.srcOffset = data_offset;
        region_to_copy.dstOffset = data_offset;
        region_to_copy.size = data_size;

        vkCmdCopyBuffer(cmd_buf, vertex_stage_buf_->handle().buf, vertex_buf_->handle().buf, 1, &region_to_copy);

        vertex_stage_buf_->last_access_mask = VK_ACCESS_TRANSFER_READ_BIT;
        vertex_stage_buf_->last_stage_mask = VK_PIPELINE_STAGE_TRANSFER_BIT;

        vertex_buf_->last_access_mask = VK_ACCESS_TRANSFER_WRITE_BIT;
        vertex_buf_->last_stage_mask = VK_PIPELINE_STAGE_TRANSFER_BIT;
    }

    if (index_count_[draw_range_index_]) {
        const uint32_t data_offset = uint32_t(draw_range_index_) * MaxIndicesPerRange * sizeof(uint16_t);
        const uint32_t data_size = uint32_t{index_count_[draw_range_index_] * sizeof(uint16_t)};

        index_stage_buf_->FlushRange(data_offset, data_size);

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
            barriers[1].offset = data_offset;
            barriers[1].size = data_size;
            ++barriers_count;
        }

        vkCmdPipelineBarrier(cmd_buf, index_stage_buf_->last_stage_mask | index_buf_->last_stage_mask,
                             VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, barriers_count, barriers, 0, nullptr);

        VkBufferCopy region_to_copy = {};
        region_to_copy.srcOffset = data_offset;
        region_to_copy.dstOffset = data_offset;
        region_to_copy.size = data_size;

        vkCmdCopyBuffer(cmd_buf, index_stage_buf_->handle().buf, index_buf_->handle().buf, 1, &region_to_copy);

        index_stage_buf_->last_access_mask = VK_ACCESS_TRANSFER_READ_BIT;
        index_stage_buf_->last_stage_mask = VK_PIPELINE_STAGE_TRANSFER_BIT;

        index_buf_->last_access_mask = VK_ACCESS_TRANSFER_WRITE_BIT;
        index_buf_->last_stage_mask = VK_PIPELINE_STAGE_TRANSFER_BIT;
    }

#if 0
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
    glBindTexture(GL_TEXTURE_2D_ARRAY, (GLuint)ctx_.texture_atlas().tex_id());

    const size_t index_buf_mem_offset =
        draw_range_index_ * MaxIndicesPerRange * sizeof(uint16_t);

    glDrawElementsBaseVertex(
        GL_TRIANGLES, index_count_[draw_range_index_], GL_UNSIGNED_SHORT,
        reinterpret_cast<const GLvoid *>(uintptr_t(index_buf_mem_offset)),
        (draw_range_index_ * MaxVerticesPerRange));

    glBindVertexArray(0);
    glUseProgram(0);
#endif

    vertex_count_[draw_range_index_] = 0;
    index_count_[draw_range_index_] = 0;

    vkEndCommandBuffer(cmd_buf);

    VkSubmitInfo submit_info = {};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &cmd_buf;

    vkQueueSubmit(vk_ctx->graphics_queue, 1, &submit_info, buf_range_fences_[draw_range_index_].fence());
}

void Gui::Renderer::PushImageQuad(const eDrawMode draw_mode, const int tex_layer, const Vec2f pos[2],
                                  const Vec2f uvs_px[2]) {
    const Vec2f uvs_scale = 1.0f / Vec2f{(float)Ren::TextureAtlasWidth, (float)Ren::TextureAtlasHeight};
    Vec4f pos_uvs[2] = {Vec4f{pos[0][0], pos[0][1], uvs_px[0][0] * uvs_scale[0], uvs_px[0][1] * uvs_scale[1]},
                        Vec4f{pos[1][0], pos[1][1], uvs_px[1][0] * uvs_scale[0], uvs_px[1][1] * uvs_scale[1]}};

    vertex_t *vtx_data;
    int vtx_avail;
    uint16_t *ndx_data;
    int ndx_avail;
    int ndx_offset = AcquireVertexData(&vtx_data, &vtx_avail, &ndx_data, &ndx_avail);
    assert(vtx_avail >= 4 && ndx_avail >= 6);

    vertex_t *cur_vtx = vtx_data;
    uint16_t *cur_ndx = ndx_data;

    const uint16_t u16_tex_layer = f32_to_u16((1.0f / 16.0f) * float(tex_layer));

    static const uint16_t u16_draw_mode[] = {0, 32767, 65535};

    if (clip_area_stack_size_ && !ClipQuadToArea(pos_uvs, clip_area_stack_[clip_area_stack_size_ - 1])) {
        return;
    }

    cur_vtx->pos[0] = pos_uvs[0][0];
    cur_vtx->pos[1] = pos_uvs[0][1];
    cur_vtx->pos[2] = 0.0f;
    cur_vtx->col[0] = cur_vtx->col[1] = cur_vtx->col[2] = cur_vtx->col[3] = 255;
    cur_vtx->uvs[0] = f32_to_u16(pos_uvs[0][2]);
    cur_vtx->uvs[1] = f32_to_u16(pos_uvs[0][3]);
    cur_vtx->uvs[2] = u16_tex_layer;
    cur_vtx->uvs[3] = u16_draw_mode[int(draw_mode)];
    ++cur_vtx;

    cur_vtx->pos[0] = pos_uvs[1][0];
    cur_vtx->pos[1] = pos_uvs[0][1];
    cur_vtx->pos[2] = 0.0f;
    cur_vtx->col[0] = cur_vtx->col[1] = cur_vtx->col[2] = cur_vtx->col[3] = 255;
    cur_vtx->uvs[0] = f32_to_u16(pos_uvs[1][2]);
    cur_vtx->uvs[1] = f32_to_u16(pos_uvs[0][3]);
    cur_vtx->uvs[2] = u16_tex_layer;
    cur_vtx->uvs[3] = u16_draw_mode[int(draw_mode)];
    ++cur_vtx;

    cur_vtx->pos[0] = pos_uvs[1][0];
    cur_vtx->pos[1] = pos_uvs[1][1];
    cur_vtx->pos[2] = 0.0f;
    cur_vtx->col[0] = cur_vtx->col[1] = cur_vtx->col[2] = cur_vtx->col[3] = 255;
    cur_vtx->uvs[0] = f32_to_u16(pos_uvs[1][2]);
    cur_vtx->uvs[1] = f32_to_u16(pos_uvs[1][3]);
    cur_vtx->uvs[2] = u16_tex_layer;
    cur_vtx->uvs[3] = u16_draw_mode[int(draw_mode)];
    ++cur_vtx;

    cur_vtx->pos[0] = pos_uvs[0][0];
    cur_vtx->pos[1] = pos_uvs[1][1];
    cur_vtx->pos[2] = 0.0f;
    cur_vtx->col[0] = cur_vtx->col[1] = cur_vtx->col[2] = cur_vtx->col[3] = 255;
    cur_vtx->uvs[0] = f32_to_u16(pos_uvs[0][2]);
    cur_vtx->uvs[1] = f32_to_u16(pos_uvs[1][3]);
    cur_vtx->uvs[2] = u16_tex_layer;
    cur_vtx->uvs[3] = u16_draw_mode[int(draw_mode)];
    ++cur_vtx;

    (*cur_ndx++) = ndx_offset + 0;
    (*cur_ndx++) = ndx_offset + 1;
    (*cur_ndx++) = ndx_offset + 2;

    (*cur_ndx++) = ndx_offset + 0;
    (*cur_ndx++) = ndx_offset + 2;
    (*cur_ndx++) = ndx_offset + 3;

    SubmitVertexData(int(cur_vtx - vtx_data), int(cur_ndx - ndx_data));
}

void Gui::Renderer::PushLine(eDrawMode draw_mode, int tex_layer, const uint8_t color[4], const Vec4f &p0,
                             const Vec4f &p1, const Vec2f &d0, const Vec2f &d1, const Vec4f &thickness) {
    const Vec2f uvs_scale = 1.0f / Vec2f{(float)Ren::TextureAtlasWidth, (float)Ren::TextureAtlasHeight};

    const uint16_t u16_tex_layer = f32_to_u16((1.0f / 16.0f) * float(tex_layer));

    static const uint16_t u16_draw_mode[] = {0, 32767, 65535};

    const Vec4f perp[2] = {Vec4f{thickness} * Vec4f{-d0[1], d0[0], 1.0f, 0.0f},
                           Vec4f{thickness} * Vec4f{-d1[1], d1[0], 1.0f, 0.0f}};

    Vec4f pos_uvs[8] = {p0 - perp[0], p1 - perp[1], p1 + perp[1], p0 + perp[0]};
    int vertex_count = 4;

    for (int i = 0; i < vertex_count; i++) {
        pos_uvs[i][2] *= uvs_scale[0];
        pos_uvs[i][3] *= uvs_scale[1];
    }

    if (clip_area_stack_size_ &&
        !(vertex_count = ClipPolyToArea(pos_uvs, vertex_count, clip_area_stack_[clip_area_stack_size_ - 1]))) {
        return;
    }
    assert(vertex_count < 8);

    vertex_t *vtx_data;
    int vtx_avail;
    uint16_t *ndx_data;
    int ndx_avail;
    int ndx_offset = AcquireVertexData(&vtx_data, &vtx_avail, &ndx_data, &ndx_avail);
    assert(vtx_avail >= vertex_count && ndx_avail >= 3 * (vertex_count - 2));

    vertex_t *cur_vtx = vtx_data;
    uint16_t *cur_ndx = ndx_data;

    for (int i = 0; i < vertex_count; i++) {
        cur_vtx->pos[0] = pos_uvs[i][0];
        cur_vtx->pos[1] = pos_uvs[i][1];
        cur_vtx->pos[2] = 0.0f;
        memcpy(cur_vtx->col, color, 4);
        cur_vtx->uvs[0] = f32_to_u16(pos_uvs[i][2]);
        cur_vtx->uvs[1] = f32_to_u16(pos_uvs[i][3]);
        cur_vtx->uvs[2] = u16_tex_layer;
        cur_vtx->uvs[3] = u16_draw_mode[int(draw_mode)];
        ++cur_vtx;
    }

    for (int i = 0; i < vertex_count - 2; i++) {
        (*cur_ndx++) = ndx_offset + 0;
        (*cur_ndx++) = ndx_offset + i + 1;
        (*cur_ndx++) = ndx_offset + i + 2;
    }

    SubmitVertexData(int(cur_vtx - vtx_data), int(cur_ndx - ndx_data));
}

void Gui::Renderer::PushCurve(eDrawMode draw_mode, int tex_layer, const uint8_t color[4], const Vec4f &p0,
                              const Vec4f &p1, const Vec4f &p2, const Vec4f &p3, const Vec4f &thickness) {
    const float tolerance = 0.000001f;

    const Vec4f p01 = 0.5f * (p0 + p1), p12 = 0.5f * (p1 + p2), p23 = 0.5f * (p2 + p3), p012 = 0.5f * (p01 + p12),
                p123 = 0.5f * (p12 + p23), p0123 = 0.5f * (p012 + p123);

    const Vec2f d = Vec2f{p3} - Vec2f{p0};
    const float d2 = std::abs((p1[0] - p3[0]) * d[1] - (p1[1] - p3[1]) * d[0]),
                d3 = std::abs((p2[0] - p3[0]) * d[1] - (p2[1] - p3[1]) * d[0]);

    if ((d2 + d3) * (d2 + d3) < tolerance * (d[0] * d[0] + d[1] * d[1])) {
        PushLine(draw_mode, tex_layer, color, p0, p3, Normalize(Vec2f{p1 - p0}), Normalize(Vec2f{p3 - p2}), thickness);
    } else {
        PushCurve(draw_mode, tex_layer, color, p0, p01, p012, p0123, thickness);
        PushCurve(draw_mode, tex_layer, color, p0123, p123, p23, p3, thickness);
    }
}

#undef VTX_POS_LOC
#undef VTX_COL_LOC
#undef VTX_UVS_LOC

#undef TEX_ATLAS_SLOT
