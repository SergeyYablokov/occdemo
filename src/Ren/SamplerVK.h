#pragma once

#include "SamplingParams.h"
#include "Storage.h"

namespace Ren {
class Sampler : public RefCounter {
    ApiContext *api_ctx_ = nullptr;
    VkSampler handle_ = VK_NULL_HANDLE;
    SamplingParams params_;

    void Destroy();

  public:
    Sampler() = default;
    Sampler(const Sampler &rhs) = delete;
    Sampler(Sampler &&rhs) noexcept { (*this) = std::move(rhs); }
    ~Sampler() { Destroy(); }

    VkSampler handle() const { return handle_; }
    SamplingParams params() const { return params_; }

    Sampler &operator=(const Sampler &rhs) = delete;
    Sampler &operator=(Sampler &&rhs) noexcept;

    void Init(ApiContext *api_ctx, SamplingParams params);
};

} // namespace Ren