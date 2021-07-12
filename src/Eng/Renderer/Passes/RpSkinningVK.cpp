#include "RpSkinning.h"

#include <Ren/Buffer.h>
#include <Ren/VKCtx.h>

#include "../DebugMarker.h"
#include "../Renderer_Structs.h"

void RpSkinning::Execute(RpBuilder &builder) {
    LazyInit(builder.ctx(), builder.sh());

    RpAllocBuf &skin_transforms_buf = builder.GetReadBuffer(skin_transforms_buf_);
    RpAllocBuf &shape_keys_buf = builder.GetReadBuffer(shape_keys_buf_);

    if (skin_regions_.count) {
        Ren::ApiContext *api_ctx = builder.ctx().api_ctx();
        VkCommandBuffer cmd_buf = api_ctx->draw_cmd_buf[api_ctx->backend_frame];

        VkPipelineStageFlags barrier_stages = 0;

        VkBufferMemoryBarrier barriers[6] = {};

        barriers[0].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        barriers[0].srcAccessMask = skin_vtx_buf_->last_access_mask;
        barriers[0].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        barriers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barriers[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barriers[0].buffer = skin_vtx_buf_->handle().buf;
        barriers[0].offset = 0;
        barriers[0].size = VK_WHOLE_SIZE;
        barrier_stages |= skin_vtx_buf_->last_stage_mask;

        barriers[1].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        barriers[1].srcAccessMask = skin_transforms_buf.ref->last_access_mask;
        barriers[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        barriers[1].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barriers[1].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barriers[1].buffer = skin_transforms_buf.ref->handle().buf;
        barriers[1].offset = 0;
        barriers[1].size = VK_WHOLE_SIZE;
        barrier_stages |= skin_transforms_buf.ref->last_stage_mask;

        barriers[2].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        barriers[2].srcAccessMask = shape_keys_buf.ref->last_access_mask;
        barriers[2].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        barriers[2].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barriers[2].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barriers[2].buffer = shape_keys_buf.ref->handle().buf;
        barriers[2].offset = 0;
        barriers[2].size = VK_WHOLE_SIZE;
        barrier_stages |= shape_keys_buf.ref->last_stage_mask;

        barriers[3].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        barriers[3].srcAccessMask = delta_buf_->last_access_mask;
        barriers[3].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        barriers[3].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barriers[3].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barriers[3].buffer = delta_buf_->handle().buf;
        barriers[3].offset = 0;
        barriers[3].size = VK_WHOLE_SIZE;
        barrier_stages |= delta_buf_->last_stage_mask;

        barriers[4].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        barriers[4].srcAccessMask = vtx_buf1_->last_access_mask;
        barriers[4].dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        barriers[4].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barriers[4].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barriers[4].buffer = vtx_buf1_->handle().buf;
        barriers[4].offset = 0;
        barriers[4].size = VK_WHOLE_SIZE;
        barrier_stages |= vtx_buf1_->last_stage_mask;

        barriers[5].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        barriers[5].srcAccessMask = vtx_buf2_->last_access_mask;
        barriers[5].dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        barriers[5].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barriers[5].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barriers[5].buffer = vtx_buf2_->handle().buf;
        barriers[5].offset = 0;
        barriers[5].size = VK_WHOLE_SIZE;
        barrier_stages |= vtx_buf2_->last_stage_mask;

        vkCmdPipelineBarrier(cmd_buf, barrier_stages, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 6, barriers,
                             0, nullptr);

        skin_vtx_buf_->last_access_mask = VK_ACCESS_SHADER_READ_BIT;
        skin_transforms_buf.ref->last_access_mask = VK_ACCESS_SHADER_READ_BIT;
        shape_keys_buf.ref->last_access_mask = VK_ACCESS_SHADER_READ_BIT;
        delta_buf_->last_access_mask = VK_ACCESS_SHADER_READ_BIT;
        vtx_buf1_->last_access_mask = VK_ACCESS_SHADER_WRITE_BIT;
        vtx_buf2_->last_access_mask = VK_ACCESS_SHADER_WRITE_BIT;

        skin_vtx_buf_->last_stage_mask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        skin_transforms_buf.ref->last_stage_mask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        shape_keys_buf.ref->last_stage_mask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        delta_buf_->last_stage_mask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        vtx_buf1_->last_stage_mask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        vtx_buf2_->last_stage_mask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;

        {                                                // update descriptor set
            const VkDescriptorBufferInfo buf_infos[6] = {// input vertices binding
                                                         {skin_vtx_buf_->handle().buf, 0, VK_WHOLE_SIZE},
                                                         // input matrices binding
                                                         {skin_transforms_buf.ref->handle().buf, 0, VK_WHOLE_SIZE},
                                                         // input shape keys binding
                                                         {shape_keys_buf.ref->handle().buf, 0, VK_WHOLE_SIZE},
                                                         // input vertex deltas binding
                                                         {delta_buf_->handle().buf, 0, VK_WHOLE_SIZE},
                                                         // output vertices0 binding
                                                         {vtx_buf1_->handle().buf, 0, VK_WHOLE_SIZE},
                                                         // output vertices1 binding
                                                         {vtx_buf2_->handle().buf, 0, VK_WHOLE_SIZE}};

            VkWriteDescriptorSet descr_write;
            descr_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descr_write.dstSet = desc_set_[api_ctx->backend_frame];
            descr_write.dstBinding = 0;
            descr_write.dstArrayElement = 0;
            descr_write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            descr_write.descriptorCount = 6;
            descr_write.pBufferInfo = buf_infos;
            descr_write.pImageInfo = nullptr;
            descr_write.pTexelBufferView = nullptr;
            descr_write.pNext = nullptr;

            vkUpdateDescriptorSets(api_ctx->device, 1, &descr_write, 0, nullptr);
        }

        const int SkinLocalGroupSize = 128;

        vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_);
        vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_layout_, 0, 1,
                                &desc_set_[api_ctx->backend_frame], 0, nullptr);

        for (uint32_t i = 0; i < skin_regions_.count; i++) {
            const SkinRegion &sr = skin_regions_.data[i];

            const uint32_t non_shapekeyed_vertex_count = sr.vertex_count - sr.shape_keyed_vertex_count;

            if (non_shapekeyed_vertex_count) {
                const Ren::Vec4u push_constant_data[3] = {
                    // uSkinParams
                    Ren::Vec4u{sr.in_vtx_offset, non_shapekeyed_vertex_count, sr.xform_offset, sr.out_vtx_offset},
                    // uShapeParamsCurr
                    Ren::Vec4u{0, 0, 0, 0},
                    // uShapeParamsPrev
                    Ren::Vec4u{0, 0, 0, 0}};

                vkCmdPushConstants(cmd_buf, pipeline_layout_, VK_SHADER_STAGE_COMPUTE_BIT, 0, 3 * sizeof(Ren::Vec4u),
                                   push_constant_data);

                vkCmdDispatch(cmd_buf, (sr.vertex_count + SkinLocalGroupSize - 1) / SkinLocalGroupSize, 1, 1);

                // glUniform4ui(0, sr.in_vtx_offset, non_shapekeyed_vertex_count, sr.xform_offset, sr.out_vtx_offset);
                // glUniform4ui(1, 0, 0, 0, 0);
                // glUniform4ui(2, 0, 0, 0, 0);

                // glDispatchCompute((sr.vertex_count + SkinLocalGroupSize - 1) / SkinLocalGroupSize, 1, 1);
            }

            if (sr.shape_keyed_vertex_count) {
                const Ren::Vec4u push_constant_data[3] = {
                    // uSkinParams
                    Ren::Vec4u{sr.in_vtx_offset + non_shapekeyed_vertex_count, sr.shape_keyed_vertex_count,
                               sr.xform_offset, sr.out_vtx_offset + non_shapekeyed_vertex_count},
                    // uShapeParamsCurr
                    Ren::Vec4u{sr.shape_key_offset_curr, sr.shape_key_count_curr, sr.delta_offset, 0},
                    // uShapeParamsPrev
                    Ren::Vec4u{sr.shape_key_offset_prev, sr.shape_key_count_prev, sr.delta_offset, 0}};

                vkCmdPushConstants(cmd_buf, pipeline_layout_, VK_SHADER_STAGE_COMPUTE_BIT, 0, 3 * sizeof(Ren::Vec4u),
                                   push_constant_data);

                vkCmdDispatch(cmd_buf, (sr.shape_keyed_vertex_count + SkinLocalGroupSize - 1) / SkinLocalGroupSize, 1,
                              1);

                // glUniform4ui(0, sr.in_vtx_offset + non_shapekeyed_vertex_count, sr.shape_keyed_vertex_count,
                //             sr.xform_offset, sr.out_vtx_offset + non_shapekeyed_vertex_count);
                // glUniform4ui(1, sr.shape_key_offset_curr, sr.shape_key_count_curr, sr.delta_offset, 0);
                // glUniform4ui(2, sr.shape_key_offset_prev, sr.shape_key_count_prev, sr.delta_offset, 0);

                // glDispatchCompute((sr.shape_keyed_vertex_count + SkinLocalGroupSize - 1) / SkinLocalGroupSize, 1, 1);
            }
        }

        /*const GLuint vertex_buf1_id = vtx_buf1_->id();
        const GLuint vertex_buf2_id = vtx_buf2_->id();
        const GLuint delta_buf_id = delta_buf_->id();
        const GLuint skin_vtx_buf_id = skin_vtx_buf_->id();

        const int SkinLocalGroupSize = 128;
        const Ren::Program *p = skinning_prog_.get();

        glUseProgram(p->id());
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, skin_vtx_buf_id);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, GLuint(skin_transforms_buf.ref->id()));
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, GLuint(shape_keys_buf.ref->id()));
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, delta_buf_id);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, vertex_buf1_id);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, vertex_buf2_id);

        for (uint32_t i = 0; i < skin_regions_.count; i++) {
            const SkinRegion &sr = skin_regions_.data[i];

            const uint32_t non_shapekeyed_vertex_count = sr.vertex_count - sr.shape_keyed_vertex_count;

            if (non_shapekeyed_vertex_count) {
                glUniform4ui(0, sr.in_vtx_offset, non_shapekeyed_vertex_count, sr.xform_offset, sr.out_vtx_offset);
                glUniform4ui(1, 0, 0, 0, 0);
                glUniform4ui(2, 0, 0, 0, 0);

                glDispatchCompute((sr.vertex_count + SkinLocalGroupSize - 1) / SkinLocalGroupSize, 1, 1);
            }

            if (sr.shape_keyed_vertex_count) {
                glUniform4ui(0, sr.in_vtx_offset + non_shapekeyed_vertex_count, sr.shape_keyed_vertex_count,
                             sr.xform_offset, sr.out_vtx_offset + non_shapekeyed_vertex_count);
                glUniform4ui(1, sr.shape_key_offset_curr, sr.shape_key_count_curr, sr.delta_offset, 0);
                glUniform4ui(2, sr.shape_key_offset_prev, sr.shape_key_count_prev, sr.delta_offset, 0);

                glDispatchCompute((sr.shape_keyed_vertex_count + SkinLocalGroupSize - 1) / SkinLocalGroupSize, 1, 1);
            }
        }

        glMemoryBarrier(GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT);*/
    }
}

