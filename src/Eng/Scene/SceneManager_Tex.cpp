#include "SceneManager.h"

#include <Ren/Context.h>
#include <Ren/Utils.h>

#include <vtune/ittnotify.h>
extern __itt_domain *__g_itt_domain;

namespace SceneManagerConstants {
#if defined(__ANDROID__)
extern const char *TEXTURES_PATH;
#else
extern const char *TEXTURES_PATH;
#endif

__itt_string_handle *itt_read_file_str = __itt_string_handle_create("ReadFile");
__itt_string_handle *itt_sort_tex_str = __itt_string_handle_create("SortTextures");
} // namespace SceneManagerConstants

namespace SceneManagerInternal {
void ParseDDSHeader(const Ren::DDSHeader &hdr, Ren::Tex2DParams &params, Ren::ILog *log) {
    params.w = uint16_t(hdr.dwWidth);
    params.h = uint16_t(hdr.dwHeight);
    params.mip_count = uint8_t(hdr.dwMipMapCount);

    const int px_format = int(hdr.sPixelFormat.dwFourCC >> 24u) - '0';
    switch (px_format) {
    case 1:
        params.format = Ren::eTexFormat::Compressed_DXT1;
        params.block = Ren::eTexBlock::_4x4;
        break;
    case 3:
        params.format = Ren::eTexFormat::Compressed_DXT3;
        params.block = Ren::eTexBlock::_4x4;
        break;
    case 5:
        params.format = Ren::eTexFormat::Compressed_DXT5;
        params.block = Ren::eTexBlock::_4x4;
        break;
    default:
        log->Error("Unknown DDS pixel format %i", px_format);
        return;
    }
}
} // namespace SceneManagerInternal

