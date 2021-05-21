#include "TextureGL.h"

#include <memory>

#include "SOIL2/SOIL2.h"
#undef min
#undef max

#include "GL.h"
#include "Utils.h"

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4996)
#endif

#ifndef NDEBUG
//#define TEX_VERBOSE_LOGGING
#endif

namespace Ren {
const uint32_t g_gl_formats[] = {
    0xffffffff,                  // Undefined
    GL_RGB,                      // RawRGB888
    GL_RGBA,                     // RawRGBA8888
    GL_RGBA,                     // RawSignedRGBA8888
    GL_RED,                      // RawR32F
    GL_RED,                      // RawR16F
    GL_RED,                      // RawR8
    GL_RG,                       // RawRG88
    GL_RGB,                      // RawRGB32F
    GL_RGBA,                     // RawRGBA32F
    0xffffffff,                  // RawRGBE8888
    GL_RGB,                      // RawRGB16F
    GL_RGBA,                     // RawRGBA16F
    GL_RG,                       // RawRG16
    GL_RG,                       // RawRG16U
    GL_RG,                       // RawRG16F
    GL_RG,                       // RawRG32F
    GL_RG,                       // RawRG32U
    GL_RGBA,                     // RawRGB10_A2
    GL_RGB,                      // RawRG11F_B10F
    GL_DEPTH_COMPONENT,          // Depth16
    GL_DEPTH_STENCIL_ATTACHMENT, // Depth24Stencil8
#ifndef __ANDROID__
    GL_DEPTH_COMPONENT, // Depth32
#endif
    0xffffffff, // Compressed_DXT1
    0xffffffff, // Compressed_DXT3
    0xffffffff, // Compressed_DXT5
    0xffffffff, // Compressed_ASTC
    0xffffffff, // None
};
static_assert(sizeof(g_gl_formats) / sizeof(g_gl_formats[0]) ==
                  size_t(eTexFormat::_Count),
              "!");

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

const uint32_t g_gl_min_filter[] = {
    GL_NEAREST,               // NoFilter
    GL_LINEAR_MIPMAP_NEAREST, // Bilinear
    GL_LINEAR_MIPMAP_LINEAR,  // Trilinear
    GL_LINEAR,                // BilinearNoMipmap
};
static_assert(sizeof(g_gl_min_filter) / sizeof(g_gl_min_filter[0]) ==
                  size_t(eTexFilter::_Count),
              "!");

const uint32_t g_gl_mag_filter[] = {
    GL_NEAREST, // NoFilter
    GL_LINEAR,  // Bilinear
    GL_LINEAR,  // Trilinear
    GL_LINEAR,  // BilinearNoMipmap
};
static_assert(sizeof(g_gl_mag_filter) / sizeof(g_gl_mag_filter[0]) ==
                  size_t(eTexFilter::_Count),
              "!");

const uint32_t g_gl_wrap_mode[] = {
    GL_REPEAT,          // Repeat
    GL_CLAMP_TO_EDGE,   // ClampToEdge
    GL_CLAMP_TO_BORDER, // ClampToBorder
};
static_assert(sizeof(g_gl_wrap_mode) / sizeof(g_gl_wrap_mode[0]) ==
                  size_t(eTexRepeat::WrapModesCount),
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

const float AnisotropyLevel = 4.0f;
uint32_t TextureHandleCounter = 0;

bool IsMainThread();

GLenum ToSRGBFormat(const GLenum internal_format) {
    switch (internal_format) {
    case GL_RGB8:
        return GL_SRGB8;
    case GL_RGBA8:
        return GL_SRGB8_ALPHA8;
    case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
        return GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT;
    case GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:
        return GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT;
    case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
        return GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT;
    case GL_COMPRESSED_RGBA_ASTC_4x4_KHR:
        return GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR;
    case GL_COMPRESSED_RGBA_ASTC_5x4_KHR:
        return GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x4_KHR;
    case GL_COMPRESSED_RGBA_ASTC_5x5_KHR:
        return GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x5_KHR;
    case GL_COMPRESSED_RGBA_ASTC_6x5_KHR:
        return GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x5_KHR;
    case GL_COMPRESSED_RGBA_ASTC_6x6_KHR:
        return GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6_KHR;
    case GL_COMPRESSED_RGBA_ASTC_8x5_KHR:
        return GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x5_KHR;
    case GL_COMPRESSED_RGBA_ASTC_8x6_KHR:
        return GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x6_KHR;
    case GL_COMPRESSED_RGBA_ASTC_8x8_KHR:
        return GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x8_KHR;
    case GL_COMPRESSED_RGBA_ASTC_10x5_KHR:
        return GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x5_KHR;
    case GL_COMPRESSED_RGBA_ASTC_10x6_KHR:
        return GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x6_KHR;
    case GL_COMPRESSED_RGBA_ASTC_10x8_KHR:
        return GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x8_KHR;
    case GL_COMPRESSED_RGBA_ASTC_10x10_KHR:
        return GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x10_KHR;
    case GL_COMPRESSED_RGBA_ASTC_12x10_KHR:
        return GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x10_KHR;
    case GL_COMPRESSED_RGBA_ASTC_12x12_KHR:
        return GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x12_KHR;
    default:
        assert(false && "Unsupported format!");
    }

    return 0xffffffff;
}

bool IsDepthFormat(const eTexFormat format) {
    return format == eTexFormat::Depth16 || format == eTexFormat::Depth24Stencil8
#ifndef __ANDROID__
           || format == eTexFormat::Depth32;
#else
        ;
#endif
}
} // namespace Ren

static_assert(sizeof(GLsync) == sizeof(void *), "!");

Ren::SyncFence::~SyncFence() {
    if (sync_) {
        auto sync = reinterpret_cast<GLsync>(exchange(sync_, nullptr));
        glDeleteSync(sync);
    }
}

Ren::SyncFence::SyncFence(SyncFence &&rhs) { sync_ = exchange(rhs.sync_, nullptr); }

Ren::SyncFence &Ren::SyncFence::operator=(SyncFence &&rhs) {
    if (sync_) {
        auto sync = reinterpret_cast<GLsync>(exchange(sync_, nullptr));
        glDeleteSync(sync);
    }
    sync_ = exchange(rhs.sync_, nullptr);
    return (*this);
}

void Ren::SyncFence::WaitSync() {
    assert(sync_);
    glWaitSync(reinterpret_cast<GLsync>(sync_), 0, GL_TIMEOUT_IGNORED);
}

Ren::SyncFence::WaitResult Ren::SyncFence::ClientWaitSync(const uint64_t timeout_us) {
    assert(sync_);
    auto sync = reinterpret_cast<GLsync>(sync_);
    const GLenum res = glClientWaitSync(sync, 0, timeout_us);

    WaitResult ret = WaitResult::WaitFailed;
    if (res == GL_ALREADY_SIGNALED) {
        ret = WaitResult::AlreadySignaled;
    } else if (res == GL_TIMEOUT_EXPIRED) {
        ret = WaitResult::TimeoutExpired;
    } else if (res == GL_CONDITION_SATISFIED) {
        ret = WaitResult::ConditionSatisfied;
    }

    if (ret != WaitResult::TimeoutExpired) {
        glDeleteSync(sync);
        sync_ = nullptr;
    }

    return ret;
}

Ren::SyncFence Ren::MakeFence() {
    return SyncFence{glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0)};
}

