#include "TextureVK.h"

#include <memory>

#include "Utils.h"

#include "stb/stb_image.h"

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4996)
#endif

#ifndef NDEBUG
//#define TEX_VERBOSE_LOGGING
#endif

namespace Ren {
const VkFormat g_vk_formats[] = {
    VK_FORMAT_UNDEFINED,                // Undefined
    VK_FORMAT_R8G8B8_UNORM,             // RawRGB888
    VK_FORMAT_R8G8B8A8_UNORM,           // RawRGBA8888
    VK_FORMAT_R8G8B8A8_SNORM,           // RawRGBA8888Signed
    VK_FORMAT_B8G8R8A8_UNORM,           // RawBGRA8888
    VK_FORMAT_R32_SFLOAT,               // RawR32F
    VK_FORMAT_R16_SFLOAT,               // RawR16F
    VK_FORMAT_R8_UNORM,                 // RawR8
    VK_FORMAT_R8G8_UNORM,               // RawRG88
    VK_FORMAT_R32G32B32_SFLOAT,         // RawRGB32F
    VK_FORMAT_R32G32B32A32_SFLOAT,      // RawRGBA32F
    VK_FORMAT_UNDEFINED,                // RawRGBE8888
    VK_FORMAT_R16G16B16_SFLOAT,         // RawRGB16F
    VK_FORMAT_R16G16B16A16_SFLOAT,      // RawRGBA16F
    VK_FORMAT_R16G16_UNORM,             // RawRG16
    VK_FORMAT_R16G16_UINT,              // RawRG16U
    VK_FORMAT_R16G16_SFLOAT,            // RawRG16F
    VK_FORMAT_R32G32_SFLOAT,            // RawRG32F
    VK_FORMAT_R32G32_UINT,              // RawRG32U
    VK_FORMAT_A2B10G10R10_UNORM_PACK32, // RawRGB10_A2
    VK_FORMAT_B10G11R11_UFLOAT_PACK32,  // RawRG11F_B10F
    VK_FORMAT_D16_UNORM,                // Depth16
    VK_FORMAT_D24_UNORM_S8_UINT,        // Depth24Stencil8
#ifndef __ANDROID__
    VK_FORMAT_D32_SFLOAT, // Depth32
#endif
    VK_FORMAT_BC1_RGBA_UNORM_BLOCK, // Compressed_DXT1
    VK_FORMAT_BC2_UNORM_BLOCK,      // Compressed_DXT3
    VK_FORMAT_BC3_UNORM_BLOCK,      // Compressed_DXT5
    VK_FORMAT_UNDEFINED,            // Compressed_ASTC
    VK_FORMAT_UNDEFINED,            // None
};
static_assert(sizeof(g_vk_formats) / sizeof(g_vk_formats[0]) == size_t(eTexFormat::_Count), "!");

/*

const uint32_t g_gl_internal_formats[] = {
    0xffffffff,           // Undefined
    GL_RGB8,              // RawRGB888
    GL_RGBA8,             // RawRGBA8888
    GL_RGBA8_SNORM,       // RawSignedRGBA8888
    GL_R32F,              // RawR32F
    GL_R16F,              // RawR16F
    GL_R8,                // RawR8
    GL_RG8,               // RawRG88
    GL_RGB32F,            // RawRGB32F
    GL_RGBA32F,           // RawRGBA32F
    0xffffffff,           // RawRGBE8888
    GL_RGB16F,            // RawRGB16F
    GL_RGBA16F,           // RawRGBA16F
    GL_RG16_SNORM_EXT,    // RawRG16
    GL_RG16_EXT,          // RawRG16U
    GL_RG16F,             // RawRG16F
    GL_RG32F,             // RawRG32F
    GL_RG32UI,            // RawRG32U
    GL_RGB10_A2,          // RawRGB10_A2
    GL_R11F_G11F_B10F,    // RawRG11F_B10F
    GL_DEPTH_COMPONENT16, // Depth16
    GL_DEPTH24_STENCIL8,  // Depth24Stencil8
#ifndef __ANDROID__
    GL_DEPTH_COMPONENT32, // Depth32
#endif
    GL_COMPRESSED_RGBA_S3TC_DXT1_EXT, // Compressed_DXT1
    GL_COMPRESSED_RGBA_S3TC_DXT3_EXT, // Compressed_DXT3
    GL_COMPRESSED_RGBA_S3TC_DXT5_EXT, // Compressed_DXT5
    0xffffffff,                       // Compressed_ASTC
    0xffffffff,                       // None
};
static_assert(sizeof(g_gl_internal_formats) / sizeof(g_gl_internal_formats[0]) ==
                  size_t(eTexFormat::_Count),
              "!");

const uint32_t g_gl_types[] = {
    0xffffffff,        // Undefined
    GL_UNSIGNED_BYTE,  // RawRGB888
    GL_UNSIGNED_BYTE,  // RawRGBA8888
    GL_BYTE,           // RawSignedRGBA8888
    GL_FLOAT,          // RawR32F
    GL_HALF_FLOAT,     // RawR16F
    GL_UNSIGNED_BYTE,  // RawR8
    GL_UNSIGNED_BYTE,  // RawRG88
    GL_FLOAT,          // RawRGB32F
    GL_FLOAT,          // RawRGBA32F
    0xffffffff,        // RawRGBE8888
    GL_HALF_FLOAT,     // RawRGB16F
    GL_HALF_FLOAT,     // RawRGBA16F
    GL_SHORT,          // RawRG16
    GL_UNSIGNED_SHORT, // RawRG16U
    GL_HALF_FLOAT,     // RawRG16F
    GL_FLOAT,          // RawRG32F
    GL_UNSIGNED_INT,   // RawRG32U
    GL_UNSIGNED_BYTE,  // RawRGB10_A2
    GL_FLOAT,          // RawRG11F_B10F
    GL_UNSIGNED_SHORT, // Depth16
    GL_UNSIGNED_INT,   // Depth24Stencil8
#ifndef __ANDROID__
    GL_UNSIGNED_INT, // Depth32
#endif
    0xffffffff, // Compressed_DXT1
    0xffffffff, // Compressed_DXT3
    0xffffffff, // Compressed_DXT5
    0xffffffff, // Compressed_ASTC
    0xffffffff, // None
};
static_assert(sizeof(g_gl_types) / sizeof(g_gl_types[0]) == size_t(eTexFormat::_Count),
              "!");

const uint32_t g_gl_compare_func[] = {
    0xffffffff,  // None
    GL_LEQUAL,   // LEqual
    GL_GEQUAL,   // GEqual
    GL_LESS,     // Less
    GL_GREATER,  // Greater
    GL_EQUAL,    // Equal
    GL_NOTEQUAL, // NotEqual
    GL_ALWAYS,   // Always
    GL_NEVER,    // Never
};
static_assert(sizeof(g_gl_compare_func) / sizeof(g_gl_compare_func[0]) ==
                  size_t(eTexCompare::_Count),
              "!");

const uint32_t gl_binding_targets[] = {
    GL_TEXTURE_2D,             // Tex2D
    GL_TEXTURE_2D_MULTISAMPLE, // Tex2DMs
    GL_TEXTURE_CUBE_MAP_ARRAY, // TexCubeArray
    GL_TEXTURE_BUFFER,         // TexBuf
    GL_UNIFORM_BUFFER,         // UBuf
};
static_assert(sizeof(gl_binding_targets) / sizeof(gl_binding_targets[0]) ==
                  size_t(eBindTarget::_Count),
              "!");
              */

uint32_t FindMemoryType(const VkPhysicalDeviceMemoryProperties *mem_properties, uint32_t mem_type_bits,
                        VkMemoryPropertyFlags desired_mem_flags);

VkDeviceSize AlignTo(VkDeviceSize size, VkDeviceSize alignment) {
    return alignment * ((size + alignment - 1) / alignment);
}

uint32_t TextureHandleCounter = 0;

bool IsMainThread();

// make sure we can simply cast these
static_assert(VkSampleCountFlagBits::VK_SAMPLE_COUNT_1_BIT == 1, "!");
static_assert(VkSampleCountFlagBits::VK_SAMPLE_COUNT_2_BIT == 2, "!");
static_assert(VkSampleCountFlagBits::VK_SAMPLE_COUNT_4_BIT == 4, "!");
static_assert(VkSampleCountFlagBits::VK_SAMPLE_COUNT_8_BIT == 8, "!");

VkFormat ToSRGBFormat(const VkFormat format) {
    switch (format) {
    case VK_FORMAT_R8G8B8_UNORM:
        return VK_FORMAT_R8G8B8_SRGB;
    case VK_FORMAT_R8G8B8A8_UNORM:
        return VK_FORMAT_R8G8B8A8_SRGB;
    case VK_FORMAT_BC1_RGBA_UNORM_BLOCK:
        return VK_FORMAT_BC1_RGBA_SRGB_BLOCK;
    case VK_FORMAT_BC2_UNORM_BLOCK:
        return VK_FORMAT_BC2_SRGB_BLOCK;
    case VK_FORMAT_BC3_UNORM_BLOCK:
        return VK_FORMAT_BC3_SRGB_BLOCK;
    case VK_FORMAT_ASTC_4x4_UNORM_BLOCK:
        return VK_FORMAT_ASTC_4x4_SRGB_BLOCK;
    case VK_FORMAT_ASTC_5x4_UNORM_BLOCK:
        return VK_FORMAT_ASTC_5x4_SRGB_BLOCK;
    case VK_FORMAT_ASTC_5x5_UNORM_BLOCK:
        return VK_FORMAT_ASTC_5x5_SRGB_BLOCK;
    case VK_FORMAT_ASTC_6x5_UNORM_BLOCK:
        return VK_FORMAT_ASTC_6x5_SRGB_BLOCK;
    case VK_FORMAT_ASTC_6x6_UNORM_BLOCK:
        return VK_FORMAT_ASTC_6x6_SRGB_BLOCK;
    case VK_FORMAT_ASTC_8x5_UNORM_BLOCK:
        return VK_FORMAT_ASTC_8x5_SRGB_BLOCK;
    case VK_FORMAT_ASTC_8x6_UNORM_BLOCK:
        return VK_FORMAT_ASTC_8x6_SRGB_BLOCK;
    case VK_FORMAT_ASTC_8x8_UNORM_BLOCK:
        return VK_FORMAT_ASTC_8x8_SRGB_BLOCK;
    case VK_FORMAT_ASTC_10x5_UNORM_BLOCK:
        return VK_FORMAT_ASTC_10x5_SRGB_BLOCK;
    case VK_FORMAT_ASTC_10x6_UNORM_BLOCK:
        return VK_FORMAT_ASTC_10x6_SRGB_BLOCK;
    case VK_FORMAT_ASTC_10x8_UNORM_BLOCK:
        return VK_FORMAT_ASTC_10x8_SRGB_BLOCK;
    case VK_FORMAT_ASTC_10x10_UNORM_BLOCK:
        return VK_FORMAT_ASTC_10x10_SRGB_BLOCK;
    case VK_FORMAT_ASTC_12x10_UNORM_BLOCK:
        return VK_FORMAT_ASTC_12x10_SRGB_BLOCK;
    case VK_FORMAT_ASTC_12x12_UNORM_BLOCK:
        return VK_FORMAT_ASTC_12x12_SRGB_BLOCK;
    default:
        assert(false && "Unsupported format!");
    }
    return VK_FORMAT_UNDEFINED;
}
} // namespace Ren