void SceneManager::TextureLoaderProc() {
    using namespace SceneManagerConstants;
    using namespace SceneManagerInternal;

    __itt_thread_set_name("Texture loader");

    int iteration = 0;

    const size_t SortPortion = 16;
    const int SortInterval = 8;

    for (;;) {
        TextureRequestPending *req = nullptr;

        {
            std::unique_lock<std::mutex> lock(tex_requests_lock_);
            tex_loader_cnd_.wait(lock, [this, &req] {
                return tex_loader_stop_ || !requested_textures_.empty();
            });
            if (tex_loader_stop_) {
                break;
            }

            for (int i = 0; i < MaxSimultaneousRequests; i++) {
                if (io_pending_tex_[i].state == eRequestState::Idle) {
                    req = &io_pending_tex_[i];
                    break;
                }
            }

            if (!req) {
                continue;
            }

            /**/

            if (iteration++ % SortInterval == 0) {
                __itt_task_begin(__g_itt_domain, __itt_null, __itt_null,
                                 itt_sort_tex_str);
                if (SortPortion != -1) {
                    const size_t sort_portion =
                        std::min(SortPortion, requested_textures_.size());
                    std::partial_sort(
                        std::begin(requested_textures_),
                        std::begin(requested_textures_) + sort_portion,
                        std::end(requested_textures_),
                        [](const TextureRequest &lhs, const TextureRequest &rhs) {
                            return lhs.sort_key < rhs.sort_key;
                        });
                } else {
                    std::sort(std::begin(requested_textures_),
                              std::end(requested_textures_),
                              [](const TextureRequest &lhs, const TextureRequest &rhs) {
                                  return lhs.sort_key < rhs.sort_key;
                              });
                }
                __itt_task_end(__g_itt_domain);
            }

            static_cast<TextureRequest &>(*req) = requested_textures_.front();
            requested_textures_.pop_front();
        }

        __itt_task_begin(__g_itt_domain, __itt_null, __itt_null, itt_read_file_str);

        req->buf->set_data_len(0);
        req->mip_offset_to_init = 0xff;

        size_t read_offset = 0, read_size = 0;

        char path_buf[4096];
        strcpy(path_buf, SceneManagerConstants::TEXTURES_PATH);
        strcat(path_buf, req->ref->name().c_str());

        bool read_success = true;

        if (req->ref->name().EndsWith(".dds") || req->ref->name().EndsWith(".DDS")) {
            if (req->orig_format == Ren::eTexFormat::Undefined) {
                Ren::DDSHeader header;

                size_t data_size = sizeof(Ren::DDSHeader);
                read_success = tex_reader_.ReadFileBlocking(path_buf, 0 /* file_offset */,
                                                            sizeof(Ren::DDSHeader),
                                                            &header, data_size);
                read_success &= (data_size == sizeof(Ren::DDSHeader));

                if (read_success) {
                    Ren::Tex2DParams temp_params;
                    ParseDDSHeader(header, temp_params, ren_ctx_.log());
                    req->orig_format = temp_params.format;
                    req->orig_block = temp_params.block;
                    req->orig_w = temp_params.w;
                    req->orig_h = temp_params.h;
                    req->orig_mip_count = int(header.dwMipMapCount);
                }
            }
            read_offset += sizeof(Ren::DDSHeader);
        } else if (req->ref->name().EndsWith(".ktx") ||
                   req->ref->name().EndsWith(".KTX")) {
            assert(false && "Not implemented!");
            read_offset += sizeof(Ren::KTXHeader);
        }

        if (read_success) {
            const Ren::Tex2DParams &cur_p = req->ref->params();

            const int max_load_w = std::max(cur_p.w * 2, 64);
            const int max_load_h = std::max(cur_p.h * 2, 64);

            int w = int(req->orig_w), h = int(req->orig_h);
            for (int i = 0; i < req->orig_mip_count; i++) {
                const int data_len =
                    Ren::GetMipDataLenBytes(w, h, req->orig_format, req->orig_block);

                if ((w <= max_load_w && h <= max_load_h) ||
                    i == req->orig_mip_count - 1) {
                    if (req->mip_offset_to_init == 0xff) {
                        req->mip_offset_to_init = uint8_t(i);
                        req->mip_count_to_init = 0;
                    }
                    if (w > cur_p.w || h > cur_p.h ||
                        (cur_p.mip_count == 1 && w == cur_p.w && h == cur_p.h)) {
                        req->mip_count_to_init++;
                        read_size += data_len;
                    }
                } else {
                    read_offset += data_len;
                }

                w = std::max(w / 2, 1);
                h = std::max(h / 2, 1);
            }

            // load next mip levels
            assert(req->ref->params().w == req->orig_w ||
                   req->ref->params().h != req->orig_h);

            if (read_size) {
                read_success = tex_reader_.ReadFileNonBlocking(
                    path_buf, read_offset, read_size, *req->buf, req->ev);
                assert(req->buf->data_len() == read_size);
            }
        }

        if (read_success) {
            std::lock_guard<std::mutex> _(tex_requests_lock_);
            req->state = eRequestState::PendingIO;
        }

        __itt_task_end(__g_itt_domain);
    }
}

