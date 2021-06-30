#include "SceneManager.h"

#include <Ren/Context.h>
#include <Ren/GL.h>
#include <Ren/Utils.h>

#include "../Renderer/Renderer_Structs.h"

#include <vtune/ittnotify.h>
extern __itt_domain *__g_itt_domain;

namespace SceneManagerConstants {} // namespace SceneManagerConstants

namespace SceneManagerInternal {} // namespace SceneManagerInternal

PersistentBuffers SceneManager::persistent_bufs() const {
    PersistentBuffers ret;
    ret.materials_buf = scene_data_.materials_buf->handle();
    ret.textures_buf = scene_data_.textures_buf->handle();
    return ret;
}

void SceneManager::InsertPersistentBuffersFence() {
    if (!scene_data_.mat_buf_fences[scene_data_.mat_buf_index]) {
        scene_data_.mat_buf_fences[scene_data_.mat_buf_index] = Ren::MakeFence();
    }
}

void SceneManager::UpdateMaterialsBuffer() {
    const uint32_t max_mat_count = scene_data_.materials.capacity();
    const uint32_t req_mat_buf_size = std::max(1u, max_mat_count) * sizeof(MaterialData);

    if (!scene_data_.materials_buf) {
        scene_data_.materials_buf = ren_ctx_.CreateBuffer("Materials Buffer", Ren::eBufType::Storage, req_mat_buf_size);
        scene_data_.materials_stage_buf = ren_ctx_.CreateBuffer("Materials Stage Buffer", Ren::eBufType::Stage,
                                                                Ren::MaxFramesInFlight * req_mat_buf_size);
    }

    if (scene_data_.materials_buf->size() < req_mat_buf_size) {
        scene_data_.materials_buf->Resize(req_mat_buf_size);
        scene_data_.materials_stage_buf->Resize(Ren::MaxFramesInFlight * req_mat_buf_size);
    }

    const uint32_t max_tex_count = std::max(1u, REN_MAX_TEX_PER_MATERIAL * max_mat_count);
    const uint32_t req_tex_buf_size = max_tex_count * sizeof(GLuint64);

    if (!scene_data_.textures_buf) {
        scene_data_.textures_buf = ren_ctx_.CreateBuffer("Textures Buffer", Ren::eBufType::Storage, req_tex_buf_size);
        scene_data_.textures_stage_buf = ren_ctx_.CreateBuffer("Textures Stage Buffer", Ren::eBufType::Stage,
                                                               Ren::MaxFramesInFlight * req_tex_buf_size);
    }

    if (scene_data_.textures_buf->size() < req_tex_buf_size) {
        scene_data_.textures_buf->Resize(req_tex_buf_size);
        scene_data_.textures_stage_buf->Resize(Ren::MaxFramesInFlight * req_tex_buf_size);
    }

    // propagate material changes
    for (const uint32_t i : scene_data_.material_changes) {
        for (int j = 0; j < Ren::MaxFramesInFlight; ++j) {
            scene_data_.mat_update_ranges[j].first = std::min(scene_data_.mat_update_ranges[j].first, i);
            scene_data_.mat_update_ranges[j].second = std::max(scene_data_.mat_update_ranges[j].second, i + 1);
        }
    }
    scene_data_.material_changes.clear();

    const uint32_t next_buf_index = (scene_data_.mat_buf_index + 1) % Ren::MaxFramesInFlight;
    auto &cur_update_range = scene_data_.mat_update_ranges[next_buf_index];

    if (cur_update_range.second <= cur_update_range.first) {
        return;
    }

    scene_data_.mat_buf_index = next_buf_index;
    if (scene_data_.mat_buf_fences[scene_data_.mat_buf_index]) {
        const Ren::WaitResult res = scene_data_.mat_buf_fences[scene_data_.mat_buf_index].ClientWaitSync();
        if (res != Ren::WaitResult::Success) {
            ren_ctx_.log()->Error("RpUpdateBuffers: Wait failed!");
        }
        scene_data_.mat_buf_fences[scene_data_.mat_buf_index] = {};
    }

    const size_t TexSizePerMaterial = REN_MAX_TEX_PER_MATERIAL * sizeof(GLuint64);

    const uint32_t mat_buf_offset = scene_data_.mat_buf_index * req_mat_buf_size;
    const uint32_t tex_buf_offset = scene_data_.mat_buf_index * req_tex_buf_size;

    MaterialData *material_data = reinterpret_cast<MaterialData *>(scene_data_.materials_stage_buf->MapRange(
        Ren::BufMapWrite, mat_buf_offset + cur_update_range.first * sizeof(MaterialData),
        (cur_update_range.second - cur_update_range.first) * sizeof(MaterialData)));
    GLuint64 *texture_data = nullptr;
    if (ren_ctx_.capabilities.bindless_texture) {
        texture_data = reinterpret_cast<GLuint64 *>(scene_data_.textures_stage_buf->MapRange(
            Ren::BufMapWrite, tex_buf_offset + cur_update_range.first * TexSizePerMaterial,
            (cur_update_range.second - cur_update_range.first) * TexSizePerMaterial));
    }

    for (uint32_t i = cur_update_range.first; i < cur_update_range.second; ++i) {
        const uint32_t rel_i = i - cur_update_range.first;
        const Ren::Material *mat = scene_data_.materials.GetOrNull(i);
        if (mat) {
            for (int j = 0; j < int(mat->textures.size()); ++j) {
                material_data[rel_i].texture_indices[j] = i * REN_MAX_TEX_PER_MATERIAL + j;
                if (texture_data) {
                    const GLuint64 handle =
                        glGetTextureSamplerHandleARB(mat->textures[j]->id(), mat->samplers[j]->id());
                    if (!glIsTextureHandleResidentARB(handle)) {
                        glMakeTextureHandleResidentARB(handle);
                    }
                    texture_data[rel_i * REN_MAX_TEX_PER_MATERIAL + j] = handle;
                }
            }
            if (!mat->params.empty()) {
                material_data[rel_i].params = mat->params[0];
            }
        }
    }

    if (texture_data) {
        scene_data_.textures_stage_buf->FlushRange(0, (cur_update_range.second - cur_update_range.first) *
                                                          TexSizePerMaterial);
        scene_data_.textures_stage_buf->Unmap();

        glBindBuffer(GL_COPY_READ_BUFFER, scene_data_.textures_stage_buf->id());
        glBindBuffer(GL_COPY_WRITE_BUFFER, scene_data_.textures_buf->id());

        glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER,
                            GLintptr(tex_buf_offset + cur_update_range.first * TexSizePerMaterial) /* readOffset */,
                            GLintptr(cur_update_range.first * TexSizePerMaterial) /* writeOffset */,
                            (cur_update_range.second - cur_update_range.first) * TexSizePerMaterial);

        glBindBuffer(GL_COPY_READ_BUFFER, 0);
        glBindBuffer(GL_COPY_WRITE_BUFFER, 0);
    }

    scene_data_.materials_stage_buf->FlushRange(0, (cur_update_range.second - cur_update_range.first) *
                                                       sizeof(MaterialData));
    scene_data_.materials_stage_buf->Unmap();

    glBindBuffer(GL_COPY_READ_BUFFER, scene_data_.materials_stage_buf->id());
    glBindBuffer(GL_COPY_WRITE_BUFFER, scene_data_.materials_buf->id());

    glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER,
                        GLintptr(mat_buf_offset + cur_update_range.first * sizeof(MaterialData)) /* readOffset */,
                        GLintptr(cur_update_range.first * sizeof(MaterialData)) /* writeOffset */,
                        (cur_update_range.second - cur_update_range.first) * sizeof(MaterialData));

    glBindBuffer(GL_COPY_READ_BUFFER, 0);
    glBindBuffer(GL_COPY_WRITE_BUFFER, 0);

    // reset just updated range
    cur_update_range.first = max_mat_count - 1;
    cur_update_range.second = 0;
}