////////////////////////////////////////////////////////////////////////////////////////

Ren::Texture2D::Texture2D(const char *name, const Tex2DParams &p, ILog *log)
    : name_(name) {
    Init(p, log);
}

Ren::Texture2D::Texture2D(const char *name, const void *data, const int size,
                          const Tex2DParams &p, eTexLoadStatus *load_status, ILog *log)
    : name_(name) {
    Init(data, size, p, load_status, log);
}

Ren::Texture2D::Texture2D(const char *name, const void *data[6], const int size[6],
                          const Tex2DParams &p, eTexLoadStatus *load_status, ILog *log)
    : name_(name) {
    Init(data, size, p, load_status, log);
}

Ren::Texture2D::~Texture2D() { Free(); }

Ren::Texture2D &Ren::Texture2D::operator=(Ren::Texture2D &&rhs) noexcept {
    if (this == &rhs) {
        return (*this);
    }

    Free();

    handle_ = exchange(rhs.handle_, {});
    params_ = exchange(rhs.params_, {});
    ready_ = exchange(rhs.ready_, false);
    cubemap_ready_ = exchange(rhs.cubemap_ready_, 0);
    name_ = std::move(rhs.name_);

    RefCounter::operator=(std::move(rhs));

    return (*this);
}

void Ren::Texture2D::Init(const Tex2DParams &params, ILog *log) {
    assert(IsMainThread());
    const void *null = nullptr;
    InitFromRAWData(null, params, log);
    ready_ = true;
}

void Ren::Texture2D::Init(const void *data, int size, const Tex2DParams &p,
                          eTexLoadStatus *load_status, ILog *log) {
    assert(IsMainThread());
    if (!data) {
        Tex2DParams _p = p;
        _p.w = _p.h = 1;
        _p.mip_count = 1;
        _p.format = eTexFormat::RawRGBA8888;
        InitFromRAWData(p.fallback_color, _p, log);
        // mark it as not ready
        ready_ = false;
        (*load_status) = eTexLoadStatus::TexCreatedDefault;
    } else {
        if (name_.EndsWith(".tga_rgbe") != 0 || name_.EndsWith(".TGA_RGBE") != 0) {
            InitFromTGA_RGBEFile(data, p, log);
        } else if (name_.EndsWith(".tga") != 0 || name_.EndsWith(".TGA") != 0) {
            InitFromTGAFile(data, p, log);
        } else if (name_.EndsWith(".dds") != 0 || name_.EndsWith(".DDS") != 0) {
            InitFromDDSFile(data, size, p, log);
        } else if (name_.EndsWith(".ktx") != 0 || name_.EndsWith(".KTX") != 0) {
            InitFromKTXFile(data, size, p, log);
        } else if (name_.EndsWith(".png") != 0 || name_.EndsWith(".PNG") != 0) {
            InitFromPNGFile(data, size, p, log);
        } else {
            InitFromRAWData(data, p, log);
        }
        ready_ = true;
        (*load_status) = eTexLoadStatus::TexCreatedFromData;
    }
}

void Ren::Texture2D::Init(const void *data[6], const int size[6], const Tex2DParams &p,
                          eTexLoadStatus *load_status, ILog *log) {
    assert(IsMainThread());
    if (!data) {
        const void *_data[6] = {p.fallback_color, p.fallback_color, p.fallback_color,
                                p.fallback_color, p.fallback_color, p.fallback_color};
        Tex2DParams _p = p;
        _p.w = _p.h = 1;
        _p.format = eTexFormat::RawRGBA8888;
        InitFromRAWData(_data, _p, log);
        // mark it as not ready
        ready_ = false;
        cubemap_ready_ = 0;
        (*load_status) = eTexLoadStatus::TexCreatedDefault;
    } else {
        if (name_.EndsWith(".tga_rgbe") != 0 || name_.EndsWith(".TGA_RGBE") != 0) {
            InitFromTGA_RGBEFile(data, p, log);
        } else if (name_.EndsWith(".tga") != 0 || name_.EndsWith(".TGA") != 0) {
            InitFromTGAFile(data, p, log);
        } else if (name_.EndsWith(".png") != 0 || name_.EndsWith(".PNG") != 0) {
            InitFromPNGFile(data, size, p, log);
        } else if (name_.EndsWith(".ktx") != 0 || name_.EndsWith(".KTX") != 0) {
            InitFromKTXFile(data, size, p, log);
        } else if (name_.EndsWith(".dds") != 0 || name_.EndsWith(".DDS") != 0) {
            InitFromDDSFile(data, size, p, log);
        } else {
            InitFromRAWData(data, p, log);
        }

        ready_ = (cubemap_ready_ & (1u << 0u)) == 1;
        for (unsigned i = 1; i < 6; i++) {
            ready_ = ready_ && ((cubemap_ready_ & (1u << i)) == 1);
        }
        (*load_status) = eTexLoadStatus::TexCreatedFromData;
    }
}

void Ren::Texture2D::Free() {
    if (params_.format != eTexFormat::Undefined && !(params_.flags & TexNoOwnership)) {
        assert(IsMainThread());
        auto tex_id = GLuint(handle_.id);
        glDeleteTextures(1, &tex_id);
        handle_ = {};
        params_.format = eTexFormat::Undefined;
    }
}

void Ren::Texture2D::Realloc(const int w, const int h, int mip_count, const int samples,
                             const Ren::eTexFormat format, const bool is_srgb,
                             ILog *log) {
    GLuint tex_id;
    glCreateTextures(samples > 1 ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D, 1, &tex_id);
#ifdef ENABLE_OBJ_LABELS
    glObjectLabel(GL_TEXTURE, tex_id, -1, name_.c_str());
#endif
    const GLuint internal_format = GLInternalFormatFromTexFormat(format, is_srgb);

    mip_count = std::min(mip_count, CalcMipCount(w, h, 1, Ren::eTexFilter::Trilinear));

    // allocate all mip levels
    ren_glTextureStorage2D_Comp(GL_TEXTURE_2D, tex_id, GLsizei(mip_count),
                                internal_format, GLsizei(w), GLsizei(h));
#ifdef TEX_VERBOSE_LOGGING
    if (params_.format != eTexFormat::Undefined) {
        log->Info("Realloc %s, %ix%i (%i mips) -> %ix%i (%i mips)", name_.c_str(),
                  int(params_.w), int(params_.h), int(params_.mip_count), w, h,
                  mip_count);
    } else {
        log->Info("Alloc %s %ix%i (%i mips)", name_.c_str(), w, h, mip_count);
    }
#endif

    const TexHandle new_handle = {tex_id, TextureHandleCounter++};
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

        for (; src_mip < int(params_.mip_count) && dst_mip < mip_count;
             ++src_mip, ++dst_mip) {
            if (initialized_mips_ & (1u << src_mip)) {
                glCopyImageSubData(GLuint(handle_.id), GL_TEXTURE_2D, GLint(src_mip), 0,
                                   0, 0, GLuint(new_handle.id), GL_TEXTURE_2D,
                                   GLint(dst_mip), 0, 0, 0,
                                   GLsizei(std::max(params_.w >> src_mip, 1)),
                                   GLsizei(std::max(params_.h >> src_mip, 1)), 1);
#ifdef TEX_VERBOSE_LOGGING
                log->Info("Copying data mip %i [old] -> mip %i [new]", src_mip, dst_mip);
#endif

                new_initialized_mips |= (1u << dst_mip);
            }
        }
    }
    Free();

    handle_ = new_handle;
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
    initialized_mips_ = new_initialized_mips;
}

