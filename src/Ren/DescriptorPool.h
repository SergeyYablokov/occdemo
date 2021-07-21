#pragma once

#include "VK.h"

namespace Ren {
struct ApiContext;

//
// DescrPool is able to allocate up to fixed amount of sets of specific size
//
struct DescrPool {
    ApiContext *api_ctx_ = nullptr;
    VkDescriptorPool handle_ = VK_NULL_HANDLE;
    uint32_t sets_count_ = 0, next_free_ = 0;

  public:
    DescrPool(ApiContext *api_ctx) : api_ctx_(api_ctx) {}
    DescrPool(const DescrPool &rhs) = delete;
    DescrPool(DescrPool &&rhs) noexcept { (*this) = std::move(rhs); }
    ~DescrPool() { Destroy(); }

    DescrPool &operator=(const DescrPool &rhs) = delete;
    DescrPool &operator=(DescrPool &&rhs) noexcept;

    uint32_t free_count() const { return sets_count_ - next_free_; }

    bool Init(uint32_t img_count, uint32_t ubuf_count, uint32_t sbuf_count, uint32_t sets_count);
    void Destroy();

    VkDescriptorSet Alloc(VkDescriptorSetLayout layout);
    bool Reset();
};

//
// DescrPoolAlloc is able to allocate any amount of sets of specific size
//
class DescrPoolAlloc {
    ApiContext *api_ctx_ = nullptr;
    uint32_t img_count_ = 0, ubuf_count_ = 0, sbuf_count_ = 0;
    uint32_t initial_sets_count_ = 0;

    SmallVector<DescrPool, 16> pools_;
    int next_free_pool_ = -1;

  public:
    DescrPoolAlloc(ApiContext *api_ctx, const uint32_t img_count, const uint32_t ubuf_count, const uint32_t sbuf_count,
                   const uint32_t initial_sets_count)
        : api_ctx_(api_ctx), img_count_(img_count), ubuf_count_(ubuf_count), sbuf_count_(sbuf_count),
          initial_sets_count_(initial_sets_count) {}

    VkDescriptorSet Alloc(VkDescriptorSetLayout layout);
    bool Reset();
};

//
// DescrMultiPoolAlloc is able to allocate any amount of sets of any size
//
class DescrMultiPoolAlloc {
    uint32_t pool_step_ = 0;
    uint32_t max_img_count_ = 0, max_ubuf_count_ = 0, max_sbuf_count_ = 0;
    SmallVector<DescrPoolAlloc, 16> pools_;

  public:
    DescrMultiPoolAlloc(ApiContext *api_ctx, uint32_t pool_step, uint32_t max_img_count, uint32_t max_ubuf_count,
                        uint32_t max_sbuf_count, uint32_t initial_sets_count);

    VkDescriptorSet Alloc(uint32_t img_count, uint32_t ubuf_count, uint32_t sbuf_count, VkDescriptorSetLayout layout);
    bool Reset();
};
} // namespace Ren