void SceneManager::ProcessPendingTextures(const int portion_size) {
    using namespace SceneManagerConstants;

    for (int i = 0; i < portion_size; i++) {
        TextureRequestPending *req = nullptr;
        Sys::eFileReadResult res;
        size_t bytes_read = 0;
        {
            std::lock_guard<std::mutex> _(tex_requests_lock_);
            for (int i = 0; i < MaxSimultaneousRequests; i++) {
                if (io_pending_tex_[i].state == eRequestState::PendingIO) {
                    res = io_pending_tex_[i].ev.GetResult(false /* block */, &bytes_read);
                    if (res != Sys::eFileReadResult::Pending) {
                        req = &io_pending_tex_[i];
                        break;
                    }
                } else if (io_pending_tex_[i].state == eRequestState::PendingUpdate) {
                    TextureRequestPending *req = &io_pending_tex_[i];
                    auto *stage_buf = static_cast<TextureUpdateFileBuf *>(req->buf.get());
                    const auto res = stage_buf->fence.ClientWaitSync(0 /* timeout_us */);
                    if (res == Ren::SyncFence::WaitResult::WaitFailed) {
                        ren_ctx_.log()->Error("Waiting on fence failed!");

                        static_cast<TextureRequest &>(*req) = {};
                        req->state = eRequestState::Idle;
                        tex_loader_cnd_.notify_one();
                    } else if (res != Ren::SyncFence::WaitResult::TimeoutExpired) {
                        if (req->ref->params().sampling.min_lod.value() > 0) {
                            const auto it = std::lower_bound(
                                std::begin(lod_transit_textures_),
                                std::end(lod_transit_textures_), req->ref,
                                [](const Ren::Tex2DRef &lhs, const Ren::Tex2DRef &rhs) {
                                    return lhs.index() < rhs.index();
                                });
                            if (it == std::end(lod_transit_textures_) ||
                                it->index() != req->ref.index()) {
                                lod_transit_textures_.insert(it, req->ref);
                            }
                        }

                        if (req->ref->params().w != req->orig_w ||
                            req->ref->params().h != req->orig_h) {
                            // send texture to be processed further (for next mip levels)
                            requested_textures_.push_back(std::move(*req));
                        }

                        req->state = eRequestState::Idle;
                        tex_loader_cnd_.notify_one();
                    }
                }
            }
        }

        if (req) {
            const Ren::String &tex_name = req->ref->name();

            if (res == Sys::eFileReadResult::Successful) {
                assert(dynamic_cast<TextureUpdateFileBuf *>(req->buf.get()));
                auto *stage_buf = static_cast<TextureUpdateFileBuf *>(req->buf.get());

                stage_buf->stage_buf().FlushMapped(0, uint32_t(bytes_read));

                const uint64_t t1_us = Sys::GetTimeUs();

                int w = std::max(int(req->orig_w) >> req->mip_offset_to_init, 1);
                int h = std::max(int(req->orig_h) >> req->mip_offset_to_init, 1);

                uint16_t initialized_mips = req->ref->initialized_mips();
                int last_initialized_mip = 0;
                while (initialized_mips >>= 1) {
                    ++last_initialized_mip;
                }

                const Ren::Tex2DParams &p = req->ref->params();

                req->ref->Realloc(w, h, last_initialized_mip + 1 + req->mip_count_to_init,
                                  1 /* samples */, req->orig_format,
                                  (p.flags & Ren::TexSRGB) != 0, ren_ctx_.log());

                int data_off = int(req->buf->data_off());
                for (int i = int(req->mip_offset_to_init);
                     i < int(req->mip_offset_to_init) + req->mip_count_to_init; i++) {
                    if (data_off >= bytes_read) {
                        ren_ctx_.log()->Error("File %s has not enough data!",
                                              tex_name.c_str());
                        break;
                    }
                    const int data_len =
                        Ren::GetMipDataLenBytes(w, h, req->orig_format, req->orig_block);

                    stage_buf->fence = req->ref->SetSubImage(
                        i - req->mip_offset_to_init, 0, 0, w, h, req->orig_format,
                        stage_buf->stage_buf(), data_off, data_len);

                    data_off += data_len;
                    w = std::max(w / 2, 1);
                    h = std::max(h / 2, 1);
                }

                { // offset min lod to account for newly allocated mip levels
                    Ren::TexSamplingParams cur_sampling = req->ref->params().sampling;
                    cur_sampling.min_lod.set_value(cur_sampling.min_lod.value() +
                                                   cur_sampling.min_lod.One *
                                                       req->mip_count_to_init);
                    req->ref->SetFilter(cur_sampling, ren_ctx_.log());
                }

                const uint64_t t2_us = Sys::GetTimeUs();

                ren_ctx_.log()->Info("Texture %s loaded (%.3f ms)", tex_name.c_str(),
                                     double(t2_us - t1_us) * 0.001);
            } else if (res == Sys::eFileReadResult::Failed) {
                ren_ctx_.log()->Error("Error loading %s", tex_name.c_str());
            }

            if (res != Sys::eFileReadResult::Pending) {
                if (res == Sys::eFileReadResult::Successful) {
                    req->state = eRequestState::PendingUpdate;
                } else {
                    static_cast<TextureRequest &>(*req) = {};

                    std::lock_guard<std::mutex> _(tex_requests_lock_);
                    req->state = eRequestState::Idle;
                    tex_loader_cnd_.notify_one();
                }
            } else {
                break;
            }
        }
    }

    //
    // Animate lod transition to avoid sudden popping
    //
    for (auto it = std::begin(lod_transit_textures_);
         it != std::end(lod_transit_textures_);) {
        Ren::TexSamplingParams cur_sampling = (*it)->params().sampling;
        cur_sampling.min_lod.set_value(std::max(cur_sampling.min_lod.value() - 1, 0));
        (*it)->SetFilter(cur_sampling, ren_ctx_.log());

        if (cur_sampling.min_lod.value() == 0) {
            // transition is done, remove texture from list
            it = lod_transit_textures_.erase(it);
        } else {
            ++it;
        }
    }
}