void Ren::Texture2D::InitFromRAWData(const void *data, const Tex2DParams &p, ILog *log) {
    Free();

    GLuint tex_id;
    glCreateTextures(p.samples > 1 ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D, 1,
                     &tex_id);
#ifdef ENABLE_OBJ_LABELS
    glObjectLabel(GL_TEXTURE, tex_id, -1, name_.c_str());
#endif
    handle_ = {tex_id, TextureHandleCounter++};

    params_ = p;
    initialized_mips_ = 0;

    const auto format = (GLenum)GLFormatFromTexFormat(p.format),
               internal_format = (GLenum)GLInternalFormatFromTexFormat(
                   p.format, (p.flags & eTexFlags::TexSRGB) != 0),
               type = (GLenum)GLTypeFromTexFormat(p.format);

    auto mip_count = (GLsizei)CalcMipCount(p.w, p.h, 1, p.sampling.filter);

    if (format != 0xffffffff && internal_format != 0xffffffff && type != 0xffffffff) {
        if (p.flags & TexMutable) {
            glBindTexture(GL_TEXTURE_2D, tex_id);
            for (int i = 0; i < mip_count; i++) {
                const int w = std::max(p.w << i, 1);
                const int h = std::max(p.h << i, 1);
                glTexImage2D(GL_TEXTURE_2D, GLint(i), internal_format, w, h, 0, format,
                             type, (i == 0) ? data : nullptr);
                initialized_mips_ |= (1u << i);
            }
        } else {
            if (p.samples > 1) {
                glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, tex_id);
                glTexStorage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, GLsizei(p.samples),
                                          internal_format, GLsizei(p.w), GLsizei(p.h),
                                          GL_TRUE);
                initialized_mips_ |= (1u << 0);
            } else {
                // allocate all mip levels
                ren_glTextureStorage2D_Comp(GL_TEXTURE_2D, tex_id, mip_count,
                                            internal_format, GLsizei(p.w), GLsizei(p.h));
                if (data) {
                    // update first level
                    ren_glTextureSubImage2D_Comp(GL_TEXTURE_2D, tex_id, 0, 0, 0, p.w, p.h,
                                                 format, type, data);
                    initialized_mips_ |= (1u << 0);
                }
            }
        }
    }

    if (p.samples == 1) {
        SetFilter(p.sampling, log);
    }

    CheckError("create texture", log);
}

void Ren::Texture2D::InitFromTGAFile(const void *data, const Tex2DParams &p, ILog *log) {
    int w = 0, h = 0;
    eTexFormat format = eTexFormat::Undefined;
    const std::unique_ptr<uint8_t[]> image_data = ReadTGAFile(data, w, h, format);

    Tex2DParams _p = p;
    _p.w = w;
    _p.h = h;
    _p.format = format;

    InitFromRAWData(image_data.get(), _p, log);
}

void Ren::Texture2D::InitFromTGA_RGBEFile(const void *data, const Tex2DParams &p,
                                          ILog *log) {
    int w = 0, h = 0;
    eTexFormat format = eTexFormat::Undefined;
    std::unique_ptr<uint8_t[]> image_data = ReadTGAFile(data, w, h, format);

    std::unique_ptr<uint16_t[]> fp_data = ConvertRGBE_to_RGB16F(image_data.get(), w, h);

    Tex2DParams _p = p;
    _p.w = w;
    _p.h = h;
    _p.format = eTexFormat::RawRGB16F;

    InitFromRAWData(fp_data.get(), _p, log);
}

void Ren::Texture2D::InitFromDDSFile(const void *data, const int size,
                                     const Tex2DParams &p, ILog *log) {
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
    Realloc(int(header.dwWidth), int(header.dwHeight), int(header.dwMipMapCount), 1,
            format, (p.flags & TexSRGB) != 0, log);

    params_.flags = p.flags;
    params_.block = block;
    params_.sampling = p.sampling;

    const GLuint internal_format =
        GLInternalFormatFromTexFormat(params_.format, (p.flags & TexSRGB) != 0);

    int w = params_.w, h = params_.h;
    int bytes_left = size - int(sizeof(DDSHeader));
    const uint8_t *p_data = (uint8_t *)data + sizeof(DDSHeader);

    for (uint32_t i = 0; i < header.dwMipMapCount; i++) {
        const int len = ((w + 3) / 4) * ((h + 3) / 4) * block_size_bytes;
        if (len > bytes_left) {
            log->Error("Insufficient data length, bytes left %i, expected %i", bytes_left,
                       len);
            return;
        }

        ren_glCompressedTextureSubImage2D_Comp(GL_TEXTURE_2D, GLuint(handle_.id),
                                               GLint(i), 0, 0, w, h, internal_format, len,
                                               p_data);
        initialized_mips_ |= (1u << i);

        p_data += len;
        bytes_left -= len;
        w = std::max(w / 2, 1);
        h = std::max(h / 2, 1);
    }

    SetFilter(p.sampling, log);
}

void Ren::Texture2D::InitFromPNGFile(const void *data, const int size,
                                     const Tex2DParams &p, ILog *log) {
    Free();

    GLuint tex_id;
    glCreateTextures(GL_TEXTURE_2D, 1, &tex_id);
#ifdef ENABLE_OBJ_LABELS
    glObjectLabel(GL_TEXTURE, tex_id, -1, name_.c_str());
#endif

    handle_ = {tex_id, TextureHandleCounter++};
    params_ = p;
    initialized_mips_ = 0;

    const unsigned res = SOIL_load_OGL_texture_from_memory(
        (unsigned char *)data, size, SOIL_LOAD_AUTO, tex_id,
        SOIL_FLAG_INVERT_Y /*| SOIL_FLAG_GL_MIPMAPS*/);
    initialized_mips_ |= (1u << 0);
    assert(res == tex_id);

    GLint w, h;
    glBindTexture(GL_TEXTURE_2D, tex_id);
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &w);
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &h);

    // generate mip maps manually
    if (params_.flags & (eTexFlags::TexMIPMin | eTexFlags::TexMIPMax)) {
        int width, height, channels;
        uint8_t *const img_data = SOIL_load_image_from_memory(
            (unsigned char *)data, size, &width, &height, &channels, 0);

        std::unique_ptr<uint8_t[]> mipmaps[16];
        int widths[16] = {}, heights[16] = {};

        mipmaps[0].reset(new uint8_t[w * h * channels]);
        widths[0] = w;
        heights[0] = h;

        for (int y = 0; y < h; y++) {
            // flip y
            memcpy(&mipmaps[0][y * w * channels], &img_data[(h - y - 1) * w * channels],
                   w * channels);
        }

        // select mip operation
        const Ren::eMipOp op = (params_.flags & eTexFlags::TexMIPMin)
                                   ? Ren::eMipOp::MinBilinear
                                   : Ren::eMipOp::MaxBilinear;
        // use it for all channels
        const Ren::eMipOp ops[4] = {op, op, op, op};
        const int mip_count = InitMipMaps(mipmaps, widths, heights, 4, ops);

        for (int lod = 1; lod < mip_count; lod++) {
            glTexImage2D(GL_TEXTURE_2D, lod, (channels == 3 ? GL_RGB : GL_RGBA),
                         widths[lod], heights[lod], 0, (channels == 3 ? GL_RGB : GL_RGBA),
                         GL_UNSIGNED_BYTE, &mipmaps[lod][0]);
            initialized_mips_ |= (1u << lod);
        }

        SOIL_free_image_data(img_data);
    }

    params_.w = int(w);
    params_.h = int(h);

    SetFilter(p.sampling, log);
}

