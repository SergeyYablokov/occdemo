#include "SamplerVK.h"

namespace Ren {
const VkFilter g_vk_min_mag_filter[] = {
    VK_FILTER_NEAREST, // NoFilter
    VK_FILTER_LINEAR,  // Bilinear
    VK_FILTER_LINEAR,  // Trilinear
    VK_FILTER_LINEAR,  // BilinearNoMipmap
};
static_assert(sizeof(g_vk_min_mag_filter) / sizeof(g_vk_min_mag_filter[0]) == size_t(eTexFilter::_Count), "!");

const VkSamplerAddressMode g_vk_wrap_mode[] = {
    VK_SAMPLER_ADDRESS_MODE_REPEAT,          // Repeat
    VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,   // ClampToEdge
    VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER, // ClampToBorder
};
static_assert(sizeof(g_vk_wrap_mode) / sizeof(g_vk_wrap_mode[0]) == size_t(eTexRepeat::WrapModesCount), "!");

const VkSamplerMipmapMode g_vk_mipmap_mode[] = {
    VK_SAMPLER_MIPMAP_MODE_NEAREST, // NoFilter
    VK_SAMPLER_MIPMAP_MODE_NEAREST, // Bilinear
    VK_SAMPLER_MIPMAP_MODE_LINEAR,  // Trilinear
    VK_SAMPLER_MIPMAP_MODE_NEAREST, // BilinearNoMipmap
};
static_assert(sizeof(g_vk_mipmap_mode) / sizeof(g_vk_mipmap_mode[0]) == size_t(eTexFilter::_Count), "!");

const float AnisotropyLevel = 4.0f;
} // namespace Ren

Ren::Sampler &Ren::Sampler::operator=(Sampler &&rhs) noexcept {
    if (&rhs == this) {
        return (*this);
    }

    Destroy();

    RefCounter::operator=(std::move(rhs));

    api_ctx_ = exchange(rhs.api_ctx_, nullptr);
    handle_ = exchange(rhs.handle_, {});
    params_ = exchange(rhs.params_, {});

    return (*this);
}

void Ren::Sampler::Destroy() {
    if (handle_) {
        vkDestroySampler(api_ctx_->device, handle_, nullptr);
    }
}

void Ren::Sampler::Init(ApiContext *api_ctx, const SamplingParams params) {
    Destroy();

    VkSamplerCreateInfo sampler_info = {};
    sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sampler_info.magFilter = g_vk_min_mag_filter[size_t(params.filter)];
    sampler_info.minFilter = g_vk_min_mag_filter[size_t(params.filter)];
    sampler_info.addressModeU = g_vk_wrap_mode[size_t(params.repeat)];
    sampler_info.addressModeV = g_vk_wrap_mode[size_t(params.repeat)];
    sampler_info.addressModeW = g_vk_wrap_mode[size_t(params.repeat)];
    sampler_info.anisotropyEnable = VK_TRUE;
    sampler_info.maxAnisotropy = AnisotropyLevel;
    sampler_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    sampler_info.unnormalizedCoordinates = VK_FALSE;
    sampler_info.compareEnable = VK_FALSE;
    sampler_info.compareOp = VK_COMPARE_OP_ALWAYS;
    sampler_info.mipmapMode = g_vk_mipmap_mode[size_t(params.filter)];
    sampler_info.mipLodBias = params.lod_bias.to_float();
    sampler_info.minLod = params.min_lod.to_float();
    sampler_info.maxLod = params.max_lod.to_float();

    const VkResult res = vkCreateSampler(api_ctx->device, &sampler_info, nullptr, &handle_);
    assert(res == VK_SUCCESS && "Failed to create sampler!");

    api_ctx_ = api_ctx;
    params_ = params;
}