void SceneManager::UpdateTexturePriorities(const TexEntry visible_textures[],
                                           const int visible_count,
                                           const TexEntry desired_textures[],
                                           const int desired_count) {
    std::unique_lock<std::mutex> lock(tex_requests_lock_);

    bool kick_loader_thread = false;
    for (auto it = requested_textures_.begin(); it != requested_textures_.end(); ++it) {
        const TexEntry *found_entry = nullptr;

        { // search among visible textures first
            const TexEntry *beg = visible_textures;
            const TexEntry *end = visible_textures + visible_count;

            const TexEntry *entry = std::lower_bound(
                beg, end, it->ref.index(),
                [](const TexEntry &t1, const uint32_t t2) { return t1.index < t2; });

            if (entry != end && entry->index == it->ref.index()) {
                found_entry = entry;
            }
        }

        if (!found_entry) { // search among surrounding texture
            const TexEntry *beg = desired_textures;
            const TexEntry *end = desired_textures + desired_count;

            const TexEntry *entry = std::lower_bound(
                beg, end, it->ref.index(),
                [](const TexEntry &t1, const uint32_t t2) { return t1.index < t2; });

            if (entry != end && entry->index == it->ref.index()) {
                found_entry = entry;
            }
        }

        if (found_entry) {
            it->sort_key = found_entry->sort_key;
            kick_loader_thread = true;
        }
    }

    if (kick_loader_thread) {
        tex_loader_cnd_.notify_one();
    }
}

void SceneManager::ForceTextureReload() {
    { // stop texture loading thread
        std::unique_lock<std::mutex> lock(tex_requests_lock_);
        tex_loader_stop_ = true;
        tex_loader_cnd_.notify_one();
    }

    tex_loader_thread_.join();
    for (int i = 0; i < MaxSimultaneousRequests; i++) {
        size_t bytes_read = 0;
        io_pending_tex_[i].ev.GetResult(true /* block */, &bytes_read);
        io_pending_tex_[i].state = eRequestState::Idle;
    }
    requested_textures_.clear();
    lod_transit_textures_.clear();

    // Reset texture to 1x1 mip and send to processing
    for (auto it = std::begin(scene_data_.textures); it != std::end(scene_data_.textures);
         ++it) {
        Ren::Tex2DParams p = (*it).params();

        // drop to lowest lod
        const int w = std::max(p.w >> p.mip_count, 1);
        const int h = std::max(p.h >> p.mip_count, 1);

        (*it).Realloc(w, h, 1 /* mip_count */, 1 /* samples */, p.format,
                      p.flags & Ren::TexSRGB, ren_ctx_.log());

        p.sampling.min_lod.from_float(-1.0f);
        (*it).SetFilter(p.sampling, ren_ctx_.log());

        TextureRequest req;
        req.ref = Ren::Tex2DRef{&scene_data_.textures, it.index()};

        requested_textures_.push_back(std::move(req));
    }

    // start texture loading thread
    tex_loader_stop_ = false;
    tex_loader_thread_ = std::thread(&SceneManager::TextureLoaderProc, this);
}

#undef NEXT_REQ_NDX
#undef PREV_REQ_NDX