Ren::Texture2D::Texture2D(const char *name, ApiContext *api_ctx, const Tex2DParams &p, MemoryAllocators *mem_allocs,
                          ILog *log)
    : api_ctx_(api_ctx), name_(name) {
    Init(p, mem_allocs, log);
}

Ren::Texture2D::Texture2D(const char *name, ApiContext *api_ctx, const void *data, const uint32_t size,
                          const Tex2DParams &p, Buffer &stage_buf, void *_cmd_buf, MemoryAllocators *mem_allocs,
                          eTexLoadStatus *load_status, ILog *log)
    : api_ctx_(api_ctx), name_(name) {
    Init(data, size, p, stage_buf, _cmd_buf, mem_allocs, load_status, log);
}

Ren::Texture2D::Texture2D(const char *name, ApiContext *api_ctx, const void *data[6], const int size[6],
                          const Tex2DParams &p, Buffer &stage_buf, void *_cmd_buf, MemoryAllocators *mem_allocs,
                          eTexLoadStatus *load_status, ILog *log)
    : api_ctx_(api_ctx), name_(name) {
    Init(data, size, p, stage_buf, _cmd_buf, mem_allocs, load_status, log);
}

Ren::Texture2D::~Texture2D() { Free(); }

Ren::Texture2D &Ren::Texture2D::operator=(Ren::Texture2D &&rhs) noexcept {
    if (this == &rhs) {
        return (*this);
    }

    RefCounter::operator=(static_cast<RefCounter &&>(rhs));

    Free();

    api_ctx_ = exchange(rhs.api_ctx_, nullptr);
    handle_ = exchange(rhs.handle_, {});
    alloc_ = exchange(rhs.alloc_, {});
    params_ = exchange(rhs.params_, {});
    ready_ = exchange(rhs.ready_, false);
    cubemap_ready_ = exchange(rhs.cubemap_ready_, 0);
    name_ = std::move(rhs.name_);

    layout = exchange(rhs.layout, VK_IMAGE_LAYOUT_UNDEFINED);
    last_access_mask = exchange(rhs.last_access_mask, 0);
    last_stage_mask = exchange(rhs.last_stage_mask, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);

    return (*this);
}

void Ren::Texture2D::Init(const Tex2DParams &p, MemoryAllocators *mem_allocs, ILog *log) {
    assert(IsMainThread());
    InitFromRAWData(nullptr, 0, nullptr, mem_allocs, p, log);
    ready_ = true;
}

void Ren::Texture2D::Init(const void *data, const uint32_t size, const Tex2DParams &p, Buffer &sbuf, void *_cmd_buf,
                          MemoryAllocators *mem_allocs, eTexLoadStatus *load_status, ILog *log) {
    assert(IsMainThread());
    if (!data) {
        uint8_t *stage_data = sbuf.Map(BufMapWrite);
        memcpy(stage_data, p.fallback_color, 4);
        sbuf.FlushMappedRange(0, 4);
        sbuf.Unmap();

        Tex2DParams _p = p;
        _p.w = _p.h = 1;
        _p.mip_count = 1;
        _p.format = eTexFormat::RawRGBA8888;

        InitFromRAWData(&sbuf, 0, _cmd_buf, mem_allocs, _p, log);
        // mark it as not ready
        ready_ = false;
        (*load_status) = eTexLoadStatus::CreatedDefault;
    } else {
        if (name_.EndsWith(".tga_rgbe") != 0 || name_.EndsWith(".TGA_RGBE") != 0) {
            InitFromTGA_RGBEFile(data, sbuf, _cmd_buf, mem_allocs, p, log);
        } else if (name_.EndsWith(".tga") != 0 || name_.EndsWith(".TGA") != 0) {
            InitFromTGAFile(data, sbuf, _cmd_buf, mem_allocs, p, log);
        } else if (name_.EndsWith(".dds") != 0 || name_.EndsWith(".DDS") != 0) {
            InitFromDDSFile(data, size, sbuf, _cmd_buf, mem_allocs, p, log);
        } else if (name_.EndsWith(".ktx") != 0 || name_.EndsWith(".KTX") != 0) {
            InitFromKTXFile(data, size, sbuf, _cmd_buf, mem_allocs, p, log);
        } else if (name_.EndsWith(".png") != 0 || name_.EndsWith(".PNG") != 0) {
            InitFromPNGFile(data, size, sbuf, _cmd_buf, mem_allocs, p, log);
        } else {
            uint8_t *stage_data = sbuf.Map(BufMapWrite);
            memcpy(stage_data, data, size);
            sbuf.FlushMappedRange(0, size);
            sbuf.Unmap();

            InitFromRAWData(&sbuf, 0, _cmd_buf, mem_allocs, p, log);
        }
        ready_ = true;
        (*load_status) = eTexLoadStatus::CreatedFromData;
    }
}

void Ren::Texture2D::Init(const void *data[6], const int size[6], const Tex2DParams &p, Buffer &sbuf, void *_cmd_buf,
                          MemoryAllocators *mem_allocs, eTexLoadStatus *load_status, ILog *log) {
    assert(IsMainThread());
    if (!data) {
        uint8_t *stage_data = sbuf.Map(BufMapWrite);
        memcpy(stage_data, p.fallback_color, 4);
        sbuf.FlushMappedRange(0, 4);
        sbuf.Unmap();

        int data_off[6] = {};

        Tex2DParams _p = p;
        _p.w = _p.h = 1;
        _p.format = eTexFormat::RawRGBA8888;
        InitFromRAWData(sbuf, data_off, _cmd_buf, mem_allocs, _p, log);
        // mark it as not ready
        ready_ = false;
        cubemap_ready_ = 0;
        (*load_status) = eTexLoadStatus::CreatedDefault;
    } else {
        if (name_.EndsWith(".tga_rgbe") != 0 || name_.EndsWith(".TGA_RGBE") != 0) {
            InitFromTGA_RGBEFile(data, sbuf, _cmd_buf, mem_allocs, p, log);
        } else if (name_.EndsWith(".tga") != 0 || name_.EndsWith(".TGA") != 0) {
            InitFromTGAFile(data, sbuf, _cmd_buf, mem_allocs, p, log);
        } else if (name_.EndsWith(".png") != 0 || name_.EndsWith(".PNG") != 0) {
            InitFromPNGFile(data, size, sbuf, _cmd_buf, mem_allocs, p, log);
        } else if (name_.EndsWith(".ktx") != 0 || name_.EndsWith(".KTX") != 0) {
            InitFromKTXFile(data, size, sbuf, _cmd_buf, mem_allocs, p, log);
        } else if (name_.EndsWith(".dds") != 0 || name_.EndsWith(".DDS") != 0) {
            InitFromDDSFile(data, size, sbuf, _cmd_buf, mem_allocs, p, log);
        } else {
            uint8_t *stage_data = sbuf.Map(BufMapWrite);
            uint32_t stage_off = 0;

            int data_off[6];
            for (int i = 0; i < 6; i++) {
                if (data[i]) {
                    memcpy(&stage_data[stage_off], data[i], size[i]);
                    data_off[i] = int(stage_off);
                    stage_off += size[i];
                } else {
                    data_off[i] = -1;
                }
            }
            sbuf.FlushMappedRange(0, 4);
            sbuf.Unmap();

            InitFromRAWData(sbuf, data_off, _cmd_buf, mem_allocs, p, log);
        }

        ready_ = (cubemap_ready_ & (1u << 0u)) == 1;
        for (unsigned i = 1; i < 6; i++) {
            ready_ = ready_ && ((cubemap_ready_ & (1u << i)) == 1);
        }
        (*load_status) = eTexLoadStatus::CreatedFromData;
    }
}

void Ren::Texture2D::Free() {
    if (params_.format != eTexFormat::Undefined && !(params_.flags & TexNoOwnership)) {
        assert(IsMainThread());

        api_ctx_->image_views_to_destroy[api_ctx_->backend_frame].push_back(handle_.view);
        api_ctx_->images_to_destroy[api_ctx_->backend_frame].push_back(handle_.img);
        api_ctx_->samplers_to_destroy[api_ctx_->backend_frame].push_back(handle_.sampler);
        api_ctx_->allocs_to_free[api_ctx_->backend_frame].emplace_back(std::move(alloc_));

        handle_ = {};
        params_.format = eTexFormat::Undefined;
    }
}