void Ren::Texture2D::InitFromKTXFile(const void *data, const int size,
                                     const Tex2DParams &p, ILog *log) {
    KTXHeader header;
    memcpy(&header, data, sizeof(KTXHeader));

    eTexBlock block;
    bool is_srgb_format;
    eTexFormat format =
        FormatFromGLInternalFormat(header.gl_internal_format, &block, &is_srgb_format);

    if (is_srgb_format && (params_.flags & TexSRGB) == 0) {
        log->Warning("Loading SRGB texture as non-SRGB!");
    }

    Free();
    Realloc(int(header.pixel_width), int(header.pixel_height),
            int(header.mipmap_levels_count), 1, format, (p.flags & TexSRGB) != 0, log);

    params_.flags = p.flags;
    params_.block = block;
    params_.sampling = p.sampling;

    int w = int(params_.w);
    int h = int(params_.h);

    params_.w = w;
    params_.h = h;

    const auto *_data = (const uint8_t *)data;
    int data_offset = sizeof(KTXHeader);

    for (int i = 0; i < int(header.mipmap_levels_count); i++) {
        if (data_offset + int(sizeof(uint32_t)) > size) {
            log->Error("Insufficient data length, bytes left %i, expected %i",
                       size - data_offset, sizeof(uint32_t));
            break;
        }

        uint32_t img_size;
        memcpy(&img_size, &_data[data_offset], sizeof(uint32_t));
        if (data_offset + int(img_size) > size) {
            log->Error("Insufficient data length, bytes left %i, expected %i",
                       size - data_offset, img_size);
            break;
        }

        data_offset += sizeof(uint32_t);

        ren_glCompressedTextureSubImage2D_Comp(
            GL_TEXTURE_2D, GLuint(handle_.id), i, 0, 0, w, h,
            GLInternalFormatFromTexFormat(params_.format, (params_.flags & TexSRGB) != 0),
            GLsizei(img_size), &_data[data_offset]);
        initialized_mips_ |= (1u << i);
        data_offset += img_size;

        w = std::max(w / 2, 1);
        h = std::max(h / 2, 1);

        const int pad = (data_offset % 4) ? (4 - (data_offset % 4)) : 0;
        data_offset += pad;
    }

    SetFilter(p.sampling, log);
}

void Ren::Texture2D::InitFromRAWData(const void *data[6], const Tex2DParams &p,
                                     ILog *log) {
    assert(p.w > 0 && p.h > 0);
    Free();

    GLuint tex_id;
    glCreateTextures(GL_TEXTURE_CUBE_MAP, 1, &tex_id);
#ifdef ENABLE_OBJ_LABELS
    glObjectLabel(GL_TEXTURE, tex_id, -1, name_.c_str());
#endif

    handle_ = {tex_id, TextureHandleCounter++};
    params_ = p;

    const auto format = (GLenum)GLFormatFromTexFormat(params_.format),
               internal_format = (GLenum)GLInternalFormatFromTexFormat(
                   params_.format, (p.flags & TexSRGB) != 0),
               type = (GLenum)GLTypeFromTexFormat(params_.format);

    const int w = p.w, h = p.h;
    const eTexFilter f = params_.sampling.filter;

    auto mip_count = (GLsizei)CalcMipCount(w, h, 1, f);

    // allocate all mip levels
    ren_glTextureStorage2D_Comp(GL_TEXTURE_CUBE_MAP, tex_id, mip_count, internal_format,
                                w, h);

    for (unsigned i = 0; i < 6; i++) {
        if (!data[i]) {
            continue;
        } else {
            cubemap_ready_ |= (1u << i);
        }

        if (format != 0xffffffff && internal_format != 0xffffffff && type != 0xffffffff) {
            ren_glTextureSubImage3D_Comp(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, tex_id, 0, 0,
                                         0, i, w, h, 1, format, type, data[i]);
        }
    }

    if (f == eTexFilter::NoFilter) {
        ren_glTextureParameteri_Comp(GL_TEXTURE_CUBE_MAP, tex_id, GL_TEXTURE_MIN_FILTER,
                                     GL_NEAREST);
        ren_glTextureParameteri_Comp(GL_TEXTURE_CUBE_MAP, tex_id, GL_TEXTURE_MAG_FILTER,
                                     GL_NEAREST);
    } else if (f == eTexFilter::Bilinear) {
        ren_glTextureParameteri_Comp(GL_TEXTURE_CUBE_MAP, tex_id, GL_TEXTURE_MIN_FILTER,
                                     (cubemap_ready_ == 0x3F) ? GL_LINEAR_MIPMAP_NEAREST
                                                              : GL_LINEAR);
        ren_glTextureParameteri_Comp(GL_TEXTURE_CUBE_MAP, tex_id, GL_TEXTURE_MAG_FILTER,
                                     GL_LINEAR);
    } else if (f == eTexFilter::Trilinear) {
        ren_glTextureParameteri_Comp(GL_TEXTURE_CUBE_MAP, tex_id, GL_TEXTURE_MIN_FILTER,
                                     (cubemap_ready_ == 0x3F) ? GL_LINEAR_MIPMAP_LINEAR
                                                              : GL_LINEAR);
        ren_glTextureParameteri_Comp(GL_TEXTURE_CUBE_MAP, tex_id, GL_TEXTURE_MAG_FILTER,
                                     GL_LINEAR);
    } else if (f == eTexFilter::BilinearNoMipmap) {
        ren_glTextureParameteri_Comp(GL_TEXTURE_CUBE_MAP, tex_id, GL_TEXTURE_MIN_FILTER,
                                     GL_LINEAR);
        ren_glTextureParameteri_Comp(GL_TEXTURE_CUBE_MAP, tex_id, GL_TEXTURE_MAG_FILTER,
                                     GL_LINEAR);
    }

    ren_glTextureParameteri_Comp(GL_TEXTURE_CUBE_MAP, tex_id, GL_TEXTURE_WRAP_S,
                                 GL_CLAMP_TO_EDGE);
    ren_glTextureParameteri_Comp(GL_TEXTURE_CUBE_MAP, tex_id, GL_TEXTURE_WRAP_T,
                                 GL_CLAMP_TO_EDGE);
#if !defined(GL_ES_VERSION_2_0) && !defined(__EMSCRIPTEN__)
    ren_glTextureParameteri_Comp(GL_TEXTURE_CUBE_MAP, tex_id, GL_TEXTURE_WRAP_R,
                                 GL_CLAMP_TO_EDGE);
#endif

    if ((f == eTexFilter::Trilinear || f == eTexFilter::Bilinear) &&
        (cubemap_ready_ == 0x3F)) {
        ren_glGenerateTextureMipmap_Comp(GL_TEXTURE_CUBE_MAP, tex_id);
    }
}

