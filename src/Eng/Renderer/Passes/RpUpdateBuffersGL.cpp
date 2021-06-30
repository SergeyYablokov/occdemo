#include "RpUpdateBuffers.h"

#include <Ren/Buffer.h>
#include <Ren/Context.h>
#include <Ren/GL.h>
#include <Sys/Time_.h>

#include "../Renderer_Structs.h"

void RpUpdateBuffers::Execute(RpBuilder &builder) {
    Ren::Context &ctx = builder.ctx();

    const GLbitfield BufferRangeMapFlags = GLbitfield(GL_MAP_WRITE_BIT) | GLbitfield(GL_MAP_INVALIDATE_RANGE_BIT) |
                                           GLbitfield(GL_MAP_UNSYNCHRONIZED_BIT) |
                                           GLbitfield(GL_MAP_FLUSH_EXPLICIT_BIT);

    RpAllocBuf &skin_transforms_buf = builder.GetWriteBuffer(skin_transforms_buf_);
    RpAllocBuf &skin_transforms_stage_buf = builder.GetWriteBuffer(skin_transforms_stage_buf_);

    // Update bone transforms buffer
    if (skin_transforms_.count) {
        glBindBuffer(GL_COPY_READ_BUFFER, GLuint(skin_transforms_stage_buf.ref->id()));

        void *pinned_mem = glMapBufferRange(GL_COPY_READ_BUFFER, ctx.backend_frame * SkinTransformsBufChunkSize,
                                            SkinTransformsBufChunkSize, BufferRangeMapFlags);
        const size_t skin_transforms_mem_size = skin_transforms_.count * sizeof(SkinTransform);
        if (pinned_mem) {
            memcpy(pinned_mem, skin_transforms_.data, skin_transforms_mem_size);
            glFlushMappedBufferRange(GL_COPY_READ_BUFFER, 0, skin_transforms_mem_size);
            glUnmapBuffer(GL_COPY_READ_BUFFER);
        } else {
            builder.log()->Error("RpUpdateBuffers: Failed to map skin transforms buffer!");
        }

        glBindBuffer(GL_COPY_WRITE_BUFFER, GLuint(skin_transforms_buf.ref->id()));
        glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER,
                            ctx.backend_frame * SkinTransformsBufChunkSize /* readOffset */, 0 /* writeOffset */,
                            skin_transforms_mem_size);

        glBindBuffer(GL_COPY_READ_BUFFER, 0);
        glBindBuffer(GL_COPY_WRITE_BUFFER, 0);
    }

    RpAllocBuf &shape_keys_buf = builder.GetWriteBuffer(shape_keys_buf_);
    RpAllocBuf &shape_keys_stage_buf = builder.GetWriteBuffer(shape_keys_stage_buf_);

    if (shape_keys_.count) {
        glBindBuffer(GL_COPY_READ_BUFFER, GLuint(shape_keys_stage_buf.ref->id()));

        void *pinned_mem = glMapBufferRange(GL_COPY_READ_BUFFER, ctx.backend_frame * ShapeKeysBufChunkSize,
                                            ShapeKeysBufChunkSize, BufferRangeMapFlags);
        const size_t shape_keys_mem_size = shape_keys_.count * sizeof(ShapeKeyData);
        if (pinned_mem) {
            memcpy(pinned_mem, shape_keys_.data, shape_keys_mem_size);
            glFlushMappedBufferRange(GL_COPY_READ_BUFFER, 0, shape_keys_mem_size);
            glUnmapBuffer(GL_COPY_READ_BUFFER);
        } else {
            builder.log()->Error("RpUpdateBuffers: Failed to map shape keys buffer!");
        }

        glBindBuffer(GL_COPY_WRITE_BUFFER, GLuint(shape_keys_buf.ref->id()));
        glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER,
                            ctx.backend_frame * ShapeKeysBufChunkSize /* readOffset */, 0 /* writeOffset */,
                            shape_keys_mem_size);

        glBindBuffer(GL_COPY_READ_BUFFER, 0);
        glBindBuffer(GL_COPY_WRITE_BUFFER, 0);
    }

    RpAllocBuf &instances_buf = builder.GetWriteBuffer(instances_buf_);
    RpAllocBuf &instances_stage_buf = builder.GetWriteBuffer(instances_stage_buf_);

    if (!instances_buf.tbos[0]) {
        instances_buf.tbos[0] = ctx.CreateTexture1D("Instances TBO", instances_buf.ref, Ren::eTexFormat::RawRGBA32F, 0,
                                                    InstanceDataBufChunkSize);
    }

    // Update instance buffer
    if (instances_.count) {
        glBindBuffer(GL_COPY_READ_BUFFER, GLuint(instances_stage_buf.ref->id()));

        void *pinned_mem = glMapBufferRange(GL_COPY_READ_BUFFER, ctx.backend_frame * InstanceDataBufChunkSize,
                                            InstanceDataBufChunkSize, BufferRangeMapFlags);
        const size_t instance_mem_size = instances_.count * sizeof(InstanceData);
        if (pinned_mem) {
            memcpy(pinned_mem, instances_.data, instance_mem_size);
            glFlushMappedBufferRange(GL_COPY_READ_BUFFER, 0, instance_mem_size);
            glUnmapBuffer(GL_COPY_READ_BUFFER);
        } else {
            builder.log()->Error("RpUpdateBuffers: Failed to map instance buffer!");
        }

        glBindBuffer(GL_COPY_WRITE_BUFFER, GLuint(instances_buf.ref->id()));
        glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER,
                            ctx.backend_frame * InstanceDataBufChunkSize /* readOffset */, 0 /* writeOffset */,
                            instance_mem_size);
        glBindBuffer(GL_COPY_READ_BUFFER, 0);
        glBindBuffer(GL_COPY_WRITE_BUFFER, 0);
    }

    RpAllocBuf &cells_buf = builder.GetWriteBuffer(cells_buf_);
    RpAllocBuf &cells_stage_buf = builder.GetWriteBuffer(cells_stage_buf_);

    if (!cells_buf.tbos[0]) {
        cells_buf.tbos[0] =
            ctx.CreateTexture1D("Cells TBO", cells_buf.ref, Ren::eTexFormat::RawRG32U, 0, CellsBufChunkSize);
    }

    // Update cells buffer
    if (cells_.count) {
        glBindBuffer(GL_COPY_READ_BUFFER, GLuint(cells_stage_buf.ref->id()));

        void *pinned_mem = glMapBufferRange(GL_COPY_READ_BUFFER, ctx.backend_frame * CellsBufChunkSize,
                                            CellsBufChunkSize, BufferRangeMapFlags);
        const size_t cells_mem_size = cells_.count * sizeof(CellData);
        if (pinned_mem) {
            memcpy(pinned_mem, cells_.data, cells_mem_size);
            glFlushMappedBufferRange(GL_COPY_READ_BUFFER, 0, cells_mem_size);
            glUnmapBuffer(GL_COPY_READ_BUFFER);
        } else {
            builder.log()->Error("RpUpdateBuffers: Failed to map cells buffer!");
        }

        glBindBuffer(GL_COPY_WRITE_BUFFER, GLuint(cells_buf.ref->id()));
        glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER,
                            ctx.backend_frame * CellsBufChunkSize /* readOffset */, 0 /* writeOffset */,
                            cells_mem_size);
        glBindBuffer(GL_COPY_READ_BUFFER, 0);
        glBindBuffer(GL_COPY_WRITE_BUFFER, 0);
    }

    RpAllocBuf &lights_buf = builder.GetWriteBuffer(lights_buf_);
    RpAllocBuf &lights_stage_buf = builder.GetWriteBuffer(lights_stage_buf_);

    if (!lights_buf.tbos[0]) { // Create buffer for lights information
        lights_buf.tbos[0] =
            ctx.CreateTexture1D("Lights TBO", lights_buf.ref, Ren::eTexFormat::RawRGBA32F, 0, LightsBufChunkSize);
    }

    // Update lights buffer
    if (light_sources_.count) {
        glBindBuffer(GL_COPY_READ_BUFFER, GLuint(lights_stage_buf.ref->id()));

        void *pinned_mem = glMapBufferRange(GL_COPY_READ_BUFFER, ctx.backend_frame * LightsBufChunkSize,
                                            LightsBufChunkSize, BufferRangeMapFlags);
        const size_t lights_mem_size = light_sources_.count * sizeof(LightSourceItem);
        if (pinned_mem) {
            memcpy(pinned_mem, light_sources_.data, lights_mem_size);
            glFlushMappedBufferRange(GL_COPY_READ_BUFFER, 0, lights_mem_size);
            glUnmapBuffer(GL_COPY_READ_BUFFER);
        } else {
            builder.log()->Error("RpUpdateBuffers: Failed to map lights buffer!");
        }

        glBindBuffer(GL_COPY_WRITE_BUFFER, GLuint(lights_buf.ref->id()));
        glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER,
                            ctx.backend_frame * LightsBufChunkSize /* readOffset */, 0 /* writeOffset */,
                            lights_mem_size);
        glBindBuffer(GL_COPY_READ_BUFFER, 0);
        glBindBuffer(GL_COPY_WRITE_BUFFER, 0);
    }

    RpAllocBuf &decals_buf = builder.GetWriteBuffer(decals_buf_);
    RpAllocBuf &decals_stage_buf = builder.GetWriteBuffer(decals_stage_buf_);

    if (!decals_buf.tbos[0]) {
        decals_buf.tbos[0] =
            ctx.CreateTexture1D("Decals TBO", decals_buf.ref, Ren::eTexFormat::RawRGBA32F, 0, DecalsBufChunkSize);
    }

    // Update decals buffer
    if (decals_.count) {
        glBindBuffer(GL_COPY_READ_BUFFER, GLuint(decals_stage_buf.ref->id()));

        void *pinned_mem = glMapBufferRange(GL_COPY_READ_BUFFER, ctx.backend_frame * DecalsBufChunkSize,
                                            DecalsBufChunkSize, BufferRangeMapFlags);
        const size_t decals_mem_size = decals_.count * sizeof(DecalItem);
        if (pinned_mem) {
            memcpy(pinned_mem, decals_.data, decals_mem_size);
            glFlushMappedBufferRange(GL_COPY_READ_BUFFER, 0, decals_mem_size);
            glUnmapBuffer(GL_COPY_READ_BUFFER);
        } else {
            builder.log()->Error("RpUpdateBuffers: Failed to map decals buffer!");
        }

        glBindBuffer(GL_COPY_WRITE_BUFFER, GLuint(decals_buf.ref->id()));
        glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER,
                            ctx.backend_frame * DecalsBufChunkSize /* readOffset */, 0 /* writeOffset */,
                            decals_mem_size);
        glBindBuffer(GL_COPY_READ_BUFFER, 0);
        glBindBuffer(GL_COPY_WRITE_BUFFER, 0);
    }

    RpAllocBuf &items_buf = builder.GetWriteBuffer(items_buf_);
    RpAllocBuf &items_stage_buf = builder.GetWriteBuffer(items_stage_buf_);

    if (!items_buf.tbos[0]) {
        items_buf.tbos[0] =
            ctx.CreateTexture1D("Items TBO", items_buf.ref, Ren::eTexFormat::RawRG32U, 0, ItemsBufChunkSize);
    }

    // Update items buffer
    if (items_.count) {
        glBindBuffer(GL_COPY_READ_BUFFER, GLuint(items_stage_buf.ref->id()));

        void *pinned_mem = glMapBufferRange(GL_COPY_READ_BUFFER, ctx.backend_frame * ItemsBufChunkSize,
                                            ItemsBufChunkSize, BufferRangeMapFlags);
        const size_t items_mem_size = items_.count * sizeof(ItemData);
        if (pinned_mem) {
            memcpy(pinned_mem, items_.data, items_mem_size);
            glFlushMappedBufferRange(GL_COPY_READ_BUFFER, 0, items_mem_size);
            glUnmapBuffer(GL_COPY_READ_BUFFER);
        } else {
            builder.log()->Error("RpUpdateBuffers: Failed to map items buffer!");
        }

        glBindBuffer(GL_COPY_WRITE_BUFFER, GLuint(items_buf.ref->id()));
        glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER,
                            ctx.backend_frame * ItemsBufChunkSize /* readOffset */, 0 /* writeOffset */,
                            items_mem_size);
        glBindBuffer(GL_COPY_READ_BUFFER, 0);
        glBindBuffer(GL_COPY_WRITE_BUFFER, 0);
    } else {
        glBindBuffer(GL_COPY_READ_BUFFER, GLuint(items_stage_buf.ref->id()));

        void *pinned_mem = glMapBufferRange(GL_COPY_READ_BUFFER, ctx.backend_frame * ItemsBufChunkSize,
                                            ItemsBufChunkSize, BufferRangeMapFlags);
        if (pinned_mem) {
            ItemData dummy = {};
            memcpy(pinned_mem, &dummy, sizeof(ItemData));
            glFlushMappedBufferRange(GL_COPY_READ_BUFFER, 0, sizeof(ItemData));
            glUnmapBuffer(GL_COPY_READ_BUFFER);
        } else {
            builder.log()->Error("RpUpdateBuffers: Failed to map items buffer!");
        }

        glBindBuffer(GL_COPY_WRITE_BUFFER, GLuint(items_buf.ref->id()));
        glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER,
                            ctx.backend_frame * ItemsBufChunkSize /* readOffset */, 0 /* writeOffset */,
                            sizeof(ItemData));
        glBindBuffer(GL_COPY_READ_BUFFER, 0);
        glBindBuffer(GL_COPY_WRITE_BUFFER, 0);
    }

    //
    // Update UBO with data that is shared between passes
    //
    RpAllocBuf &unif_shared_data_buf = builder.GetWriteBuffer(shared_data_buf_);
    RpAllocBuf &unif_shared_data_stage_buf = builder.GetWriteBuffer(shared_data_stage_buf_);

    { // Prepare data that is shared for all instances
        SharedDataBlock shrd_data;

        shrd_data.uViewMatrix = draw_cam_->view_matrix();
        shrd_data.uProjMatrix = draw_cam_->proj_matrix();

        shrd_data.uTaaInfo[0] = draw_cam_->px_offset()[0];
        shrd_data.uTaaInfo[1] = draw_cam_->px_offset()[1];

        shrd_data.uProjMatrix[2][0] += draw_cam_->px_offset()[0];
        shrd_data.uProjMatrix[2][1] += draw_cam_->px_offset()[1];

        shrd_data.uViewProjMatrix = shrd_data.uProjMatrix * shrd_data.uViewMatrix;
        shrd_data.uViewProjPrevMatrix = view_state_->prev_clip_from_world;
        shrd_data.uInvViewMatrix = Ren::Inverse(shrd_data.uViewMatrix);
        shrd_data.uInvProjMatrix = Ren::Inverse(shrd_data.uProjMatrix);
        shrd_data.uInvViewProjMatrix = Ren::Inverse(shrd_data.uViewProjMatrix);
        // delta matrix between current and previous frame
        shrd_data.uDeltaMatrix =
            view_state_->prev_clip_from_view * (view_state_->down_buf_view_from_world * shrd_data.uInvViewMatrix);

        if (shadow_regions_.count) {
            assert(shadow_regions_.count <= REN_MAX_SHADOWMAPS_TOTAL);
            memcpy(&shrd_data.uShadowMapRegions[0], &shadow_regions_.data[0],
                   sizeof(ShadowMapRegion) * shadow_regions_.count);
        }

        shrd_data.uSunDir = Ren::Vec4f{env_->sun_dir[0], env_->sun_dir[1], env_->sun_dir[2], 0.0f};
        shrd_data.uSunCol = Ren::Vec4f{env_->sun_col[0], env_->sun_col[1], env_->sun_col[2], 0.0f};

        // actual resolution and full resolution
        shrd_data.uResAndFRes = Ren::Vec4f{float(view_state_->act_res[0]), float(view_state_->act_res[1]),
                                           float(view_state_->scr_res[0]), float(view_state_->scr_res[1])};

        const float near = draw_cam_->near(), far = draw_cam_->far();
        const float time_s = 0.001f * Sys::GetTimeMs();
        const float transparent_near = near;
        const float transparent_far = 16.0f;
        const int transparent_mode =
#if (REN_OIT_MODE == REN_OIT_MOMENT_BASED)
            (render_flags_ & EnableOIT) ? 2 : 0;
#elif (REN_OIT_MODE == REN_OIT_WEIGHTED_BLENDED)
            (render_flags_ & EnableOIT) ? 1 : 0;
#else
            0;
#endif

        shrd_data.uTranspParamsAndTime =
            Ren::Vec4f{std::log(transparent_near), std::log(transparent_far) - std::log(transparent_near),
                       float(transparent_mode), time_s};
        shrd_data.uClipInfo = Ren::Vec4f{near * far, near, far, std::log2(1.0f + far / near)};

        const Ren::Vec3f &pos = draw_cam_->world_position();
        shrd_data.uCamPosAndGamma = Ren::Vec4f{pos[0], pos[1], pos[2], 2.2f};
        shrd_data.uWindScroll = Ren::Vec4f{env_->curr_wind_scroll_lf[0], env_->curr_wind_scroll_lf[1],
                                           env_->curr_wind_scroll_hf[0], env_->curr_wind_scroll_hf[1]};
        shrd_data.uWindScrollPrev = Ren::Vec4f{env_->prev_wind_scroll_lf[0], env_->prev_wind_scroll_lf[1],
                                               env_->prev_wind_scroll_hf[0], env_->prev_wind_scroll_hf[1]};

        memcpy(&shrd_data.uProbes[0], probes_.data, sizeof(ProbeItem) * probes_.count);
        memcpy(&shrd_data.uEllipsoids[0], ellipsoids_.data, sizeof(EllipsItem) * ellipsoids_.count);

        glBindBuffer(GL_COPY_READ_BUFFER, GLuint(unif_shared_data_stage_buf.ref->id()));
        void *pinned_mem = glMapBufferRange(GL_COPY_READ_BUFFER, ctx.backend_frame * SharedDataBlockSize,
                                            sizeof(SharedDataBlock), BufferRangeMapFlags);
        if (pinned_mem) {
            memcpy(pinned_mem, &shrd_data, sizeof(SharedDataBlock));
            glFlushMappedBufferRange(GL_COPY_READ_BUFFER, 0, sizeof(SharedDataBlock));
            glUnmapBuffer(GL_COPY_READ_BUFFER);
        }

        glBindBuffer(GL_COPY_WRITE_BUFFER, GLuint(unif_shared_data_buf.ref->id()));
        glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER,
                            ctx.backend_frame * SharedDataBlockSize /* readOffset */, 0 /* writeOffset */,
                            SharedDataBlockSize);
        glBindBuffer(GL_COPY_READ_BUFFER, 0);
        glBindBuffer(GL_COPY_WRITE_BUFFER, 0);
    }
}