#pragma once

#include <Ren/Texture.h>
#include <Sys/AsyncFileReader.h>

class TextureUpdateFileBuf : public Sys::FileReadBufBase {
    Ren::Buffer stage_buf_;

  public:
    TextureUpdateFileBuf(Ren::ApiContext *api_ctx) : stage_buf_("Tex Stage Buf", api_ctx, Ren::eBufType::Stage, 0) {
        Realloc(24 * 1024 * 1024);
    }
    ~TextureUpdateFileBuf() override { Free(); }

    Ren::Buffer &stage_buf() { return stage_buf_; }

    uint8_t *Alloc(const size_t new_size) override {
        stage_buf_.Resize(uint32_t(new_size));
        return stage_buf_.Map(Ren::BufMapWrite, true /* persistent */);
    }

    void Free() override {
        mem_ = nullptr;
        if (stage_buf_.is_mapped()) {
            stage_buf_.Unmap();
        }
        stage_buf_.Free();
    }

    Ren::SyncFence fence;
};