void Ren::Texture2D::InitFromTGAFile(const void *data[6], const Tex2DParams &p,
                                     ILog *log) {
    std::unique_ptr<uint8_t[]> image_data[6];
    const void *_image_data[6] = {};
    int w = 0, h = 0;
    eTexFormat format = eTexFormat::Undefined;
    for (int i = 0; i < 6; i++) {
        if (data[i]) {
            image_data[i] = ReadTGAFile(data[i], w, h, format);
            _image_data[i] = image_data[i].get();
        }
    }

    Tex2DParams _p = p;
    _p.w = w;
    _p.h = h;
    _p.format = format;

    InitFromRAWData(_image_data, _p, log);
}

void Ren::Texture2D::InitFromTGA_RGBEFile(const void *data[6], const Tex2DParams &p,
                                          ILog *log) {
    std::unique_ptr<uint16_t[]> image_data[6];
    const void *_image_data[6] = {};
    int w = p.w, h = p.h;
    for (int i = 0; i < 6; i++) {
        if (data[i]) {
            image_data[i] = ConvertRGBE_to_RGB16F((const uint8_t *)data[i], w, h);
            _image_data[i] = image_data[i].get();
        }
    }

    Tex2DParams _p = p;
    _p.w = w;
    _p.h = h;
    _p.format = Ren::eTexFormat::RawRGB16F;

    InitFromRAWData(_image_data, _p, log);
}

void Ren::Texture2D::InitFromPNGFile(const void *data[6], const int size[6],
                                     const Tex2DParams &p, ILog *log) {
    Free();

    GLuint tex_id;
    glCreateTextures(GL_TEXTURE_CUBE_MAP, 1, &tex_id);
#ifdef ENABLE_OBJ_LABELS
    glObjectLabel(GL_TEXTURE, tex_id, -1, name_.c_str());
#endif

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, tex_id);

    params_ = p;

    unsigned int handle = SOIL_load_OGL_cubemap_from_memory(
        (const unsigned char *)data[0], size[0], (const unsigned char *)data[1], size[1],
        (const unsigned char *)data[2], size[2], (const unsigned char *)data[3], size[3],
        (const unsigned char *)data[4], size[4], (const unsigned char *)data[5], size[5],
        0, tex_id, SOIL_FLAG_INVERT_Y);
    assert(handle == tex_id);

    GLint w, h;
    glGetTexLevelParameteriv(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, GL_TEXTURE_WIDTH, &w);
    glGetTexLevelParameteriv(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, GL_TEXTURE_HEIGHT, &h);

    params_.w = int(w);
    params_.h = int(h);
    params_.cube = 1;

    SetFilter(p.sampling, log);
}

void Ren::Texture2D::InitFromDDSFile(const void *data[6], const int size[6],
                                     const Tex2DParams &p, ILog *log) {
    assert(p.w > 0 && p.h > 0);
    Free();

    GLuint tex_id;
    glCreateTextures(GL_TEXTURE_CUBE_MAP, 1, &tex_id);
#ifdef ENABLE_OBJ_LABELS
    glObjectLabel(GL_TEXTURE, tex_id, -1, name_.c_str());
#endif

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, tex_id);

    handle_ = {tex_id, TextureHandleCounter++};
    params_ = p;
    initialized_mips_ = 0;

    for (int i = 0; i < 6; i++) {
        DDSHeader header = {};
        memcpy(&header, data[i], sizeof(DDSHeader));

        const uint8_t *pdata = (uint8_t *)data[i] + sizeof(DDSHeader);
        int data_len = size[i] - int(sizeof(DDSHeader));

        for (uint32_t j = 0; j < header.dwMipMapCount; j++) {
            const int width = std::max(int(header.dwWidth >> j), 1),
                      height = std::max(int(header.dwHeight >> j), 1);

            GLenum format = 0;
            int block_size = 0;

            switch ((header.sPixelFormat.dwFourCC >> 24u) - '0') {
            case 1:
                format = GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
                block_size = 8;
                break;
            case 3:
                format = GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;
                block_size = 16;
                break;
            case 5:
                format = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
                block_size = 16;
                break;
            default:
                log->Error("Unknown DDS format %i",
                           int((header.sPixelFormat.dwFourCC >> 24u) - '0'));
                break;
            }

            const int image_len = ((width + 3) / 4) * ((height + 3) / 4) * block_size;
            if (image_len > data_len) {
                log->Error("Insufficient data length, bytes left %i, expected %i",
                           data_len, image_len);
                break;
            }

            glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, j, format, width,
                                   height, 0, image_len, pdata);

            pdata += image_len;
            data_len -= image_len;
        }
    }

    params_.cube = 1;

    SetFilter(p.sampling, log);
}

void Ren::Texture2D::InitFromKTXFile(const void *data[6], const int size[6],
                                     const Tex2DParams &p, ILog *log) {
    (void)size;

    Free();

    GLuint tex_id;
    glCreateTextures(GL_TEXTURE_CUBE_MAP, 1, &tex_id);
#ifdef ENABLE_OBJ_LABELS
    glObjectLabel(GL_TEXTURE, tex_id, -1, name_.c_str());
#endif

    handle_ = {tex_id, TextureHandleCounter++};
    params_ = p;
    initialized_mips_ = 0;

    KTXHeader first_header;
    memcpy(&first_header, data[0], sizeof(KTXHeader));

    bool is_srgb_format;
    params_.format = FormatFromGLInternalFormat(first_header.gl_internal_format,
                                                &params_.block, &is_srgb_format);

    if (is_srgb_format && (params_.flags & TexSRGB) == 0) {
        log->Warning("Loading SRGB texture as non-SRGB!");
    }

    params_.w = int(first_header.pixel_width);
    params_.h = int(first_header.pixel_height);
    params_.cube = true;

    glBindTexture(GL_TEXTURE_CUBE_MAP, tex_id);

    for (int j = 0; j < 6; j++) {
        const auto *_data = (const uint8_t *)data[j];

#ifndef NDEBUG
        KTXHeader this_header;
        memcpy(&this_header, data[j], sizeof(KTXHeader));

        // make sure all images have same properties
        if (this_header.pixel_width != first_header.pixel_width) {
            log->Error("Image width mismatch %i, expected %i",
                       int(this_header.pixel_width), int(first_header.pixel_width));
            continue;
        }
        if (this_header.pixel_height != first_header.pixel_height) {
            log->Error("Image height mismatch %i, expected %i",
                       int(this_header.pixel_height), int(first_header.pixel_height));
            continue;
        }
        if (this_header.gl_internal_format != first_header.gl_internal_format) {
            log->Error("Internal format mismatch %i, expected %i",
                       int(this_header.gl_internal_format),
                       int(first_header.gl_internal_format));
            continue;
        }
#endif
        int data_offset = sizeof(KTXHeader);
        int _w = params_.w, _h = params_.h;

        for (int i = 0; i < int(first_header.mipmap_levels_count); i++) {
            uint32_t img_size;
            memcpy(&img_size, &_data[data_offset], sizeof(uint32_t));
            data_offset += sizeof(uint32_t);
            glCompressedTexImage2D(GLenum(GL_TEXTURE_CUBE_MAP_POSITIVE_X + j), i,
                                   GLenum(first_header.gl_internal_format), _w, _h, 0,
                                   GLsizei(img_size), &_data[data_offset]);
            data_offset += img_size;

            _w = std::max(_w / 2, 1);
            _h = std::max(_h / 2, 1);

            const int pad = (data_offset % 4) ? (4 - (data_offset % 4)) : 0;
            data_offset += pad;
        }
    }

    SetFilter(p.sampling, log);
}

