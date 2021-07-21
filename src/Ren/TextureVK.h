#pragma once

#include <cstdint>
#include <cstring>

#include "Buffer.h"
#include "MemoryAllocator.h"
#include "TextureParams.h"

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4996)
#endif

namespace Ren {
class ILog;
class MemoryAllocators;

enum eTexFlags {
    TexNoOwnership = (1u << 0u),
    TexMutable = (1u << 1u),
    TexSigned = (1u << 2u),
    TexSRGB = (1u << 3u),
    TexNoRepeat = (1u << 4u),
    TexMIPMin = (1u << 5u),
    TexMIPMax = (1u << 6u),
    TexNoBias = (1u << 7u),
    TexUsageScene = (1u << 8u),
    TexUsageUI = (1u << 9u)
};

struct TexHandle {
    VkImage img = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
    VkSampler sampler = VK_NULL_HANDLE;
    uint32_t generation = 0; // used to identify unique texture (name can be reused)

    TexHandle() = default;
    TexHandle(VkImage _img, VkImageView _view, VkSampler _sampler, uint32_t _generation)
        : img(_img), view(_view), sampler(_sampler), generation(_generation) {}

    explicit operator bool() const { return img != VK_NULL_HANDLE; }
};
inline bool operator==(const TexHandle lhs, const TexHandle rhs) {
    return lhs.img == rhs.img && lhs.view == rhs.view && lhs.sampler == rhs.sampler && lhs.generation == rhs.generation;
}
inline bool operator!=(const TexHandle lhs, const TexHandle rhs) { return !operator==(lhs, rhs); }
inline bool operator<(const TexHandle lhs, const TexHandle rhs) {
    if (lhs.img < rhs.img) {
        return true;
    } else if (lhs.img == rhs.img) {
        if (lhs.view < rhs.view) {
            return true;
        } else if (lhs.view == rhs.view) {
            return lhs.generation < rhs.generation;
        }
    }
    return false;
}

class TextureStageBuf;

class Texture2D : public RefCounter {
    ApiContext *api_ctx_ = nullptr;
    TexHandle handle_;
    MemAllocation alloc_;
    Tex2DParams params_;
    uint16_t initialized_mips_ = 0;
    bool ready_ = false;
    uint32_t cubemap_ready_ = 0;
    String name_;

    void Free();

    void InitFromRAWData(Buffer *sbuf, int data_off, void *_cmd_buf, MemoryAllocators *mem_allocs, const Tex2DParams &p,
                         ILog *log);
    void InitFromTGAFile(const void *data, Buffer &stage_buf, void *_cmd_buf, MemoryAllocators *mem_allocs,
                         const Tex2DParams &p, ILog *log);
    void InitFromTGA_RGBEFile(const void *data, Buffer &stage_buf, void *_cmd_buf, MemoryAllocators *mem_allocs,
                              const Tex2DParams &p, ILog *log);
    void InitFromDDSFile(const void *data, int size, Buffer &sbuf, void *_cmd_buf, MemoryAllocators *mem_allocs,
                         const Tex2DParams &p, ILog *log);
    void InitFromPNGFile(const void *data, int size, Buffer &sbuf, void *_cmd_buf, MemoryAllocators *mem_allocs,
                         const Tex2DParams &p, ILog *log);
    void InitFromKTXFile(const void *data, int size, Buffer &sbuf, void *_cmd_buf, MemoryAllocators *mem_allocs,
                         const Tex2DParams &p, ILog *log);

    void InitFromRAWData(Buffer &sbuf, int data_off[6], void *_cmd_buf, MemoryAllocators *mem_allocs,
                         const Tex2DParams &p, ILog *log);
    void InitFromTGAFile(const void *data[6], Buffer &sbuf, void *_cmd_buf, MemoryAllocators *mem_allocs,
                         const Tex2DParams &p, ILog *log);
    void InitFromTGA_RGBEFile(const void *data[6], Buffer &sbuf, void *_cmd_buf, MemoryAllocators *mem_allocs,
                              const Tex2DParams &p, ILog *log);
    void InitFromPNGFile(const void *data[6], const int size[6], Buffer &sbuf, void *_cmd_buf,
                         MemoryAllocators *mem_allocs, const Tex2DParams &p, ILog *log);
    void InitFromDDSFile(const void *data[6], const int size[6], Buffer &sbuf, void *_cmd_buf,
                         MemoryAllocators *mem_allocs, const Tex2DParams &p, ILog *log);
    void InitFromKTXFile(const void *data[6], const int size[6], Buffer &sbuf, void *_cmd_buf,
                         MemoryAllocators *mem_allocs, const Tex2DParams &p, ILog *log);