bool RpSkinning::InitPipeline(Ren::Context &ctx) {
    Ren::ApiContext *api_ctx = ctx.api_ctx();

    { // create descriptor set layout
        const VkDescriptorSetLayoutBinding layout_bindings[6] = {
            // input vertices binding
            {0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            // input matrices binding
            {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            // input shape keys binding
            {2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            // input vertex deltas binding
            {3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            // output vertices0 binding
            {4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            // output vertices1 binding
            {5, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}};

        VkDescriptorSetLayoutCreateInfo layout_info = {};
        layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layout_info.bindingCount = 6;
        layout_info.pBindings = layout_bindings;

        const VkResult res = vkCreateDescriptorSetLayout(api_ctx->device, &layout_info, nullptr, &desc_set_layout_);
        if (res != VK_SUCCESS) {
            ctx.log()->Error("Failed to create descriptor set layout!");
            return false;
        }
    }

    { // create descriptor pool
        VkDescriptorPoolSize pool_size;
        pool_size.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        pool_size.descriptorCount = 6;

        VkDescriptorPoolCreateInfo pool_info = {};
        pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pool_info.poolSizeCount = 1;
        pool_info.pPoolSizes = &pool_size;
        pool_info.maxSets = Ren::MaxFramesInFlight;

        const VkResult res = vkCreateDescriptorPool(api_ctx->device, &pool_info, nullptr, &desc_pool_);
        assert(res == VK_SUCCESS && "Failed to create descriptor pool!");
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
        assert(res == VK_SUCCESS && "Failed to allocate descriptor sets!");
    }

    { // create pipeline layout
        VkPipelineLayoutCreateInfo layout_create_info = {};
        layout_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layout_create_info.setLayoutCount = 1;
        layout_create_info.pSetLayouts = &desc_set_layout_;

        VkPushConstantRange pc_ranges[1] = {};
        pc_ranges[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        pc_ranges[0].offset = 0;
        pc_ranges[0].size = 3 * sizeof(Ren::Vec4u);

        layout_create_info.pushConstantRangeCount = 1;
        layout_create_info.pPushConstantRanges = pc_ranges;

        const VkResult res = vkCreatePipelineLayout(api_ctx->device, &layout_create_info, nullptr, &pipeline_layout_);
        if (res != VK_SUCCESS) {
            ctx.log()->Error("Failed to create pipeline layout!");
            return false;
        }
    }

    { // create pipeline
        VkPipelineShaderStageCreateInfo shader_stage_create_info = {};
        shader_stage_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shader_stage_create_info.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        shader_stage_create_info.module = skinning_prog_->shader(Ren::eShaderType::Comp)->module();
        shader_stage_create_info.pName = "main";
        shader_stage_create_info.pSpecializationInfo = nullptr;

        VkComputePipelineCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        info.stage = shader_stage_create_info;
        info.layout = pipeline_layout_;

        const VkResult res = vkCreateComputePipelines(api_ctx->device, VK_NULL_HANDLE, 1, &info, nullptr, &pipeline_);
        if (res != VK_SUCCESS) {
            ctx.log()->Error("Failed to create pipeline!");
            return false;
        }
    }

    return true;
}

RpSkinning::~RpSkinning() {}