void Ren::Texture2D::SetFilter(TexSamplingParams sampling, ILog *log) {
    const auto tex_id = GLuint(handle_.id);

    if (!params_.cube) {
        ren_glTextureParameteri_Comp(GL_TEXTURE_2D, tex_id, GL_TEXTURE_MIN_FILTER,
                                     g_gl_min_filter[size_t(sampling.filter)]);
        ren_glTextureParameteri_Comp(GL_TEXTURE_2D, tex_id, GL_TEXTURE_MAG_FILTER,
                                     g_gl_mag_filter[size_t(sampling.filter)]);

        ren_glTextureParameteri_Comp(GL_TEXTURE_2D, tex_id, GL_TEXTURE_WRAP_S,
                                     g_gl_wrap_mode[size_t(sampling.repeat)]);
        ren_glTextureParameteri_Comp(GL_TEXTURE_2D, tex_id, GL_TEXTURE_WRAP_T,
                                     g_gl_wrap_mode[size_t(sampling.repeat)]);

#ifndef __ANDROID__
        ren_glTextureParameterf_Comp(
            GL_TEXTURE_2D, tex_id, GL_TEXTURE_LOD_BIAS,
            (params_.flags & eTexFlags::TexNoBias) ? 0.0f : sampling.lod_bias.to_float());
#endif
        ren_glTextureParameterf_Comp(GL_TEXTURE_2D, tex_id, GL_TEXTURE_MIN_LOD,
                                     sampling.min_lod.to_float());
        ren_glTextureParameterf_Comp(GL_TEXTURE_2D, tex_id, GL_TEXTURE_MAX_LOD,
                                     sampling.max_lod.to_float());

        ren_glTextureParameterf_Comp(GL_TEXTURE_2D, tex_id, GL_TEXTURE_MAX_ANISOTROPY_EXT,
                                     AnisotropyLevel);

        const bool custom_mip_filter =
            (params_.flags & (eTexFlags::TexMIPMin | eTexFlags::TexMIPMax));

        if (!IsCompressedFormat(params_.format) &&
            (sampling.filter == eTexFilter::Trilinear ||
             sampling.filter == eTexFilter::Bilinear) &&
            !custom_mip_filter) {
            if (!initialized_mips_) {
                log->Error("Error generating mips from uninitilized data!");
            } else if (initialized_mips_ != (1u << 0)) {
                log->Warning("Overriding initialized mips!");
            }

            ren_glGenerateTextureMipmap_Comp(GL_TEXTURE_2D, tex_id);

            const int mip_count = CalcMipCount(params_.w, params_.h, 1, sampling.filter);
            initialized_mips_ = (1u << mip_count) - 1;
        }

        if (sampling.compare != eTexCompare::None) {
            assert(IsDepthFormat(params_.format));
            ren_glTextureParameteri_Comp(GL_TEXTURE_2D, tex_id, GL_TEXTURE_COMPARE_MODE,
                                         GL_COMPARE_REF_TO_TEXTURE);
            ren_glTextureParameteri_Comp(GL_TEXTURE_2D, tex_id, GL_TEXTURE_COMPARE_FUNC,
                                         g_gl_compare_func[size_t(sampling.compare)]);
        } else {
            ren_glTextureParameteri_Comp(GL_TEXTURE_2D, tex_id, GL_TEXTURE_COMPARE_MODE,
                                         GL_NONE);
        }
    } else {
        ren_glTextureParameteri_Comp(GL_TEXTURE_CUBE_MAP, tex_id, GL_TEXTURE_MIN_FILTER,
                                     g_gl_min_filter[size_t(sampling.filter)]);
        ren_glTextureParameteri_Comp(GL_TEXTURE_CUBE_MAP, tex_id, GL_TEXTURE_MAG_FILTER,
                                     g_gl_mag_filter[size_t(sampling.filter)]);

        ren_glTextureParameteri_Comp(GL_TEXTURE_CUBE_MAP, tex_id, GL_TEXTURE_WRAP_S,
                                     g_gl_wrap_mode[size_t(sampling.repeat)]);
        ren_glTextureParameteri_Comp(GL_TEXTURE_CUBE_MAP, tex_id, GL_TEXTURE_WRAP_T,
                                     g_gl_wrap_mode[size_t(sampling.repeat)]);
        ren_glTextureParameteri_Comp(GL_TEXTURE_CUBE_MAP, tex_id, GL_TEXTURE_WRAP_R,
                                     g_gl_wrap_mode[size_t(sampling.repeat)]);

        if (!IsCompressedFormat(params_.format) &&
            (sampling.filter == eTexFilter::Trilinear ||
             sampling.filter == eTexFilter::Bilinear)) {
            ren_glGenerateTextureMipmap_Comp(GL_TEXTURE_CUBE_MAP, tex_id);
        }
    }

    params_.sampling = sampling;
}

void Ren::Texture2D::SetSubImage(const int level, const int offsetx, const int offsety,
                                 const int sizex, const int sizey,
                                 const Ren::eTexFormat format, const void *data,
                                 const int data_len) {
    assert(format == params_.format);
    assert(params_.samples == 1);
    assert(offsetx >= 0 && offsetx + sizex <= std::max(params_.w >> level, 1));
    assert(offsety >= 0 && offsety + sizey <= std::max(params_.h >> level, 1));

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

    if (offsetx == 0 && offsety == 0 && sizex == std::max(params_.w >> level, 1) &&
        sizey == std::max(params_.h >> level, 1)) {
        // consider this level initialized
        initialized_mips_ |= (1u << level);
    }
}