  public:
    uint32_t first_user = 0xffffffff;

    Texture2D() = default;
    Texture2D(const char *name, ApiContext *api_ctx, const Tex2DParams &params, MemoryAllocators *mem_allocs,
              ILog *log);
    Texture2D(const char *name, ApiContext *api_ctx, VkImage img, VkImageView view, VkSampler sampler,
              const Tex2DParams &params, ILog *log)
        : handle_{img, view, sampler, 0}, params_(params), ready_(true), name_(name) {}
    Texture2D(const char *name, ApiContext *api_ctx, const void *data, const uint32_t size, const Tex2DParams &p,
              Buffer &stage_buf, void *_cmd_buf, MemoryAllocators *mem_allocs, eTexLoadStatus *load_status, ILog *log);
    Texture2D(const char *name, ApiContext *api_ctx, const void *data[6], const int size[6], const Tex2DParams &p,
              Buffer &stage_buf, void *_cmd_buf, MemoryAllocators *mem_allocs, eTexLoadStatus *load_status, ILog *log);
    Texture2D(const Texture2D &rhs) = delete;
    Texture2D(Texture2D &&rhs) noexcept { (*this) = std::move(rhs); }
    ~Texture2D();

    Texture2D &operator=(const Texture2D &rhs) = delete;
    Texture2D &operator=(Texture2D &&rhs) noexcept;

    void Init(const Tex2DParams &params, MemoryAllocators *mem_allocs, ILog *log);
    void Init(const void *data, const uint32_t size, const Tex2DParams &p, Buffer &stage_buf, void *_cmd_buf,
              MemoryAllocators *mem_allocs, eTexLoadStatus *load_status, ILog *log);
    void Init(const void *data[6], const int size[6], const Tex2DParams &p, Buffer &stage_buf, void *_cmd_buf,
              MemoryAllocators *mem_allocs, eTexLoadStatus *load_status, ILog *log);

    bool Realloc(int w, int h, int mip_count, int samples, Ren::eTexFormat format, Ren::eTexBlock block, bool is_srgb,
                 void *_cmd_buf, MemoryAllocators *mem_allocs, ILog *log);

    TexHandle handle() const { return handle_; }
    VkSampler sampler() const { return handle_.sampler; }
    uint16_t initialized_mips() const { return initialized_mips_; }

    const Tex2DParams &params() const { return params_; }
    const SamplingParams &sampling() const { return params_.sampling; }

    bool ready() const { return ready_; }
    const String &name() const { return name_; }

    void SetSampling(SamplingParams sampling);
    void ApplySampling(SamplingParams sampling, ILog *log) { SetSampling(sampling); }

    void SetSubImage(int level, int offsetx, int offsety, int sizex, int sizey, Ren::eTexFormat format,
                     const void *data, int data_len);
    void SetSubImage(int level, int offsetx, int offsety, int sizex, int sizey, Ren::eTexFormat format,
                     const Buffer &sbuf, void *_cmd_buf, int data_off, int data_len);

    void DownloadTextureData(eTexFormat format, void *out_data) const;

    mutable VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
    mutable VkAccessFlags last_access_mask = 0;
    mutable VkPipelineStageFlags last_stage_mask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
};

struct Texture1DParams {
    uint16_t offset = 0, size = 0;
    eTexFormat format = eTexFormat::Undefined;
    uint8_t _padding;
};
static_assert(sizeof(Texture1DParams) == 6, "!");

class Texture1D : public RefCounter {
    TexHandle handle_;
    BufferRef buf_;
    Texture1DParams params_;
    String name_;

    VkBufferView buf_view_ = VK_NULL_HANDLE;

    void Free();

  public:
    Texture1D(const char *name, BufferRef buf, eTexFormat format, uint32_t offset, uint32_t size, ILog *log);
    Texture1D(const Texture1D &rhs) = delete;
    Texture1D(Texture1D &&rhs) noexcept { (*this) = std::move(rhs); }
    ~Texture1D();

    Texture1D &operator=(const Texture1D &rhs) = delete;
    Texture1D &operator=(Texture1D &&rhs) noexcept;

    TexHandle handle() const { return handle_; }
    // uint32_t id() const { return handle_.id; }
    // int generation() const { return handle_.generation; }

    const Texture1DParams &params() const { return params_; }

    const String &name() const { return name_; }

    void Init(BufferRef buf, eTexFormat format, uint32_t offset, uint32_t size, ILog *log);
};

VkFormat VKFormatFromTexFormat(eTexFormat format);

} // namespace Ren

#ifdef _MSC_VER
#pragma warning(pop)
#endif