bool Ren::Texture2D::Realloc(const int w, const int h, int mip_count, const int samples, const Ren::eTexFormat format,
                             const Ren::eTexBlock block, const bool is_srgb, void *_cmd_buf,
                             MemoryAllocators *mem_allocs, ILog *log) {
    VkImage new_image = VK_NULL_HANDLE;
    VkImageView new_image_view = VK_NULL_HANDLE;
    MemAllocation new_alloc = {};
    VkImageLayout new_layout = VK_IMAGE_LAYOUT_UNDEFINED;
    VkAccessFlags new_access_mask = 0;
    VkPipelineStageFlags new_stage_mask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;

    mip_count = std::min(mip_count, CalcMipCount(w, h, 1, Ren::eTexFilter::Trilinear));

    { // create new image
        VkImageCreateInfo img_info = {};
        img_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        img_info.imageType = VK_IMAGE_TYPE_2D;
        img_info.extent.width = uint32_t(w);
        img_info.extent.height = uint32_t(h);
        img_info.extent.depth = 1;
        img_info.mipLevels = mip_count;
        img_info.arrayLayers = 1;
        img_info.format = g_vk_formats[size_t(format)];
        if (is_srgb) {
            img_info.format = ToSRGBFormat(img_info.format);
        }
        img_info.tiling = VK_IMAGE_TILING_OPTIMAL;
        img_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        img_info.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

        if (!IsCompressedFormat(format)) {
            img_info.usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        }

        img_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        img_info.samples = VkSampleCountFlagBits(samples);
        img_info.flags = 0;

        VkResult res = vkCreateImage(api_ctx_->device, &img_info, nullptr, &new_image);
        if (res != VK_SUCCESS) {
            log->Error("Failed to create image!");
            return false;
        }

#ifdef ENABLE_OBJ_LABELS
        VkDebugUtilsObjectNameInfoEXT name_info = {};
        name_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
        name_info.objectType = VK_OBJECT_TYPE_IMAGE;
        name_info.objectHandle = uint64_t(new_image);
        name_info.pObjectName = name_.c_str();
        vkSetDebugUtilsObjectNameEXT(api_ctx_->device, &name_info);
#endif

        VkMemoryRequirements tex_mem_req;
        vkGetImageMemoryRequirements(api_ctx_->device, new_image, &tex_mem_req);

        new_alloc = mem_allocs->Allocate(
            uint32_t(tex_mem_req.size), uint32_t(tex_mem_req.alignment),
            FindMemoryType(&api_ctx_->mem_properties, tex_mem_req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT),
            name_.c_str());

        const VkDeviceSize aligned_offset = AlignTo(VkDeviceSize(new_alloc.alloc_off), tex_mem_req.alignment);

        res = vkBindImageMemory(api_ctx_->device, new_image, new_alloc.owner->mem(new_alloc.block_ndx), aligned_offset);
        if (res != VK_SUCCESS) {
            log->Error("Failed to bind memory!");
            return false;
        }
    }

    { // create new image view
        VkImageViewCreateInfo view_info = {};
        view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view_info.image = new_image;
        view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view_info.format = g_vk_formats[size_t(format)];
        if (is_srgb) {
            view_info.format = ToSRGBFormat(view_info.format);
        }
        view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        view_info.subresourceRange.baseMipLevel = 0;
        view_info.subresourceRange.levelCount = mip_count;
        view_info.subresourceRange.baseArrayLayer = 0;
        view_info.subresourceRange.layerCount = 1;

        const VkResult res = vkCreateImageView(api_ctx_->device, &view_info, nullptr, &new_image_view);
        if (res != VK_SUCCESS) {
            log->Error("Failed to create image view!");
            return false;
        }
    }

#ifdef TEX_VERBOSE_LOGGING
    if (params_.format != eTexFormat::Undefined) {
        log->Info("Realloc %s, %ix%i (%i mips) -> %ix%i (%i mips)", name_.c_str(), int(params_.w), int(params_.h),
                  int(params_.mip_count), w, h, mip_count);
    } else {
        log->Info("Alloc %s %ix%i (%i mips)", name_.c_str(), w, h, mip_count);
    }
#endif

    const TexHandle new_handle = {new_image, new_image_view, exchange(handle_.sampler, {}), TextureHandleCounter++};
    uint16_t new_initialized_mips = 0;

    // copy data from old texture
    if (params_.format == format) {
        int src_mip = 0, dst_mip = 0;
        while (std::max(params_.w >> src_mip, 1) != std::max(w >> dst_mip, 1) ||
               std::max(params_.h >> src_mip, 1) != std::max(h >> dst_mip, 1)) {
            if (std::max(params_.w >> src_mip, 1) > std::max(w >> dst_mip, 1) ||
                std::max(params_.h >> src_mip, 1) > std::max(h >> dst_mip, 1)) {
                ++src_mip;
            } else {
                ++dst_mip;
            }
        }

        VkImageCopy copy_regions[16];
        uint32_t copy_regions_count = 0;

        for (; src_mip < int(params_.mip_count) && dst_mip < mip_count; ++src_mip, ++dst_mip) {
            if (initialized_mips_ & (1u << src_mip)) {
                VkImageCopy &reg = copy_regions[copy_regions_count++];

                reg.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                reg.srcSubresource.baseArrayLayer = 0;
                reg.srcSubresource.layerCount = 1;
                reg.srcSubresource.mipLevel = src_mip;
                reg.srcOffset = {0, 0, 0};
                reg.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                reg.dstSubresource.baseArrayLayer = 0;
                reg.dstSubresource.layerCount = 1;
                reg.dstSubresource.mipLevel = dst_mip;
                reg.dstOffset = {0, 0, 0};
                reg.extent = {uint32_t(std::max(w >> dst_mip, 1)), uint32_t(std::max(h >> dst_mip, 1)), 1};

#ifdef TEX_VERBOSE_LOGGING
                log->Info("Copying data mip %i [old] -> mip %i [new]", src_mip, dst_mip);
#endif

                new_initialized_mips |= (1u << dst_mip);
            }
        }

        if (copy_regions_count) {
            VkCommandBuffer cmd_buf = reinterpret_cast<VkCommandBuffer>(_cmd_buf);

            VkImageMemoryBarrier barriers[2] = {};

            // src image barrier
            barriers[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barriers[0].srcAccessMask = this->last_access_mask;
            barriers[0].dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            barriers[0].oldLayout = this->layout;
            barriers[0].newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            barriers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barriers[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barriers[0].image = handle_.img;
            barriers[0].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            barriers[0].subresourceRange.baseMipLevel = 0;
            barriers[0].subresourceRange.levelCount = params_.mip_count; // transit the whole image
            barriers[0].subresourceRange.baseArrayLayer = 0;
            barriers[0].subresourceRange.layerCount = 1;

            // dst image barrier
            barriers[1].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barriers[1].srcAccessMask = 0;
            barriers[1].dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barriers[1].oldLayout = new_layout;
            barriers[1].newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            barriers[1].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barriers[1].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barriers[1].image = new_image;
            barriers[1].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            barriers[1].subresourceRange.baseMipLevel = 0;
            barriers[1].subresourceRange.levelCount = mip_count; // transit the whole image
            barriers[1].subresourceRange.baseArrayLayer = 0;
            barriers[1].subresourceRange.layerCount = 1;

            vkCmdPipelineBarrier(cmd_buf, this->last_stage_mask, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0,
                                 nullptr, 2, barriers);

            vkCmdCopyImage(cmd_buf, handle_.img, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, new_image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, copy_regions_count, copy_regions);

            new_layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            new_access_mask = VK_ACCESS_TRANSFER_WRITE_BIT;
            new_stage_mask = VK_PIPELINE_STAGE_TRANSFER_BIT;
        }
    }
    Free();

    handle_ = new_handle;
    alloc_ = std::move(new_alloc);
    params_.w = w;
    params_.h = h;
    if (is_srgb) {
        params_.flags |= TexSRGB;
    } else {
        params_.flags &= ~TexSRGB;
    }
    params_.mip_count = mip_count;
    params_.samples = samples;
    params_.format = format;
    params_.block = block;
    initialized_mips_ = new_initialized_mips;

    this->layout = new_layout;
    this->last_access_mask = new_access_mask;
    this->last_stage_mask = new_stage_mask;

    return true;
}

void Ren::Texture2D::InitFromRAWData(Buffer *sbuf, int data_off, void *_cmd_buf, MemoryAllocators *mem_allocs,
                                     const Tex2DParams &p, ILog *log) {
    Free();

    handle_.generation = TextureHandleCounter++;
    params_ = p;
    initialized_mips_ = 0;

    const int mip_count = CalcMipCount(p.w, p.h, 1, p.sampling.filter);

    { // create image
        VkImageCreateInfo img_info = {};
        img_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        img_info.imageType = VK_IMAGE_TYPE_2D;
        img_info.extent.width = uint32_t(p.w);
        img_info.extent.height = uint32_t(p.h);
        img_info.extent.depth = 1;
        img_info.mipLevels = mip_count;
        img_info.arrayLayers = 1;
        img_info.format = g_vk_formats[size_t(p.format)];
        img_info.tiling = VK_IMAGE_TILING_OPTIMAL;
        img_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        img_info.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        if (IsDepthFormat(p.format)) {
            img_info.usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        } else {
            img_info.usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        }
        img_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        img_info.samples = VkSampleCountFlagBits(p.samples);
        img_info.flags = 0;

        VkResult res = vkCreateImage(api_ctx_->device, &img_info, nullptr, &handle_.img);
        if (res != VK_SUCCESS) {
            log->Error("Failed to create image!");
            return;
        }

#ifdef ENABLE_OBJ_LABELS
        VkDebugUtilsObjectNameInfoEXT name_info = {};
        name_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
        name_info.objectType = VK_OBJECT_TYPE_IMAGE;
        name_info.objectHandle = uint64_t(handle_.img);
        name_info.pObjectName = name_.c_str();
        vkSetDebugUtilsObjectNameEXT(api_ctx_->device, &name_info);
#endif

        VkMemoryRequirements tex_mem_req;
        vkGetImageMemoryRequirements(api_ctx_->device, handle_.img, &tex_mem_req);

        alloc_ = mem_allocs->Allocate(
            uint32_t(tex_mem_req.size), uint32_t(tex_mem_req.alignment),
            FindMemoryType(&api_ctx_->mem_properties, tex_mem_req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT),
            name_.c_str());

        const VkDeviceSize aligned_offset = AlignTo(VkDeviceSize(alloc_.alloc_off), tex_mem_req.alignment);

        res = vkBindImageMemory(api_ctx_->device, handle_.img, alloc_.owner->mem(alloc_.block_ndx), aligned_offset);
        if (res != VK_SUCCESS) {
            log->Error("Failed to bind memory!");
            return;
        }
    }

    { // create default image view
        VkImageViewCreateInfo view_info = {};
        view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view_info.image = handle_.img;
        view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view_info.format = g_vk_formats[size_t(p.format)];
        if (IsDepthFormat(p.format)) {
            view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
        } else {
            view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        }
        view_info.subresourceRange.baseMipLevel = 0;
        view_info.subresourceRange.levelCount = mip_count;
        view_info.subresourceRange.baseArrayLayer = 0;
        view_info.subresourceRange.layerCount = 1;

        const VkResult res = vkCreateImageView(api_ctx_->device, &view_info, nullptr, &handle_.view);
        if (res != VK_SUCCESS) {
            log->Error("Failed to create image view!");
            return;
        }
    }

    if (sbuf) {
        assert(p.samples == 1);
        assert(sbuf && sbuf->type() == eBufType::Stage);
        VkCommandBuffer cmd_buf = reinterpret_cast<VkCommandBuffer>(_cmd_buf);

        VkBufferMemoryBarrier stage_barrier = {};
        stage_barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        stage_barrier.srcAccessMask = sbuf->last_access_mask;
        stage_barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        stage_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        stage_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        stage_barrier.buffer = sbuf->handle().buf;
        stage_barrier.offset = VkDeviceSize(data_off);
        stage_barrier.size = VkDeviceSize(sbuf->size() - data_off);

        VkImageMemoryBarrier img_barrier = {};
        img_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        img_barrier.srcAccessMask = this->last_access_mask;
        img_barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        img_barrier.oldLayout = this->layout;
        img_barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        img_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        img_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        img_barrier.image = handle_.img;
        img_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        img_barrier.subresourceRange.baseMipLevel = 0;
        img_barrier.subresourceRange.levelCount = mip_count; // transit whole image
        img_barrier.subresourceRange.baseArrayLayer = 0;
        img_barrier.subresourceRange.layerCount = 1;

        vkCmdPipelineBarrier(cmd_buf, sbuf->last_stage_mask | this->last_stage_mask, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
                             0, nullptr, 1, &stage_barrier, 1, &img_barrier);

        VkBufferImageCopy region = {};
        region.bufferOffset = VkDeviceSize(data_off);
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;

        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = 1;

        region.imageOffset = {0, 0, 0};
        region.imageExtent = {uint32_t(p.w), uint32_t(p.h), 1};

        vkCmdCopyBufferToImage(cmd_buf, sbuf->handle().buf, handle_.img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
                               &region);

        sbuf->last_access_mask = VK_ACCESS_TRANSFER_READ_BIT;
        sbuf->last_stage_mask = VK_PIPELINE_STAGE_TRANSFER_BIT;

        this->layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        this->last_access_mask = VK_ACCESS_TRANSFER_WRITE_BIT;
        this->last_stage_mask = VK_PIPELINE_STAGE_TRANSFER_BIT;

        initialized_mips_ |= (1u << 0);
    }

    { // create new sampler
        VkSamplerCreateInfo sampler_info = {};
        sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        sampler_info.magFilter = g_vk_min_mag_filter[size_t(p.sampling.filter)];
        sampler_info.minFilter = g_vk_min_mag_filter[size_t(p.sampling.filter)];
        sampler_info.addressModeU = g_vk_wrap_mode[size_t(p.sampling.repeat)];
        sampler_info.addressModeV = g_vk_wrap_mode[size_t(p.sampling.repeat)];
        sampler_info.addressModeW = g_vk_wrap_mode[size_t(p.sampling.repeat)];
        sampler_info.anisotropyEnable = VK_TRUE;
        sampler_info.maxAnisotropy = AnisotropyLevel;
        sampler_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        sampler_info.unnormalizedCoordinates = VK_FALSE;
        sampler_info.compareEnable = VK_FALSE;
        sampler_info.compareOp = VK_COMPARE_OP_ALWAYS;
        sampler_info.mipmapMode = g_vk_mipmap_mode[size_t(p.sampling.filter)];
        sampler_info.mipLodBias = p.sampling.lod_bias.to_float();
        sampler_info.minLod = p.sampling.min_lod.to_float();
        sampler_info.maxLod = p.sampling.max_lod.to_float();

        const VkResult res = vkCreateSampler(api_ctx_->device, &sampler_info, nullptr, &handle_.sampler);
        if (res != VK_SUCCESS) {
            log->Error("Failed to create sampler!");
        }
    }
}

void Ren::Texture2D::InitFromTGAFile(const void *data, Buffer &sbuf, void *_cmd_buf, MemoryAllocators *mem_allocs,
                                     const Tex2DParams &p, ILog *log) {
    int w = 0, h = 0;
    eTexFormat format = eTexFormat::Undefined;
    uint32_t img_size = 0;
    const bool res1 = ReadTGAFile(data, w, h, format, nullptr, img_size);
    assert(res1 && img_size <= sbuf.size());

    uint8_t *stage_data = sbuf.Map(BufMapWrite);
    const bool res2 = ReadTGAFile(data, w, h, format, stage_data, img_size);
    assert(res2);
    sbuf.FlushMappedRange(0, img_size);
    sbuf.Unmap();

    Tex2DParams _p = p;
    _p.w = w;
    _p.h = h;
    _p.format = format;

    InitFromRAWData(&sbuf, 0, _cmd_buf, mem_allocs, _p, log);
}

void Ren::Texture2D::InitFromTGA_RGBEFile(const void *data, Buffer &sbuf, void *_cmd_buf, MemoryAllocators *mem_allocs,
                                          const Tex2DParams &p, ILog *log) {
    int w = 0, h = 0;
    eTexFormat format = eTexFormat::Undefined;
    std::unique_ptr<uint8_t[]> image_data = ReadTGAFile(data, w, h, format);
    assert(format == eTexFormat::RawRGBA8888);

    uint16_t *stage_data = reinterpret_cast<uint16_t *>(sbuf.Map(BufMapWrite));
    ConvertRGBE_to_RGB16F(image_data.get(), w, h, stage_data);
    sbuf.FlushMappedRange(0, 3 * w * h * sizeof(uint16_t));
    sbuf.Unmap();

    Tex2DParams _p = p;
    _p.w = w;
    _p.h = h;
    _p.format = eTexFormat::RawRGB16F;

    InitFromRAWData(&sbuf, 0, _cmd_buf, mem_allocs, _p, log);
}

void Ren::Texture2D::InitFromDDSFile(const void *data, const int size, Buffer &sbuf, void *_cmd_buf,
                                     MemoryAllocators *mem_allocs, const Tex2DParams &p, ILog *log) {
    DDSHeader header;
    memcpy(&header, data, sizeof(DDSHeader));

    eTexFormat format;
    eTexBlock block;
    int block_size_bytes;

    const int px_format = int(header.sPixelFormat.dwFourCC >> 24u) - '0';
    switch (px_format) {
    case 1:
        format = eTexFormat::Compressed_DXT1;
        block = eTexBlock::_4x4;
        block_size_bytes = 8;
        break;
    case 3:
        format = eTexFormat::Compressed_DXT3;
        block = eTexBlock::_4x4;
        block_size_bytes = 16;
        break;
    case 5:
        format = eTexFormat::Compressed_DXT5;
        block = eTexBlock::_4x4;
        block_size_bytes = 16;
        break;
    default:
        log->Error("Unknow DDS pixel format %i", px_format);
        return;
    }

    Free();
    Realloc(int(header.dwWidth), int(header.dwHeight), int(header.dwMipMapCount), 1, format, block,
            (p.flags & TexSRGB) != 0, _cmd_buf, mem_allocs, log);

    params_.flags = p.flags;
    params_.block = block;
    params_.sampling = p.sampling;

    int w = params_.w, h = params_.h;
    uint32_t bytes_left = uint32_t(size) - sizeof(DDSHeader);
    const uint8_t *p_data = (uint8_t *)data + sizeof(DDSHeader);

    assert(bytes_left <= sbuf.size());
    uint8_t *stage_data = sbuf.Map(BufMapWrite);
    memcpy(stage_data, p_data, bytes_left);
    sbuf.FlushMappedRange(0, bytes_left);
    sbuf.Unmap();

    assert(sbuf.type() == eBufType::Stage);
    VkCommandBuffer cmd_buf = reinterpret_cast<VkCommandBuffer>(_cmd_buf);

    VkBufferMemoryBarrier buf_barrier = {};
    buf_barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    buf_barrier.srcAccessMask = sbuf.last_access_mask;
    buf_barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    buf_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    buf_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    buf_barrier.buffer = sbuf.handle().buf;
    buf_barrier.offset = 0;
    buf_barrier.size = VkDeviceSize(bytes_left);

    VkImageMemoryBarrier img_barrier = {};
    img_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    img_barrier.srcAccessMask = this->last_access_mask;
    img_barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    img_barrier.oldLayout = this->layout;
    img_barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    img_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    img_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    img_barrier.image = handle_.img;
    img_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    img_barrier.subresourceRange.baseMipLevel = 0;
    img_barrier.subresourceRange.levelCount = params_.mip_count; // transit the whole image
    img_barrier.subresourceRange.baseArrayLayer = 0;
    img_barrier.subresourceRange.layerCount = 1;

    vkCmdPipelineBarrier(cmd_buf, sbuf.last_stage_mask | this->last_stage_mask, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0,
                         nullptr, 1, &buf_barrier, 1, &img_barrier);

    VkBufferImageCopy regions[16] = {};
    int regions_count = 0;

    uintptr_t data_off = 0;
    for (uint32_t i = 0; i < header.dwMipMapCount; i++) {
        const uint32_t len = ((w + 3) / 4) * ((h + 3) / 4) * block_size_bytes;
        if (len > bytes_left) {
            log->Error("Insufficient data length, bytes left %i, expected %i", bytes_left, len);
            return;
        }

        VkBufferImageCopy &reg = regions[regions_count++];

        reg.bufferOffset = VkDeviceSize(data_off);
        reg.bufferRowLength = 0;
        reg.bufferImageHeight = 0;

        reg.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        reg.imageSubresource.mipLevel = i;
        reg.imageSubresource.baseArrayLayer = 0;
        reg.imageSubresource.layerCount = 1;

        reg.imageOffset = {0, 0, 0};
        reg.imageExtent = {uint32_t(w), uint32_t(h), 1};

        initialized_mips_ |= (1u << i);

        data_off += len;
        bytes_left -= len;
        w = std::max(w / 2, 1);
        h = std::max(h / 2, 1);
    }

    vkCmdCopyBufferToImage(cmd_buf, sbuf.handle().buf, handle_.img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, regions_count,
                           regions);

    sbuf.last_access_mask = VK_ACCESS_TRANSFER_READ_BIT;
    sbuf.last_stage_mask = VK_PIPELINE_STAGE_TRANSFER_BIT;

    this->layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    this->last_access_mask = VK_ACCESS_TRANSFER_WRITE_BIT;
    this->last_stage_mask = VK_PIPELINE_STAGE_TRANSFER_BIT;

    ApplySampling(p.sampling, log);
}

void Ren::Texture2D::InitFromPNGFile(const void *data, const int size, Buffer &sbuf, void *_cmd_buf,
                                     MemoryAllocators *mem_allocs, const Tex2DParams &p, ILog *log) {
    int width, height, channels;
    unsigned char *const image_data = stbi_load_from_memory((const uint8_t *)data, size, &width, &height, &channels, 0);

    Tex2DParams _p = p;
    _p.w = width;
    _p.h = height;
    if (channels == 3) {
        _p.format = Ren::eTexFormat::RawRGB888;
    } else if (channels == 4) {
        _p.format = Ren::eTexFormat::RawRGBA8888;
    } else {
        assert(false);
    }

    const uint32_t img_size = channels * width * height;
    assert(img_size <= sbuf.size());
    uint8_t *stage_data = sbuf.Map(BufMapWrite);
    memcpy(stage_data, image_data, img_size);
    sbuf.FlushMappedRange(0, img_size);
    sbuf.Unmap();

    InitFromRAWData(&sbuf, 0, _cmd_buf, mem_allocs, _p, log);
    free(image_data);
}

void Ren::Texture2D::InitFromKTXFile(const void *data, const int size, Buffer &sbuf, void *_cmd_buf,
                                     MemoryAllocators *mem_allocs, const Tex2DParams &p, ILog *log) {
    KTXHeader header;
    memcpy(&header, data, sizeof(KTXHeader));

    eTexBlock block;
    bool is_srgb_format;
    eTexFormat format = FormatFromGLInternalFormat(header.gl_internal_format, &block, &is_srgb_format);

    if (is_srgb_format && (params_.flags & TexSRGB) == 0) {
        log->Warning("Loading SRGB texture as non-SRGB!");
    }

    Free();
    Realloc(int(header.pixel_width), int(header.pixel_height), int(header.mipmap_levels_count), 1, format, block,
            (p.flags & TexSRGB) != 0, nullptr, nullptr, log);

    params_.flags = p.flags;
    params_.block = block;
    params_.sampling = p.sampling;

    int w = int(params_.w);
    int h = int(params_.h);

    params_.w = w;
    params_.h = h;

    const auto *_data = (const uint8_t *)data;
    int data_offset = sizeof(KTXHeader);

    assert(uint32_t(size - data_offset) <= sbuf.size());
    uint8_t *stage_data = sbuf.Map(BufMapWrite);
    memcpy(stage_data, _data, size);
    sbuf.FlushMappedRange(0, size);
    sbuf.Unmap();

    assert(sbuf.type() == eBufType::Stage);
    VkCommandBuffer cmd_buf = reinterpret_cast<VkCommandBuffer>(_cmd_buf);

    VkBufferMemoryBarrier buf_barrier = {};
    buf_barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    buf_barrier.srcAccessMask = sbuf.last_access_mask;
    buf_barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    buf_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    buf_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    buf_barrier.buffer = sbuf.handle().buf;
    buf_barrier.offset = 0;
    buf_barrier.size = VkDeviceSize(size);

    VkImageMemoryBarrier img_barrier = {};
    img_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    img_barrier.srcAccessMask = this->last_access_mask;
    img_barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    img_barrier.oldLayout = this->layout;
    img_barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    img_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    img_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    img_barrier.image = handle_.img;
    img_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    img_barrier.subresourceRange.baseMipLevel = 0;
    img_barrier.subresourceRange.levelCount = params_.mip_count; // transit the whole image
    img_barrier.subresourceRange.baseArrayLayer = 0;
    img_barrier.subresourceRange.layerCount = 1;

    vkCmdPipelineBarrier(cmd_buf, sbuf.last_stage_mask | this->last_stage_mask, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0,
                         nullptr, 1, &buf_barrier, 1, &img_barrier);

    this->layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

    VkBufferImageCopy regions[16] = {};
    int regions_count = 0;

    for (int i = 0; i < int(header.mipmap_levels_count); i++) {
        if (data_offset + int(sizeof(uint32_t)) > size) {
            log->Error("Insufficient data length, bytes left %i, expected %i", size - data_offset, sizeof(uint32_t));
            break;
        }

        uint32_t img_size;
        memcpy(&img_size, &_data[data_offset], sizeof(uint32_t));
        if (data_offset + int(img_size) > size) {
            log->Error("Insufficient data length, bytes left %i, expected %i", size - data_offset, img_size);
            break;
        }

        data_offset += sizeof(uint32_t);

        VkBufferImageCopy &reg = regions[regions_count++];

        reg.bufferOffset = VkDeviceSize(data_offset);
        reg.bufferRowLength = 0;
        reg.bufferImageHeight = 0;

        reg.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        reg.imageSubresource.mipLevel = i;
        reg.imageSubresource.baseArrayLayer = 0;
        reg.imageSubresource.layerCount = 1;

        reg.imageOffset = {0, 0, 0};
        reg.imageExtent = {uint32_t(w), uint32_t(h), 1};

        initialized_mips_ |= (1u << i);
        data_offset += img_size;

        w = std::max(w / 2, 1);
        h = std::max(h / 2, 1);

        const int pad = (data_offset % 4) ? (4 - (data_offset % 4)) : 0;
        data_offset += pad;
    }

    vkCmdCopyBufferToImage(cmd_buf, sbuf.handle().buf, handle_.img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, regions_count,
                           regions);

    sbuf.last_access_mask = VK_ACCESS_TRANSFER_READ_BIT;
    sbuf.last_stage_mask = VK_PIPELINE_STAGE_TRANSFER_BIT;

    this->layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    this->last_access_mask = VK_ACCESS_TRANSFER_WRITE_BIT;
    this->last_stage_mask = VK_PIPELINE_STAGE_TRANSFER_BIT;

    ApplySampling(p.sampling, log);
}

void Ren::Texture2D::InitFromRAWData(Buffer &sbuf, int data_off[6], void *_cmd_buf, MemoryAllocators *mem_allocs,
                                     const Tex2DParams &p, ILog *log) {
    assert(p.w > 0 && p.h > 0);
    Free();

    handle_.generation = TextureHandleCounter++;
    params_ = p;
    initialized_mips_ = 0;

    const int mip_count = CalcMipCount(p.w, p.h, 1, p.sampling.filter);

    { // create image
        VkImageCreateInfo img_info = {};
        img_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        img_info.imageType = VK_IMAGE_TYPE_2D;
        img_info.extent.width = uint32_t(p.w);
        img_info.extent.height = uint32_t(p.h);
        img_info.extent.depth = 1;
        img_info.mipLevels = mip_count;
        img_info.arrayLayers = 1;
        img_info.format = g_vk_formats[size_t(p.format)];
        img_info.tiling = VK_IMAGE_TILING_OPTIMAL;
        img_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        img_info.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                         VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        img_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        img_info.samples = VkSampleCountFlagBits(p.samples);
        img_info.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

        VkResult res = vkCreateImage(api_ctx_->device, &img_info, nullptr, &handle_.img);
        if (res != VK_SUCCESS) {
            log->Error("Failed to create image!");
            return;
        }

#ifdef ENABLE_OBJ_LABELS
        VkDebugUtilsObjectNameInfoEXT name_info = {};
        name_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
        name_info.objectType = VK_OBJECT_TYPE_IMAGE;
        name_info.objectHandle = uint64_t(handle_.img);
        name_info.pObjectName = name_.c_str();
        vkSetDebugUtilsObjectNameEXT(api_ctx_->device, &name_info);
#endif

        VkMemoryRequirements tex_mem_req;
        vkGetImageMemoryRequirements(api_ctx_->device, handle_.img, &tex_mem_req);

        alloc_ = mem_allocs->Allocate(
            uint32_t(tex_mem_req.size), uint32_t(tex_mem_req.alignment),
            FindMemoryType(&api_ctx_->mem_properties, tex_mem_req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT),
            name_.c_str());

        const VkDeviceSize aligned_offset = AlignTo(VkDeviceSize(alloc_.alloc_off), tex_mem_req.alignment);

        res = vkBindImageMemory(api_ctx_->device, handle_.img, alloc_.owner->mem(alloc_.block_ndx), aligned_offset);
        if (res != VK_SUCCESS) {
            log->Error("Failed to bind memory!");
            return;
        }
    }

    { // create default image view
        VkImageViewCreateInfo view_info = {};
        view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view_info.image = handle_.img;
        view_info.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
        view_info.format = g_vk_formats[size_t(p.format)];
        view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        view_info.subresourceRange.baseMipLevel = 0;
        view_info.subresourceRange.levelCount = mip_count;
        view_info.subresourceRange.baseArrayLayer = 0;
        view_info.subresourceRange.layerCount = 1;

        const VkResult res = vkCreateImageView(api_ctx_->device, &view_info, nullptr, &handle_.view);
        if (res != VK_SUCCESS) {
            log->Error("Failed to create image view!");
            return;
        }
    }

    assert(p.samples == 1);
    assert(sbuf.type() == eBufType::Stage);
    VkCommandBuffer cmd_buf = reinterpret_cast<VkCommandBuffer>(_cmd_buf);

    VkBufferMemoryBarrier stage_barrier = {};
    stage_barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    stage_barrier.srcAccessMask = sbuf.last_access_mask;
    stage_barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    stage_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    stage_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    stage_barrier.buffer = sbuf.handle().buf;
    stage_barrier.offset = VkDeviceSize(0);
    stage_barrier.size = VkDeviceSize(sbuf.size());

    VkImageMemoryBarrier img_barrier = {};
    img_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    img_barrier.srcAccessMask = this->last_access_mask;
    img_barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    img_barrier.oldLayout = this->layout;
    img_barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    img_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    img_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    img_barrier.image = handle_.img;
    img_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    img_barrier.subresourceRange.baseMipLevel = 0;
    img_barrier.subresourceRange.levelCount = mip_count; // transit whole image
    img_barrier.subresourceRange.baseArrayLayer = 0;
    img_barrier.subresourceRange.layerCount = 1;

    vkCmdPipelineBarrier(cmd_buf, sbuf.last_stage_mask | this->last_stage_mask, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0,
                         nullptr, 1, &stage_barrier, 1, &img_barrier);

    VkBufferImageCopy regions[6] = {};
    for (int i = 0; i < 6; i++) {
        regions[i].bufferOffset = VkDeviceSize(data_off[i]);
        regions[i].bufferRowLength = 0;
        regions[i].bufferImageHeight = 0;

        regions[i].imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        regions[i].imageSubresource.mipLevel = 0;
        regions[i].imageSubresource.baseArrayLayer = i;
        regions[i].imageSubresource.layerCount = 1;

        regions[i].imageOffset = {0, 0, 0};
        regions[i].imageExtent = {uint32_t(p.w), uint32_t(p.h), 1};
    }

    vkCmdCopyBufferToImage(cmd_buf, sbuf.handle().buf, handle_.img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 6, regions);

    sbuf.last_access_mask = VK_ACCESS_TRANSFER_READ_BIT;
    sbuf.last_stage_mask = VK_PIPELINE_STAGE_TRANSFER_BIT;

    this->layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    this->last_access_mask = VK_ACCESS_TRANSFER_WRITE_BIT;
    this->last_stage_mask = VK_PIPELINE_STAGE_TRANSFER_BIT;

    initialized_mips_ |= (1u << 0);

    ApplySampling(p.sampling, log);
}

void Ren::Texture2D::InitFromTGAFile(const void *data[6], Buffer &sbuf, void *_cmd_buf, MemoryAllocators *mem_allocs,
                                     const Tex2DParams &p, ILog *log) {
    int w = 0, h = 0;
    eTexFormat format = eTexFormat::Undefined;

    uint8_t *stage_data = sbuf.Map(BufMapWrite);
    uint32_t stage_off = 0;

    int data_off[6] = {-1, -1, -1, -1, -1, -1};

    for (int i = 0; i < 6; i++) {
        if (data[i]) {
            uint32_t data_size;
            const bool res1 = ReadTGAFile(data[i], w, h, format, nullptr, data_size);
            assert(res1);

            assert(stage_off + data_size < sbuf.size());
            const bool res2 = ReadTGAFile(data[i], w, h, format, &stage_data[stage_off], data_size);
            assert(res2);

            data_off[i] = int(stage_off);
            stage_off += data_size;
        }
    }

    sbuf.FlushMappedRange(0, stage_off);
    sbuf.Unmap();

    Tex2DParams _p = p;
    _p.w = w;
    _p.h = h;
    _p.format = format;

    InitFromRAWData(sbuf, data_off, _cmd_buf, mem_allocs, _p, log);
}

void Ren::Texture2D::InitFromTGA_RGBEFile(const void *data[6], Buffer &sbuf, void *_cmd_buf,
                                          MemoryAllocators *mem_allocs, const Tex2DParams &p, ILog *log) {
    int w = p.w, h = p.h;

    uint8_t *stage_data = sbuf.Map(BufMapWrite);
    uint32_t stage_off = 0;

    int data_off[6];

    for (int i = 0; i < 6; i++) {
        if (data[i]) {
            const uint32_t img_size = 3 * w * h * sizeof(uint16_t);
            assert(stage_off + img_size <= sbuf.size());
            ConvertRGBE_to_RGB16F((const uint8_t *)data[i], w, h, (uint16_t *)&stage_data[stage_off]);
            data_off[i] = int(stage_off);
            stage_off += img_size;
        } else {
            data_off[i] = -1;
        }
    }

    sbuf.FlushMappedRange(0, stage_off);
    sbuf.Unmap();

    Tex2DParams _p = p;
    _p.w = w;
    _p.h = h;
    _p.format = Ren::eTexFormat::RawRGB16F;

    InitFromRAWData(sbuf, data_off, _cmd_buf, mem_allocs, _p, log);
}

void Ren::Texture2D::InitFromPNGFile(const void *data[6], const int size[6], Buffer &sbuf, void *_cmd_buf,
                                     MemoryAllocators *mem_allocs, const Tex2DParams &p, ILog *log) {
    uint8_t *stage_data = sbuf.Map(BufMapWrite);
    uint32_t stage_off = 0;

    int data_off[6] = {-1, -1, -1, -1, -1, -1};

    int width, height, channels;
    for (int i = 0; i < 6; i++) {
        if (data[i]) {
            uint8_t *img_data = stbi_load_from_memory((const uint8_t *)data[i], size[i], &width, &height, &channels, 0);

            assert(stage_off + channels * width * height <= sbuf.size());
            memcpy(&stage_data[stage_off], img_data, channels * width * height);
            data_off[i] = int(stage_off);
            stage_off += channels * width * height;

            free(img_data);
        }
    }

    sbuf.FlushMappedRange(0, stage_off);
    sbuf.Unmap();

    Tex2DParams _p = p;
    _p.w = width;
    _p.h = height;
    if (channels == 3) {
        _p.format = Ren::eTexFormat::RawRGB888;
    } else if (channels == 4) {
        _p.format = Ren::eTexFormat::RawRGBA8888;
    } else {
        assert(false);
    }

    InitFromRAWData(sbuf, data_off, _cmd_buf, mem_allocs, _p, log);
}

void Ren::Texture2D::InitFromDDSFile(const void *data[6], const int size[6], Buffer &sbuf, void *_cmd_buf,
                                     MemoryAllocators *mem_allocs, const Tex2DParams &p, ILog *log) {
    assert(p.w > 0 && p.h > 0);
    Free();

    uint8_t *stage_data = sbuf.Map(BufMapWrite);
    uint32_t data_off[6] = {};
    uint32_t stage_len = 0;

    eTexFormat first_format = eTexFormat::None;
    uint32_t first_mip_count = 0;
    int first_block_size_bytes = 0;

    for (int i = 0; i < 6; ++i) {
        const DDSHeader *header = reinterpret_cast<const DDSHeader *>(data[i]);

        eTexFormat format;
        eTexBlock block;
        int block_size_bytes;
        const int px_format = int(header->sPixelFormat.dwFourCC >> 24u) - '0';
        switch (px_format) {
        case 1:
            format = eTexFormat::Compressed_DXT1;
            block = eTexBlock::_4x4;
            block_size_bytes = 8;
            break;
        case 3:
            format = eTexFormat::Compressed_DXT3;
            block = eTexBlock::_4x4;
            block_size_bytes = 16;
            break;
        case 5:
            format = eTexFormat::Compressed_DXT5;
            block = eTexBlock::_4x4;
            block_size_bytes = 16;
            break;
        default:
            log->Error("Unknow DDS pixel format %i", px_format);
            return;
        }

        if (i == 0) {
            first_format = format;
            first_mip_count = header->dwMipMapCount;
            first_block_size_bytes = block_size_bytes;
        } else {
            assert(format == first_format);
            assert(first_mip_count == header->dwMipMapCount);
            assert(block_size_bytes == first_block_size_bytes);
        }

        memcpy(stage_data + stage_len, data[i], size[i]);

        data_off[i] = stage_len;
        stage_len += size[i];
    }

    sbuf.FlushMappedRange(0, stage_len);
    sbuf.Unmap();

    handle_.generation = TextureHandleCounter++;
    params_ = p;
    params_.cube = 1;
    initialized_mips_ = 0;

    { // create image
        VkImageCreateInfo img_info = {};
        img_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        img_info.imageType = VK_IMAGE_TYPE_2D;
        img_info.extent.width = uint32_t(p.w);
        img_info.extent.height = uint32_t(p.h);
        img_info.extent.depth = 1;
        img_info.mipLevels = first_mip_count;
        img_info.arrayLayers = 6;
        img_info.format = g_vk_formats[size_t(first_format)];
        img_info.tiling = VK_IMAGE_TILING_OPTIMAL;
        img_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        img_info.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        img_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        img_info.samples = VK_SAMPLE_COUNT_1_BIT;
        img_info.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

        VkResult res = vkCreateImage(api_ctx_->device, &img_info, nullptr, &handle_.img);
        if (res != VK_SUCCESS) {
            log->Error("Failed to create image!");
            return;
        }

#ifdef ENABLE_OBJ_LABELS
        VkDebugUtilsObjectNameInfoEXT name_info = {};
        name_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
        name_info.objectType = VK_OBJECT_TYPE_IMAGE;
        name_info.objectHandle = uint64_t(handle_.img);
        name_info.pObjectName = name_.c_str();
        vkSetDebugUtilsObjectNameEXT(api_ctx_->device, &name_info);
#endif

        VkMemoryRequirements tex_mem_req;
        vkGetImageMemoryRequirements(api_ctx_->device, handle_.img, &tex_mem_req);

        alloc_ = mem_allocs->Allocate(
            uint32_t(tex_mem_req.size), uint32_t(tex_mem_req.alignment),
            FindMemoryType(&api_ctx_->mem_properties, tex_mem_req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT),
            name_.c_str());

        const VkDeviceSize aligned_offset = AlignTo(VkDeviceSize(alloc_.alloc_off), tex_mem_req.alignment);

        res = vkBindImageMemory(api_ctx_->device, handle_.img, alloc_.owner->mem(alloc_.block_ndx), aligned_offset);
        if (res != VK_SUCCESS) {
            log->Error("Failed to bind memory!");
            return;
        }
    }

    { // create default image view
        VkImageViewCreateInfo view_info = {};
        view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view_info.image = handle_.img;
        view_info.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
        view_info.format = g_vk_formats[size_t(p.format)];
        view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        view_info.subresourceRange.baseMipLevel = 0;
        view_info.subresourceRange.levelCount = first_mip_count;
        view_info.subresourceRange.baseArrayLayer = 0;
        view_info.subresourceRange.layerCount = 6;

        const VkResult res = vkCreateImageView(api_ctx_->device, &view_info, nullptr, &handle_.view);
        if (res != VK_SUCCESS) {
            log->Error("Failed to create image view!");
            return;
        }
    }

    assert(p.samples == 1);
    assert(sbuf.type() == eBufType::Stage);
    VkCommandBuffer cmd_buf = reinterpret_cast<VkCommandBuffer>(_cmd_buf);

    VkBufferMemoryBarrier stage_barrier = {};
    stage_barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    stage_barrier.srcAccessMask = sbuf.last_access_mask;
    stage_barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    stage_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    stage_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    stage_barrier.buffer = sbuf.handle().buf;
    stage_barrier.offset = VkDeviceSize(0);
    stage_barrier.size = VkDeviceSize(sbuf.size());

    VkImageMemoryBarrier img_barrier = {};
    img_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    img_barrier.srcAccessMask = this->last_access_mask;
    img_barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    img_barrier.oldLayout = this->layout;
    img_barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    img_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    img_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    img_barrier.image = handle_.img;
    img_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    img_barrier.subresourceRange.baseMipLevel = 0;
    img_barrier.subresourceRange.levelCount = first_mip_count; // transit whole image
    img_barrier.subresourceRange.baseArrayLayer = 0;
    img_barrier.subresourceRange.layerCount = 6;

    vkCmdPipelineBarrier(cmd_buf, sbuf.last_stage_mask | this->last_stage_mask, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0,
                         nullptr, 1, &stage_barrier, 1, &img_barrier);

    VkBufferImageCopy regions[6 * 16] = {};
    int regions_count = 0;

    for (int i = 0; i < 6; i++) {
        const auto *header = reinterpret_cast<const DDSHeader *>(data[i]);

        int offset = sizeof(DDSHeader);
        int data_len = size[i] - int(sizeof(DDSHeader));

        for (uint32_t j = 0; j < header->dwMipMapCount; j++) {
            const int width = std::max(int(header->dwWidth >> j), 1), height = std::max(int(header->dwHeight >> j), 1);

            const int image_len = ((width + 3) / 4) * ((height + 3) / 4) * first_block_size_bytes;
            if (image_len > data_len) {
                log->Error("Insufficient data length, bytes left %i, expected %i", data_len, image_len);
                break;
            }

            auto &reg = regions[regions_count++];

            reg.bufferOffset = VkDeviceSize(data_off[i] + offset);
            reg.bufferRowLength = 0;
            reg.bufferImageHeight = 0;

            reg.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            reg.imageSubresource.mipLevel = uint32_t(j);
            reg.imageSubresource.baseArrayLayer = i;
            reg.imageSubresource.layerCount = 1;

            reg.imageOffset = {0, 0, 0};
            reg.imageExtent = {uint32_t(width), uint32_t(height), 1};

            offset += image_len;
            data_len -= image_len;
        }
    }

    vkCmdCopyBufferToImage(cmd_buf, sbuf.handle().buf, handle_.img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, regions_count,
                           regions);

    sbuf.last_access_mask = VK_ACCESS_TRANSFER_READ_BIT;
    sbuf.last_stage_mask = VK_PIPELINE_STAGE_TRANSFER_BIT;

    this->layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    this->last_access_mask = VK_ACCESS_TRANSFER_WRITE_BIT;
    this->last_stage_mask = VK_PIPELINE_STAGE_TRANSFER_BIT;

    ApplySampling(p.sampling, log);
}

void Ren::Texture2D::InitFromKTXFile(const void *data[6], const int size[6], Buffer &sbuf, void *_cmd_buf,
                                     MemoryAllocators *mem_allocs, const Tex2DParams &p, ILog *log) {
    Free();

    const auto *first_header = reinterpret_cast<const KTXHeader *>(data[0]);

    uint8_t *stage_data = sbuf.Map(BufMapWrite);
    uint32_t data_off[6] = {};
    uint32_t stage_len = 0;

    for (int i = 0; i < 6; ++i) {
        const auto *_data = (const uint8_t *)data[i];
        const auto *this_header = reinterpret_cast<const KTXHeader *>(_data);

        // make sure all images have same properties
        if (this_header->pixel_width != first_header->pixel_width) {
            log->Error("Image width mismatch %i, expected %i", int(this_header->pixel_width),
                       int(first_header->pixel_width));
            continue;
        }
        if (this_header->pixel_height != first_header->pixel_height) {
            log->Error("Image height mismatch %i, expected %i", int(this_header->pixel_height),
                       int(first_header->pixel_height));
            continue;
        }
        if (this_header->gl_internal_format != first_header->gl_internal_format) {
            log->Error("Internal format mismatch %i, expected %i", int(this_header->gl_internal_format),
                       int(first_header->gl_internal_format));
            continue;
        }

        memcpy(stage_data + stage_len, _data, size[i]);

        data_off[i] = stage_len;
        stage_len += size[i];
    }

    sbuf.FlushMappedRange(0, stage_len);
    sbuf.Unmap();

    handle_.generation = TextureHandleCounter++;
    params_ = p;
    params_.cube = 1;
    initialized_mips_ = 0;

    bool is_srgb_format;
    params_.format = FormatFromGLInternalFormat(first_header->gl_internal_format, &params_.block, &is_srgb_format);

    if (is_srgb_format && (params_.flags & TexSRGB) == 0) {
        log->Warning("Loading SRGB texture as non-SRGB!");
    }

    { // create image
        VkImageCreateInfo img_info = {};
        img_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        img_info.imageType = VK_IMAGE_TYPE_2D;
        img_info.extent.width = uint32_t(p.w);
        img_info.extent.height = uint32_t(p.h);
        img_info.extent.depth = 1;
        img_info.mipLevels = first_header->mipmap_levels_count;
        img_info.arrayLayers = 6;
        img_info.format = g_vk_formats[size_t(params_.format)];
        img_info.tiling = VK_IMAGE_TILING_OPTIMAL;
        img_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        img_info.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        img_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        img_info.samples = VK_SAMPLE_COUNT_1_BIT;
        img_info.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

        VkResult res = vkCreateImage(api_ctx_->device, &img_info, nullptr, &handle_.img);
        if (res != VK_SUCCESS) {
            log->Error("Failed to create image!");
            return;
        }

#ifdef ENABLE_OBJ_LABELS
        VkDebugUtilsObjectNameInfoEXT name_info = {};
        name_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
        name_info.objectType = VK_OBJECT_TYPE_IMAGE;
        name_info.objectHandle = uint64_t(handle_.img);
        name_info.pObjectName = name_.c_str();
        vkSetDebugUtilsObjectNameEXT(api_ctx_->device, &name_info);
#endif

        VkMemoryRequirements tex_mem_req;
        vkGetImageMemoryRequirements(api_ctx_->device, handle_.img, &tex_mem_req);

        alloc_ = mem_allocs->Allocate(
            uint32_t(tex_mem_req.size), uint32_t(tex_mem_req.alignment),
            FindMemoryType(&api_ctx_->mem_properties, tex_mem_req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT),
            name_.c_str());

        const VkDeviceSize aligned_offset = AlignTo(VkDeviceSize(alloc_.alloc_off), tex_mem_req.alignment);

        res = vkBindImageMemory(api_ctx_->device, handle_.img, alloc_.owner->mem(alloc_.block_ndx), aligned_offset);
        if (res != VK_SUCCESS) {
            log->Error("Failed to bind memory!");
            return;
        }
    }

    { // create default image view
        VkImageViewCreateInfo view_info = {};
        view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view_info.image = handle_.img;
        view_info.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
        view_info.format = g_vk_formats[size_t(p.format)];
        view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        view_info.subresourceRange.baseMipLevel = 0;
        view_info.subresourceRange.levelCount = first_header->mipmap_levels_count;
        view_info.subresourceRange.baseArrayLayer = 0;
        view_info.subresourceRange.layerCount = 6;

        const VkResult res = vkCreateImageView(api_ctx_->device, &view_info, nullptr, &handle_.view);
        if (res != VK_SUCCESS) {
            log->Error("Failed to create image view!");
            return;
        }
    }

    assert(p.samples == 1);
    assert(sbuf.type() == eBufType::Stage);
    VkCommandBuffer cmd_buf = reinterpret_cast<VkCommandBuffer>(_cmd_buf);

    VkBufferMemoryBarrier stage_barrier = {};
    stage_barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    stage_barrier.srcAccessMask = sbuf.last_access_mask;
    stage_barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    stage_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    stage_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    stage_barrier.buffer = sbuf.handle().buf;
    stage_barrier.offset = VkDeviceSize(0);
    stage_barrier.size = VkDeviceSize(sbuf.size());

    VkImageMemoryBarrier img_barrier = {};
    img_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    img_barrier.srcAccessMask = this->last_access_mask;
    img_barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    img_barrier.oldLayout = this->layout;
    img_barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    img_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    img_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    img_barrier.image = handle_.img;
    img_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    img_barrier.subresourceRange.baseMipLevel = 0;
    img_barrier.subresourceRange.levelCount = first_header->mipmap_levels_count; // transit whole image
    img_barrier.subresourceRange.baseArrayLayer = 0;
    img_barrier.subresourceRange.layerCount = 6;

    vkCmdPipelineBarrier(cmd_buf, sbuf.last_stage_mask | this->last_stage_mask, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0,
                         nullptr, 1, &stage_barrier, 1, &img_barrier);

    VkBufferImageCopy regions[6 * 16] = {};
    int regions_count = 0;

    for (int i = 0; i < 6; ++i) {
        const auto *_data = (const uint8_t *)data[i];

#ifndef NDEBUG
        const auto *this_header = reinterpret_cast<const KTXHeader *>(data[i]);

        // make sure all images have same properties
        if (this_header->pixel_width != first_header->pixel_width) {
            log->Error("Image width mismatch %i, expected %i", int(this_header->pixel_width),
                       int(first_header->pixel_width));
            continue;
        }
        if (this_header->pixel_height != first_header->pixel_height) {
            log->Error("Image height mismatch %i, expected %i", int(this_header->pixel_height),
                       int(first_header->pixel_height));
            continue;
        }
        if (this_header->gl_internal_format != first_header->gl_internal_format) {
            log->Error("Internal format mismatch %i, expected %i", int(this_header->gl_internal_format),
                       int(first_header->gl_internal_format));
            continue;
        }
#endif
        int data_offset = sizeof(KTXHeader);
        int _w = params_.w, _h = params_.h;

        for (int j = 0; j < int(first_header->mipmap_levels_count); j++) {
            uint32_t img_size;
            memcpy(&img_size, &_data[data_offset], sizeof(uint32_t));
            data_offset += sizeof(uint32_t);

            auto &reg = regions[regions_count++];

            reg.bufferOffset = VkDeviceSize(data_off[i] + data_offset);
            reg.bufferRowLength = 0;
            reg.bufferImageHeight = 0;

            reg.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            reg.imageSubresource.mipLevel = uint32_t(j);
            reg.imageSubresource.baseArrayLayer = i;
            reg.imageSubresource.layerCount = 1;

            reg.imageOffset = {0, 0, 0};
            reg.imageExtent = {uint32_t(_w), uint32_t(_h), 1};

            data_offset += img_size;

            _w = std::max(_w / 2, 1);
            _h = std::max(_h / 2, 1);

            const int pad = (data_offset % 4) ? (4 - (data_offset % 4)) : 0;
            data_offset += pad;
        }
    }

    vkCmdCopyBufferToImage(cmd_buf, sbuf.handle().buf, handle_.img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, regions_count,
                           regions);

    sbuf.last_access_mask = VK_ACCESS_TRANSFER_READ_BIT;
    sbuf.last_stage_mask = VK_PIPELINE_STAGE_TRANSFER_BIT;

    this->layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    this->last_access_mask = VK_ACCESS_TRANSFER_WRITE_BIT;
    this->last_stage_mask = VK_PIPELINE_STAGE_TRANSFER_BIT;

    ApplySampling(p.sampling, log);
}

void Ren::Texture2D::SetSubImage(const int level, const int offsetx, const int offsety, const int sizex,
                                 const int sizey, const Ren::eTexFormat format, const void *data, const int data_len) {
    assert(format == params_.format);
    assert(params_.samples == 1);
    assert(offsetx >= 0 && offsetx + sizex <= std::max(params_.w >> level, 1));
    assert(offsety >= 0 && offsety + sizey <= std::max(params_.h >> level, 1));

#if 0
    if (IsCompressedFormat(format)) {
        ren_glCompressedTextureSubImage2D_Comp(
            GL_TEXTURE_2D, GLuint(handle_.id), GLint(level), GLint(offsetx),
            GLint(offsety), GLsizei(sizex), GLsizei(sizey),
            GLInternalFormatFromTexFormat(format, (params_.flags & TexSRGB) != 0),
            GLsizei(data_len), data);
    } else {
        ren_glTextureSubImage2D_Comp(GL_TEXTURE_2D, GLuint(handle_.id), level, offsetx,
                                     offsety, sizex, sizey, GLFormatFromTexFormat(format),
                                     GLTypeFromTexFormat(format), data);
    }
#endif

    if (offsetx == 0 && offsety == 0 && sizex == std::max(params_.w >> level, 1) &&
        sizey == std::max(params_.h >> level, 1)) {
        // consider this level initialized
        initialized_mips_ |= (1u << level);
    }
}

void Ren::Texture2D::SetSubImage(const int level, const int offsetx, const int offsety, const int sizex,
                                 const int sizey, const Ren::eTexFormat format, const Buffer &sbuf, void *_cmd_buf,
                                 const int data_off, const int data_len) {
    assert(format == params_.format);
    assert(params_.samples == 1);
    assert(offsetx >= 0 && offsetx + sizex <= std::max(params_.w >> level, 1));
    assert(offsety >= 0 && offsety + sizey <= std::max(params_.h >> level, 1));

    assert(sbuf.type() == eBufType::Stage);
    VkCommandBuffer cmd_buf = reinterpret_cast<VkCommandBuffer>(_cmd_buf);

    VkBufferMemoryBarrier stage_barrier = {};
    stage_barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    stage_barrier.srcAccessMask = sbuf.last_access_mask;
    stage_barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    stage_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    stage_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    stage_barrier.buffer = sbuf.handle().buf;
    stage_barrier.offset = VkDeviceSize(0);
    stage_barrier.size = VkDeviceSize(sbuf.size());

    VkImageMemoryBarrier img_barrier = {};
    img_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    img_barrier.srcAccessMask = this->last_access_mask;
    img_barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    img_barrier.oldLayout = this->layout;
    img_barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    img_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    img_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    img_barrier.image = handle_.img;
    img_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    img_barrier.subresourceRange.baseMipLevel = 0;
    img_barrier.subresourceRange.levelCount = params_.mip_count; // transit whole image
    img_barrier.subresourceRange.baseArrayLayer = 0;
    img_barrier.subresourceRange.layerCount = 1;

    vkCmdPipelineBarrier(cmd_buf, sbuf.last_stage_mask | this->last_stage_mask, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0,
                         nullptr, 1, &stage_barrier, 1, &img_barrier);

    VkBufferImageCopy region = {};

    region.bufferOffset = VkDeviceSize(data_off);
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;

    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = uint32_t(level);
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;

    region.imageOffset = {int32_t(offsetx), int32_t(offsety), 0};
    region.imageExtent = {uint32_t(sizex), uint32_t(sizey), 1};

    vkCmdCopyBufferToImage(cmd_buf, sbuf.handle().buf, handle_.img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    sbuf.last_access_mask = VK_ACCESS_TRANSFER_READ_BIT;
    sbuf.last_stage_mask = VK_PIPELINE_STAGE_TRANSFER_BIT;

    this->layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    this->last_access_mask = VK_ACCESS_TRANSFER_WRITE_BIT;
    this->last_stage_mask = VK_PIPELINE_STAGE_TRANSFER_BIT;

    if (offsetx == 0 && offsety == 0 && sizex == std::max(params_.w >> level, 1) &&
        sizey == std::max(params_.h >> level, 1)) {
        // consider this level initialized
        initialized_mips_ |= (1u << level);
    }
}

void Ren::Texture2D::SetSampling(const SamplingParams params) {
    if (handle_.sampler) {
        api_ctx_->samplers_to_destroy->emplace_back(handle_.sampler);
    }

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

    const VkResult res = vkCreateSampler(api_ctx_->device, &sampler_info, nullptr, &handle_.sampler);
    assert(res == VK_SUCCESS && "Failed to create sampler!");

    params_.sampling = params;
}


void Ren::Texture2D::DownloadTextureData(const eTexFormat format, void *out_data) const {
#if defined(__ANDROID__)
#else
#if 0
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, GLuint(handle_.id));

    if (format == eTexFormat::RawRGBA8888) {
        glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, out_data);
    }

    glBindTexture(GL_TEXTURE_2D, 0);
#endif
#endif
}

////////////////////////////////////////////////////////////////////////////////////////

Ren::Texture1D::Texture1D(const char *name, BufferRef buf, const eTexFormat format, const uint32_t offset,
                          const uint32_t size, ILog *log)
    : name_(name) {
    Init(std::move(buf), format, offset, size, log);
}

Ren::Texture1D::~Texture1D() { Free(); }

Ren::Texture1D &Ren::Texture1D::operator=(Texture1D &&rhs) noexcept {
    if (this == &rhs) {
        return (*this);
    }

    RefCounter::operator=(static_cast<RefCounter &&>(rhs));

    Free();

    handle_ = exchange(rhs.handle_, {});
    buf_ = std::move(rhs.buf_);
    params_ = exchange(rhs.params_, {});
    name_ = std::move(rhs.name_);
    buf_view_ = exchange(rhs.buf_view_, {});

    return (*this);
}

void Ren::Texture1D::Init(BufferRef buf, const eTexFormat format, const uint32_t offset, const uint32_t size,
                          ILog *log) {
    Free();

    VkBufferViewCreateInfo view_info = {};
    view_info.sType = VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO;
    view_info.buffer = buf->handle().buf;
    view_info.format = g_vk_formats[size_t(format)];
    view_info.offset = VkDeviceSize(offset);
    view_info.range = VkDeviceSize(size);

    const VkResult res = vkCreateBufferView(buf->api_ctx()->device, &view_info, nullptr, &buf_view_);
    assert(res == VK_SUCCESS);

    handle_.generation = TextureHandleCounter++;
    buf_ = std::move(buf);
    params_.offset = offset;
    params_.size = size;
    params_.format = format;
}

void Ren::Texture1D::Free() {
    if (params_.format != eTexFormat::Undefined) {
        assert(IsMainThread());
        // auto tex_id = GLuint(handle_.id);
        // glDeleteTextures(1, &tex_id);
        handle_ = {};
    }
    if (buf_) {
        buf_->api_ctx()->buf_views_to_destroy[buf_->api_ctx()->backend_frame].push_back(buf_view_);
        buf_view_ = {};
        buf_ = {};
    }
}

VkFormat Ren::VKFormatFromTexFormat(eTexFormat format) { return g_vk_formats[size_t(format)]; }

#ifdef _MSC_VER
#pragma warning(pop)
#endif