Ren::SyncFence Ren::Texture2D::SetSubImage(const int level, const int offsetx,
                                           const int offsety, const int sizex,
                                           const int sizey, const Ren::eTexFormat format,
                                           const TextureStageBuf &sbuf,
                                           const int data_off, const int data_len) {
    assert(format == params_.format);
    assert(params_.samples == 1);
    assert(offsetx >= 0 && offsetx + sizex <= std::max(params_.w >> level, 1));
    assert(offsety >= 0 && offsety + sizey <= std::max(params_.h >> level, 1));

    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, sbuf.id());

    if (IsCompressedFormat(format)) {
        ren_glCompressedTextureSubImage2D_Comp(
            GL_TEXTURE_2D, GLuint(handle_.id), GLint(level), GLint(offsetx),
            GLint(offsety), GLsizei(sizex), GLsizei(sizey),
            GLInternalFormatFromTexFormat(format, (params_.flags & TexSRGB) != 0),
            GLsizei(data_len), reinterpret_cast<const void *>(uintptr_t(data_off)));
    } else {
        ren_glTextureSubImage2D_Comp(GL_TEXTURE_2D, GLuint(handle_.id), level, offsetx,
                                     offsety, sizex, sizey, GLFormatFromTexFormat(format),
                                     GLTypeFromTexFormat(format),
                                     reinterpret_cast<const void *>(uintptr_t(data_off)));
    }

    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

    if (offsetx == 0 && offsety == 0 && sizex == std::max(params_.w >> level, 1) &&
        sizey == std::max(params_.h >> level, 1)) {
        // consider this level initialized
        initialized_mips_ |= (1u << level);
    }

    return MakeFence();
}

void Ren::Texture2D::DownloadTextureData(const eTexFormat format, void *out_data) const {
#if defined(__ANDROID__)
#else
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, GLuint(handle_.id));

    if (format == eTexFormat::RawRGBA8888) {
        glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, out_data);
    }

    glBindTexture(GL_TEXTURE_2D, 0);
#endif
}

////////////////////////////////////////////////////////////////////////////////////////

void Ren::TextureStageBuf::Alloc(const uint32_t new_size,
                                 const bool persistantly_mapped) {
    assert(Ren::IsMainThread());
    assert(id_ == 0xffffffff);

    GLuint pbo_id;
    glGenBuffers(1, &pbo_id);

    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo_id);
    glBufferStorage(GL_PIXEL_UNPACK_BUFFER, new_size, nullptr,
                    persistantly_mapped
                        ? GLbitfield(GL_MAP_WRITE_BIT) | GLbitfield(GL_MAP_PERSISTENT_BIT)
                        : GLbitfield(GL_MAP_WRITE_BIT));

    if (persistantly_mapped) {
        assert(!mapped_ptr_);
        const GLbitfield BufferRangeMapFlags =
            GLbitfield(GL_MAP_WRITE_BIT) | GLbitfield(GL_MAP_PERSISTENT_BIT) |
            GLbitfield(GL_MAP_INVALIDATE_RANGE_BIT) |
            GLbitfield(GL_MAP_UNSYNCHRONIZED_BIT) | GLbitfield(GL_MAP_FLUSH_EXPLICIT_BIT);
        mapped_ptr_ = (uint8_t *)glMapBufferRange(GL_PIXEL_UNPACK_BUFFER, 0, new_size,
                                                  BufferRangeMapFlags);
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    }

    id_ = uint32_t(pbo_id);
    size_ = new_size;
}

void Ren::TextureStageBuf::Free() {
    assert(Ren::IsMainThread());
    if (id_ != 0xffffffff) {
        auto pbo_id = GLuint(id_);
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo_id);
        if (mapped_ptr_) {
            glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
            mapped_ptr_ = nullptr;
        }
        glDeleteBuffers(1, &pbo_id);
        id_ = 0xffffffff;
    }
}

uint8_t *Ren::TextureStageBuf::MapRange(const uint32_t offset, const uint32_t size) {
    const GLbitfield BufferRangeMapFlags =
        GLbitfield(GL_MAP_WRITE_BIT) | GLbitfield(GL_MAP_INVALIDATE_RANGE_BIT) |
        GLbitfield(GL_MAP_UNSYNCHRONIZED_BIT) | GLbitfield(GL_MAP_FLUSH_EXPLICIT_BIT);
    assert(!mapped_ptr_);

    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, GLuint(id_));
    auto *pinned_mem = (uint8_t *)glMapBufferRange(
        GL_PIXEL_UNPACK_BUFFER, GLintptr(offset), GLsizeiptr(size), BufferRangeMapFlags);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

    return pinned_mem;
}

void Ren::TextureStageBuf::Unmap() {
    assert(!mapped_ptr_);

    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, GLuint(id_));
    glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
}

void Ren::TextureStageBuf::FlushMapped(const uint32_t offset, const uint32_t size) {
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, GLuint(id_));
    glFlushMappedBufferRange(GL_PIXEL_UNPACK_BUFFER, offset,
                             GLsizeiptr(size ? size : size_));
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
}

////////////////////////////////////////////////////////////////////////////////////////

Ren::Texture1D::Texture1D(const char *name, BufferRef buf, const eTexFormat format,
                          const uint32_t offset, const uint32_t size, ILog *log)
    : name_(name) {
    Init(std::move(buf), format, offset, size, log);
}

Ren::Texture1D::~Texture1D() { Free(); }

Ren::Texture1D &Ren::Texture1D::operator=(Texture1D &&rhs) noexcept {
    if (this == &rhs) {
        return (*this);
    }

    Free();

    handle_ = exchange(rhs.handle_, {});
    buf_ = std::move(rhs.buf_);
    params_ = exchange(rhs.params_, {});
    name_ = std::move(rhs.name_);

    RefCounter::operator=(std::move(rhs));

    return (*this);
}

void Ren::Texture1D::Init(BufferRef buf, const eTexFormat format, const uint32_t offset,
                          const uint32_t size, ILog *log) {
    Free();

    GLuint tex_id;
    glGenTextures(1, &tex_id);
    glBindTexture(GL_TEXTURE_BUFFER, tex_id);

    glTexBufferRange(GL_TEXTURE_BUFFER,
                     GLInternalFormatFromTexFormat(format, false /* is_srgb */),
                     GLuint(buf->id()), offset, size);

    glBindTexture(GL_TEXTURE_BUFFER, 0);

    handle_ = {uint32_t(tex_id), TextureHandleCounter++};
    buf_ = std::move(buf);
    params_.offset = offset;
    params_.size = size;
    params_.format = format;
}

void Ren::Texture1D::Free() {
    if (params_.format != eTexFormat::Undefined) {
        assert(IsMainThread());
        auto tex_id = GLuint(handle_.id);
        glDeleteTextures(1, &tex_id);
        handle_ = {};
    }
}

uint32_t Ren::GLFormatFromTexFormat(const eTexFormat format) {
    return g_gl_formats[size_t(format)];
}

uint32_t Ren::GLInternalFormatFromTexFormat(const eTexFormat format, const bool is_srgb) {
    const uint32_t ret = g_gl_internal_formats[size_t(format)];
    return is_srgb ? Ren::ToSRGBFormat(ret) : ret;
}

uint32_t Ren::GLTypeFromTexFormat(const eTexFormat format) {
    return g_gl_types[size_t(format)];
}

uint32_t Ren::GLBindTarget(const eBindTarget binding) {
    return gl_binding_targets[size_t(binding)];
}

Ren::eTexFormat Ren::FormatFromGLInternalFormat(const uint32_t gl_internal_format,
                                                eTexBlock *block, bool *is_srgb) {
    (*is_srgb) = false;

    switch (gl_internal_format) {
    case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT:
        (*is_srgb) = true;
    case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
        (*block) = eTexBlock::_4x4;
        return eTexFormat::Compressed_DXT1;
    case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT:
        (*is_srgb) = true;
    case GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:
        (*block) = eTexBlock::_4x4;
        return eTexFormat::Compressed_DXT3;
    case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT:
        (*is_srgb) = true;
    case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
        (*block) = eTexBlock::_4x4;
        return eTexFormat::Compressed_DXT5;
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR:
        (*is_srgb) = true;
    case GL_COMPRESSED_RGBA_ASTC_4x4_KHR:
        (*block) = eTexBlock::_4x4;
        return eTexFormat::Compressed_ASTC;
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x4_KHR:
        (*is_srgb) = true;
    case GL_COMPRESSED_RGBA_ASTC_5x4_KHR:
        (*block) = eTexBlock::_5x4;
        return eTexFormat::Compressed_ASTC;
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x5_KHR:
        (*is_srgb) = true;
    case GL_COMPRESSED_RGBA_ASTC_5x5_KHR:
        (*block) = eTexBlock::_5x5;
        return eTexFormat::Compressed_ASTC;
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x5_KHR:
        (*is_srgb) = true;
    case GL_COMPRESSED_RGBA_ASTC_6x5_KHR:
        (*block) = eTexBlock::_6x5;
        return eTexFormat::Compressed_ASTC;
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6_KHR:
        (*is_srgb) = true;
    case GL_COMPRESSED_RGBA_ASTC_6x6_KHR:
        (*block) = eTexBlock::_6x6;
        return eTexFormat::Compressed_ASTC;
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x5_KHR:
        (*is_srgb) = true;
    case GL_COMPRESSED_RGBA_ASTC_8x5_KHR:
        (*block) = eTexBlock::_8x5;
        return eTexFormat::Compressed_ASTC;
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x6_KHR:
        (*is_srgb) = true;
    case GL_COMPRESSED_RGBA_ASTC_8x6_KHR:
        (*block) = eTexBlock::_8x6;
        return eTexFormat::Compressed_ASTC;
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x8_KHR:
        (*is_srgb) = true;
    case GL_COMPRESSED_RGBA_ASTC_8x8_KHR:
        (*block) = eTexBlock::_8x8;
        return eTexFormat::Compressed_ASTC;
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x5_KHR:
        (*is_srgb) = true;
    case GL_COMPRESSED_RGBA_ASTC_10x5_KHR:
        (*block) = eTexBlock::_10x5;
        return eTexFormat::Compressed_ASTC;
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x6_KHR:
        (*is_srgb) = true;
    case GL_COMPRESSED_RGBA_ASTC_10x6_KHR:
        (*block) = eTexBlock::_10x6;
        return eTexFormat::Compressed_ASTC;
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x8_KHR:
        (*is_srgb) = true;
    case GL_COMPRESSED_RGBA_ASTC_10x8_KHR:
        (*block) = eTexBlock::_10x8;
        return eTexFormat::Compressed_ASTC;
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x10_KHR:
        (*is_srgb) = true;
    case GL_COMPRESSED_RGBA_ASTC_10x10_KHR:
        (*block) = eTexBlock::_10x10;
        return eTexFormat::Compressed_ASTC;
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x10_KHR:
        (*is_srgb) = true;
    case GL_COMPRESSED_RGBA_ASTC_12x10_KHR:
        (*block) = eTexBlock::_12x10;
        return eTexFormat::Compressed_ASTC;
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x12_KHR:
        (*is_srgb) = true;
    case GL_COMPRESSED_RGBA_ASTC_12x12_KHR:
        (*block) = eTexBlock::_12x12;
        return eTexFormat::Compressed_ASTC;
    default:
        assert(false && "Unsupported format!");
    }

    return eTexFormat::Undefined;
}

void Ren::GLUnbindTextureUnits(const int start, const int count) {
    for (int i = start; i < start + count; i++) {
        ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, i, 0);
        ren_glBindTextureUnit_Comp(GL_TEXTURE_2D_ARRAY, i, 0);
        ren_glBindTextureUnit_Comp(GL_TEXTURE_2D_MULTISAMPLE, i, 0);
        ren_glBindTextureUnit_Comp(GL_TEXTURE_CUBE_MAP, i, 0);
        ren_glBindTextureUnit_Comp(GL_TEXTURE_CUBE_MAP_ARRAY, i, 0);
        ren_glBindTextureUnit_Comp(GL_TEXTURE_BUFFER, i, 0);
    }
}

Ren::Framebuffer::~Framebuffer() {
    if (id_) {
        auto fb = GLuint(id_);
        glDeleteFramebuffers(1, &fb);
    }
}

bool Ren::Framebuffer::Setup(const TexHandle color_attachments[],
                             const int color_attachments_count,
                             const TexHandle depth_attachment,
                             const TexHandle stencil_attachment,
                             const bool is_multisampled) {
    if (color_attachments_count == color_attachments_count_ &&
        std::equal(color_attachments, color_attachments + color_attachments_count,
                   color_attachments_) &&
        depth_attachment == depth_attachment_ &&
        stencil_attachment == stencil_attachment_) {
        // nothing has changed
        return true;
    }

    if (color_attachments_count == 1 && color_attachments[0].id == 0) {
        // default backbuffer
        return true;
    }

    if (!id_) {
        GLuint fb;
        glGenFramebuffers(1, &fb);
        id_ = uint32_t(fb);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, GLuint(id_));

    const GLenum target = is_multisampled ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D;

    GLenum draw_buffers[MaxColorAttachments];
    for (int i = 0; i < color_attachments_count; i++) {
        if (color_attachments[i].id) {
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, target,
                                   GLuint(color_attachments[i].id), 0);

            draw_buffers[i] = GL_COLOR_ATTACHMENT0 + i;
        } else {
            draw_buffers[i] = GL_NONE;
        }
        color_attachments_[i] = color_attachments[i];
    }

    for (int i = color_attachments_count; i < color_attachments_count_; i++) {
        if (color_attachments_[i].id) {
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, target, 0,
                                   0);
            color_attachments_[i] = {};
        }
    }

    color_attachments_count_ = color_attachments_count;
    glDrawBuffers(GLsizei(color_attachments_count_), draw_buffers);

    if (depth_attachment.id) {
        if (depth_attachment == stencil_attachment) {
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, target,
                                   GLuint(depth_attachment.id), 0);
        } else {
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, target,
                                   GLuint(depth_attachment.id), 0);
        }
        depth_attachment_ = depth_attachment;
    } else {
        depth_attachment_ = {};
    }

    if (stencil_attachment.id) {
        if (depth_attachment != stencil_attachment) {
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, target,
                                   GLuint(stencil_attachment.id), 0);
        }
        stencil_attachment_ = stencil_attachment;
    } else {
        stencil_attachment_ = {};
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    const GLenum s = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    return (s == GL_FRAMEBUFFER_COMPLETE);
}

#ifdef _MSC_VER
#pragma warning(pop)
#endif
