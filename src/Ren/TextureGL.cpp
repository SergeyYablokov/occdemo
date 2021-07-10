#include "TextureGL.h"

#include <memory>
#undef min
#undef max

#include "GL.h"
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
static_assert(sizeof(g_gl_formats) / sizeof(g_gl_formats[0]) == size_t(eTexFormat::_Count), "!");

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
static_assert(sizeof(g_gl_internal_formats) / sizeof(g_gl_internal_formats[0]) == size_t(eTexFormat::_Count), "!");

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
static_assert(sizeof(g_gl_types) / sizeof(g_gl_types[0]) == size_t(eTexFormat::_Count), "!");

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
static_assert(sizeof(g_gl_compare_func) / sizeof(g_gl_compare_func[0]) == size_t(eTexCompare::_Count), "!");

const uint32_t gl_binding_targets[] = {
    GL_TEXTURE_2D,             // Tex2D
    GL_TEXTURE_2D_MULTISAMPLE, // Tex2DMs
    GL_TEXTURE_CUBE_MAP_ARRAY, // TexCubeArray
    GL_TEXTURE_BUFFER,         // TexBuf
    GL_UNIFORM_BUFFER,         // UBuf
};
static_assert(sizeof(gl_binding_targets) / sizeof(gl_binding_targets[0]) == size_t(eBindTarget::_Count), "!");

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

} // namespace Ren

static_assert(sizeof(GLsync) == sizeof(void *), "!");

Ren::Texture2D::Texture2D(const char *name, ApiContext *api_ctx, const Tex2DParams &p, MemoryAllocators *, ILog *log)
    : name_(name) {
    Init(p, nullptr, log);
}

Ren::Texture2D::Texture2D(const char *name, ApiContext *api_ctx, const void *data, const uint32_t size,
                          const Tex2DParams &p, Buffer &stage_buf, void *, MemoryAllocators *,
                          eTexLoadStatus *load_status, ILog *log)
    : name_(name) {
    Init(data, size, p, stage_buf, nullptr, nullptr, load_status, log);
}

Ren::Texture2D::Texture2D(const char *name, ApiContext *api_ctx, const void *data[6], const int size[6],
                          const Tex2DParams &p, Buffer &stage_buf, void *_cmd_buf, MemoryAllocators *mem_allocs,
                          eTexLoadStatus *load_status, ILog *log)
    : name_(name) {
    Init(data, size, p, stage_buf, nullptr, nullptr, load_status, log);
}

Ren::Texture2D::~Texture2D() { Free(); }

Ren::Texture2D &Ren::Texture2D::operator=(Ren::Texture2D &&rhs) noexcept {
    if (this == &rhs) {
        return (*this);
    }

    RefCounter::operator=(static_cast<RefCounter &&>(rhs));

    Free();

    handle_ = exchange(rhs.handle_, {});
    params_ = exchange(rhs.params_, {});
    ready_ = exchange(rhs.ready_, false);
    cubemap_ready_ = exchange(rhs.cubemap_ready_, 0);
    name_ = std::move(rhs.name_);

    return (*this);
}

uint64_t Ren::Texture2D::GetBindlessHandle() const { return glGetTextureHandleARB(GLuint(handle_.id)); }

void Ren::Texture2D::Init(const Tex2DParams &p, MemoryAllocators *, ILog *log) {
    assert(IsMainThread());
    InitFromRAWData(nullptr, 0, p, log);
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
        InitFromRAWData(&sbuf, 0, _p, log);
        // mark it as not ready
        ready_ = false;
        (*load_status) = eTexLoadStatus::CreatedDefault;
    } else {
        if (name_.EndsWith(".tga_rgbe") != 0 || name_.EndsWith(".TGA_RGBE") != 0) {
            InitFromTGA_RGBEFile(data, sbuf, p, log);
        } else if (name_.EndsWith(".tga") != 0 || name_.EndsWith(".TGA") != 0) {
            InitFromTGAFile(data, sbuf, p, log);
        } else if (name_.EndsWith(".dds") != 0 || name_.EndsWith(".DDS") != 0) {
            InitFromDDSFile(data, size, sbuf, p, log);
        } else if (name_.EndsWith(".ktx") != 0 || name_.EndsWith(".KTX") != 0) {
            InitFromKTXFile(data, size, sbuf, p, log);
        } else if (name_.EndsWith(".png") != 0 || name_.EndsWith(".PNG") != 0) {
            InitFromPNGFile(data, size, sbuf, p, log);
        } else {
            uint8_t *stage_data = sbuf.Map(BufMapWrite);
            memcpy(stage_data, data, size);
            sbuf.FlushMappedRange(0, size);
            sbuf.Unmap();

            InitFromRAWData(&sbuf, 0, p, log);
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
        InitFromRAWData(sbuf, data_off, _p, log);
        // mark it as not ready
        ready_ = false;
        cubemap_ready_ = 0;
        (*load_status) = eTexLoadStatus::CreatedDefault;
    } else {
        if (name_.EndsWith(".tga_rgbe") != 0 || name_.EndsWith(".TGA_RGBE") != 0) {
            InitFromTGA_RGBEFile(data, sbuf, p, log);
        } else if (name_.EndsWith(".tga") != 0 || name_.EndsWith(".TGA") != 0) {
            InitFromTGAFile(data, sbuf, p, log);
        } else if (name_.EndsWith(".png") != 0 || name_.EndsWith(".PNG") != 0) {
            InitFromPNGFile(data, size, sbuf, p, log);
        } else if (name_.EndsWith(".ktx") != 0 || name_.EndsWith(".KTX") != 0) {
            InitFromKTXFile(data, size, sbuf, p, log);
        } else if (name_.EndsWith(".dds") != 0 || name_.EndsWith(".DDS") != 0) {
            InitFromDDSFile(data, size, sbuf, p, log);
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

            InitFromRAWData(sbuf, data_off, p, log);
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
        auto tex_id = GLuint(handle_.id);
        glDeleteTextures(1, &tex_id);
        handle_ = {};
        params_.format = eTexFormat::Undefined;
    }
}

void Ren::Texture2D::Realloc(const int w, const int h, int mip_count, const int samples, const Ren::eTexFormat format,
                             const Ren::eTexBlock block, const bool is_srgb, void *_cmd_buf,
                             MemoryAllocators *mem_allocs, ILog *log) {
    GLuint tex_id;
    glCreateTextures(samples > 1 ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D, 1, &tex_id);
#ifdef ENABLE_OBJ_LABELS
    glObjectLabel(GL_TEXTURE, tex_id, -1, name_.c_str());
#endif
    const GLuint internal_format = GLInternalFormatFromTexFormat(format, is_srgb);

    mip_count = std::min(mip_count, CalcMipCount(w, h, 1, Ren::eTexFilter::Trilinear));

    // allocate all mip levels
    ren_glTextureStorage2D_Comp(GL_TEXTURE_2D, tex_id, GLsizei(mip_count), internal_format, GLsizei(w), GLsizei(h));
#ifdef TEX_VERBOSE_LOGGING
    if (params_.format != eTexFormat::Undefined) {
        log->Info("Realloc %s, %ix%i (%i mips) -> %ix%i (%i mips)", name_.c_str(), int(params_.w), int(params_.h),
                  int(params_.mip_count), w, h, mip_count);
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

        for (; src_mip < int(params_.mip_count) && dst_mip < mip_count; ++src_mip, ++dst_mip) {
            if (initialized_mips_ & (1u << src_mip)) {
                glCopyImageSubData(GLuint(handle_.id), GL_TEXTURE_2D, GLint(src_mip), 0, 0, 0, GLuint(new_handle.id),
                                   GL_TEXTURE_2D, GLint(dst_mip), 0, 0, 0, GLsizei(std::max(params_.w >> src_mip, 1)),
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
    params_.block = block;
    initialized_mips_ = new_initialized_mips;
}

void Ren::Texture2D::InitFromRAWData(const Buffer *sbuf, int data_off, const Tex2DParams &p, ILog *log) {
    Free();

    GLuint tex_id;
    glCreateTextures(p.samples > 1 ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D, 1, &tex_id);
#ifdef ENABLE_OBJ_LABELS
    glObjectLabel(GL_TEXTURE, tex_id, -1, name_.c_str());
#endif
    handle_ = {tex_id, TextureHandleCounter++};

    params_ = p;
    initialized_mips_ = 0;

    const auto format = (GLenum)GLFormatFromTexFormat(p.format),
               internal_format = (GLenum)GLInternalFormatFromTexFormat(p.format, (p.flags & eTexFlags::TexSRGB) != 0),
               type = (GLenum)GLTypeFromTexFormat(p.format);

    auto mip_count = (GLsizei)CalcMipCount(p.w, p.h, 1, p.sampling.filter);

    if (format != 0xffffffff && internal_format != 0xffffffff && type != 0xffffffff) {
        if (p.flags & TexMutable) {
            glBindTexture(GL_TEXTURE_2D, tex_id);
            for (int i = 0; i < mip_count; i++) {
                const int w = std::max(p.w << i, 1);
                const int h = std::max(p.h << i, 1);

                if (i == 0 && sbuf) {
                    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, sbuf->id());
                    glTexImage2D(GL_TEXTURE_2D, GLint(i), internal_format, w, h, 0, format, type,
                                 reinterpret_cast<const GLvoid *>(uintptr_t(data_off)));
                    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
                } else {
                    glTexImage2D(GL_TEXTURE_2D, GLint(i), internal_format, w, h, 0, format, type, nullptr);
                }

                initialized_mips_ |= (1u << i);
            }
        } else {
            if (p.samples > 1) {
                glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, tex_id);
                glTexStorage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, GLsizei(p.samples), internal_format, GLsizei(p.w),
                                          GLsizei(p.h), GL_TRUE);
                initialized_mips_ |= (1u << 0);
            } else {
                // allocate all mip levels
                ren_glTextureStorage2D_Comp(GL_TEXTURE_2D, tex_id, mip_count, internal_format, GLsizei(p.w),
                                            GLsizei(p.h));
                if (sbuf) {
                    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, sbuf->id());

                    // update first level
                    ren_glTextureSubImage2D_Comp(GL_TEXTURE_2D, tex_id, 0, 0, 0, p.w, p.h, format, type,
                                                 reinterpret_cast<const GLvoid *>(uintptr_t(data_off)));
                    initialized_mips_ |= (1u << 0);

                    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
                }
            }
        }
    }

    if (p.samples == 1) {
        ApplySampling(p.sampling, log);
    }

    CheckError("create texture", log);
}

void Ren::Texture2D::InitFromTGAFile(const void *data, Buffer &sbuf, const Tex2DParams &p, ILog *log) {
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

    InitFromRAWData(&sbuf, 0, _p, log);
}

void Ren::Texture2D::InitFromTGA_RGBEFile(const void *data, Buffer &sbuf, const Tex2DParams &p, ILog *log) {
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

    InitFromRAWData(&sbuf, 0, _p, log);
}

void Ren::Texture2D::InitFromDDSFile(const void *data, const int size, Buffer &sbuf, const Tex2DParams &p, ILog *log) {
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
            (p.flags & TexSRGB) != 0, nullptr, nullptr, log);

    params_.flags = p.flags;
    params_.block = block;
    params_.sampling = p.sampling;

    const GLuint internal_format = GLInternalFormatFromTexFormat(params_.format, (p.flags & TexSRGB) != 0);

    int w = params_.w, h = params_.h;
    uint32_t bytes_left = uint32_t(size) - sizeof(DDSHeader);
    const uint8_t *p_data = (uint8_t *)data + sizeof(DDSHeader);

    assert(bytes_left <= sbuf.size());
    uint8_t *stage_data = sbuf.Map(BufMapWrite);
    memcpy(stage_data, p_data, bytes_left);
    sbuf.FlushMappedRange(0, bytes_left);
    sbuf.Unmap();

    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, sbuf.id());

    uintptr_t data_off = 0;
    for (uint32_t i = 0; i < header.dwMipMapCount; i++) {
        const uint32_t len = ((w + 3) / 4) * ((h + 3) / 4) * block_size_bytes;
        if (len > bytes_left) {
            log->Error("Insufficient data length, bytes left %i, expected %i", bytes_left, len);
            return;
        }

        ren_glCompressedTextureSubImage2D_Comp(GL_TEXTURE_2D, GLuint(handle_.id), GLint(i), 0, 0, w, h, internal_format,
                                               len, reinterpret_cast<const GLvoid *>(data_off));
        initialized_mips_ |= (1u << i);

        data_off += len;
        bytes_left -= len;
        w = std::max(w / 2, 1);
        h = std::max(h / 2, 1);
    }

    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

    ApplySampling(p.sampling, log);
}

void Ren::Texture2D::InitFromPNGFile(const void *data, const int size, Buffer &sbuf, const Tex2DParams &p, ILog *log) {
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

    InitFromRAWData(&sbuf, 0, _p, log);
    free(image_data);
}

void Ren::Texture2D::InitFromKTXFile(const void *data, const int size, Buffer &sbuf, const Tex2DParams &p, ILog *log) {
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

    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, sbuf.id());

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

        ren_glCompressedTextureSubImage2D_Comp(
            GL_TEXTURE_2D, GLuint(handle_.id), i, 0, 0, w, h,
            GLInternalFormatFromTexFormat(params_.format, (params_.flags & TexSRGB) != 0), GLsizei(img_size),
            reinterpret_cast<const GLvoid *>(uintptr_t(data_offset)));
        initialized_mips_ |= (1u << i);
        data_offset += img_size;

        w = std::max(w / 2, 1);
        h = std::max(h / 2, 1);

        const int pad = (data_offset % 4) ? (4 - (data_offset % 4)) : 0;
        data_offset += pad;
    }

    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

    ApplySampling(p.sampling, log);
}

void Ren::Texture2D::InitFromRAWData(const Buffer &sbuf, int data_off[6], const Tex2DParams &p, ILog *log) {
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
               internal_format = (GLenum)GLInternalFormatFromTexFormat(params_.format, (p.flags & TexSRGB) != 0),
               type = (GLenum)GLTypeFromTexFormat(params_.format);

    const int w = p.w, h = p.h;
    const eTexFilter f = params_.sampling.filter;

    auto mip_count = (GLsizei)CalcMipCount(w, h, 1, f);

    // allocate all mip levels
    ren_glTextureStorage2D_Comp(GL_TEXTURE_CUBE_MAP, tex_id, mip_count, internal_format, w, h);

    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, sbuf.id());

    for (unsigned i = 0; i < 6; i++) {
        if (data_off[i] == -1) {
            continue;
        } else {
            cubemap_ready_ |= (1u << i);
        }

        if (format != 0xffffffff && internal_format != 0xffffffff && type != 0xffffffff) {
            ren_glTextureSubImage3D_Comp(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, tex_id, 0, 0, 0, i, w, h, 1, format, type,
                                         reinterpret_cast<const GLvoid *>(uintptr_t(data_off[i])));
        }
    }

    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

    if (f == eTexFilter::NoFilter) {
        ren_glTextureParameteri_Comp(GL_TEXTURE_CUBE_MAP, tex_id, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        ren_glTextureParameteri_Comp(GL_TEXTURE_CUBE_MAP, tex_id, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    } else if (f == eTexFilter::Bilinear) {
        ren_glTextureParameteri_Comp(GL_TEXTURE_CUBE_MAP, tex_id, GL_TEXTURE_MIN_FILTER,
                                     (cubemap_ready_ == 0x3F) ? GL_LINEAR_MIPMAP_NEAREST : GL_LINEAR);
        ren_glTextureParameteri_Comp(GL_TEXTURE_CUBE_MAP, tex_id, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    } else if (f == eTexFilter::Trilinear) {
        ren_glTextureParameteri_Comp(GL_TEXTURE_CUBE_MAP, tex_id, GL_TEXTURE_MIN_FILTER,
                                     (cubemap_ready_ == 0x3F) ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
        ren_glTextureParameteri_Comp(GL_TEXTURE_CUBE_MAP, tex_id, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    } else if (f == eTexFilter::BilinearNoMipmap) {
        ren_glTextureParameteri_Comp(GL_TEXTURE_CUBE_MAP, tex_id, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        ren_glTextureParameteri_Comp(GL_TEXTURE_CUBE_MAP, tex_id, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }

    ren_glTextureParameteri_Comp(GL_TEXTURE_CUBE_MAP, tex_id, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    ren_glTextureParameteri_Comp(GL_TEXTURE_CUBE_MAP, tex_id, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
#if !defined(GL_ES_VERSION_2_0) && !defined(__EMSCRIPTEN__)
    ren_glTextureParameteri_Comp(GL_TEXTURE_CUBE_MAP, tex_id, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
#endif

    if ((f == eTexFilter::Trilinear || f == eTexFilter::Bilinear) && (cubemap_ready_ == 0x3F)) {
        ren_glGenerateTextureMipmap_Comp(GL_TEXTURE_CUBE_MAP, tex_id);
    }
}

void Ren::Texture2D::InitFromTGAFile(const void *data[6], Buffer &sbuf, const Tex2DParams &p, ILog *log) {
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

    InitFromRAWData(sbuf, data_off, _p, log);
}

void Ren::Texture2D::InitFromTGA_RGBEFile(const void *data[6], Buffer &sbuf, const Tex2DParams &p, ILog *log) {
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

    InitFromRAWData(sbuf, data_off, _p, log);
}

void Ren::Texture2D::InitFromPNGFile(const void *data[6], const int size[6], Buffer &sbuf, const Tex2DParams &p,
                                     ILog *log) {
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

    InitFromRAWData(sbuf, data_off, _p, log);
}

void Ren::Texture2D::InitFromDDSFile(const void *data[6], const int size[6], Buffer &sbuf, const Tex2DParams &p,
                                     ILog *log) {
    assert(p.w > 0 && p.h > 0);
    Free();

    uint8_t *stage_data = sbuf.Map(BufMapWrite);
    uint32_t data_off[6] = {};
    uint32_t stage_len = 0;

    GLenum first_format = 0;

    for (int i = 0; i < 6; ++i) {
        const DDSHeader *header = reinterpret_cast<const DDSHeader *>(data[i]);

        GLenum format = 0;

        switch ((header->sPixelFormat.dwFourCC >> 24u) - '0') {
        case 1:
            format = GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
            break;
        case 3:
            format = GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;
            break;
        case 5:
            format = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
            break;
        default:
            log->Error("Unknown DDS format %i", int((header->sPixelFormat.dwFourCC >> 24u) - '0'));
            break;
        }

        if (i == 0) {
            first_format = format;
        } else {
            assert(format == first_format);
        }

        memcpy(stage_data + stage_len, data[i], size[i]);

        data_off[i] = stage_len;
        stage_len += size[i];
    }

    sbuf.FlushMappedRange(0, stage_len);
    sbuf.Unmap();

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

    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, sbuf.handle().id);

    for (int i = 0; i < 6; i++) {
        const DDSHeader *header = reinterpret_cast<const DDSHeader *>(data[i]);
        int data_offset = sizeof(DDSHeader);
        for (uint32_t j = 0; j < header->dwMipMapCount; j++) {
            const int width = std::max(int(header->dwWidth >> j), 1), height = std::max(int(header->dwHeight >> j), 1);

            const int image_len = ((width + 3) / 4) * ((height + 3) / 4) * BlockLenFromGLInternalFormat(first_format);
            if (data_off[i] + image_len > stage_len) {
                log->Error("Insufficient data length!");
                break;
            }

            glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, j, first_format, width, height, 0, image_len,
                                   reinterpret_cast<const GLvoid *>(uintptr_t(data_off[i] + data_offset)));

            data_offset += image_len;
        }
    }

    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

    params_.cube = 1;

    ApplySampling(p.sampling, log);
}

void Ren::Texture2D::InitFromKTXFile(const void *data[6], const int size[6], Buffer &sbuf, const Tex2DParams &p,
                                     ILog *log) {
    (void)size;

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

    GLuint tex_id;
    glCreateTextures(GL_TEXTURE_CUBE_MAP, 1, &tex_id);
#ifdef ENABLE_OBJ_LABELS
    glObjectLabel(GL_TEXTURE, tex_id, -1, name_.c_str());
#endif

    handle_ = {tex_id, TextureHandleCounter++};
    params_ = p;
    initialized_mips_ = 0;

    bool is_srgb_format;
    params_.format = FormatFromGLInternalFormat(first_header->gl_internal_format, &params_.block, &is_srgb_format);

    if (is_srgb_format && (params_.flags & TexSRGB) == 0) {
        log->Warning("Loading SRGB texture as non-SRGB!");
    }

    params_.w = int(first_header->pixel_width);
    params_.h = int(first_header->pixel_height);
    params_.cube = true;

    glBindTexture(GL_TEXTURE_CUBE_MAP, tex_id);

    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, sbuf.handle().id);

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
            glCompressedTexImage2D(GLenum(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i), j,
                                   GLenum(first_header->gl_internal_format), _w, _h, 0, GLsizei(img_size),
                                   reinterpret_cast<const GLvoid *>(uintptr_t(data_off[i] + data_offset)));
            data_offset += img_size;

            _w = std::max(_w / 2, 1);
            _h = std::max(_h / 2, 1);

            const int pad = (data_offset % 4) ? (4 - (data_offset % 4)) : 0;
            data_offset += pad;
        }
    }

    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

    ApplySampling(p.sampling, log);
}

void Ren::Texture2D::ApplySampling(SamplingParams sampling, ILog *log) {
    const auto tex_id = GLuint(handle_.id);

    if (!params_.cube) {
        ren_glTextureParameteri_Comp(GL_TEXTURE_2D, tex_id, GL_TEXTURE_MIN_FILTER,
                                     g_gl_min_filter[size_t(sampling.filter)]);
        ren_glTextureParameteri_Comp(GL_TEXTURE_2D, tex_id, GL_TEXTURE_MAG_FILTER,
                                     g_gl_mag_filter[size_t(sampling.filter)]);

        ren_glTextureParameteri_Comp(GL_TEXTURE_2D, tex_id, GL_TEXTURE_WRAP_S, g_gl_wrap_mode[size_t(sampling.repeat)]);
        ren_glTextureParameteri_Comp(GL_TEXTURE_2D, tex_id, GL_TEXTURE_WRAP_T, g_gl_wrap_mode[size_t(sampling.repeat)]);

#ifndef __ANDROID__
        ren_glTextureParameterf_Comp(GL_TEXTURE_2D, tex_id, GL_TEXTURE_LOD_BIAS,
                                     (params_.flags & eTexFlags::TexNoBias) ? 0.0f : sampling.lod_bias.to_float());
#endif
        ren_glTextureParameterf_Comp(GL_TEXTURE_2D, tex_id, GL_TEXTURE_MIN_LOD, sampling.min_lod.to_float());
        ren_glTextureParameterf_Comp(GL_TEXTURE_2D, tex_id, GL_TEXTURE_MAX_LOD, sampling.max_lod.to_float());

        ren_glTextureParameterf_Comp(GL_TEXTURE_2D, tex_id, GL_TEXTURE_MAX_ANISOTROPY_EXT, AnisotropyLevel);

        const bool custom_mip_filter = (params_.flags & (eTexFlags::TexMIPMin | eTexFlags::TexMIPMax));

        if (!IsCompressedFormat(params_.format) &&
            (sampling.filter == eTexFilter::Trilinear || sampling.filter == eTexFilter::Bilinear) &&
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
            ren_glTextureParameteri_Comp(GL_TEXTURE_2D, tex_id, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
            ren_glTextureParameteri_Comp(GL_TEXTURE_2D, tex_id, GL_TEXTURE_COMPARE_FUNC,
                                         g_gl_compare_func[size_t(sampling.compare)]);
        } else {
            ren_glTextureParameteri_Comp(GL_TEXTURE_2D, tex_id, GL_TEXTURE_COMPARE_MODE, GL_NONE);
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
            (sampling.filter == eTexFilter::Trilinear || sampling.filter == eTexFilter::Bilinear)) {
            ren_glGenerateTextureMipmap_Comp(GL_TEXTURE_CUBE_MAP, tex_id);
        }
    }

    params_.sampling = sampling;
}

void Ren::Texture2D::SetSubImage(const int level, const int offsetx, const int offsety, const int sizex,
                                 const int sizey, const Ren::eTexFormat format, const void *data, const int data_len) {
    assert(format == params_.format);
    assert(params_.samples == 1);
    assert(offsetx >= 0 && offsetx + sizex <= std::max(params_.w >> level, 1));
    assert(offsety >= 0 && offsety + sizey <= std::max(params_.h >> level, 1));

    if (IsCompressedFormat(format)) {
        ren_glCompressedTextureSubImage2D_Comp(GL_TEXTURE_2D, GLuint(handle_.id), GLint(level), GLint(offsetx),
                                               GLint(offsety), GLsizei(sizex), GLsizei(sizey),
                                               GLInternalFormatFromTexFormat(format, (params_.flags & TexSRGB) != 0),
                                               GLsizei(data_len), data);
    } else {
        ren_glTextureSubImage2D_Comp(GL_TEXTURE_2D, GLuint(handle_.id), level, offsetx, offsety, sizex, sizey,
                                     GLFormatFromTexFormat(format), GLTypeFromTexFormat(format), data);
    }

    if (offsetx == 0 && offsety == 0 && sizex == std::max(params_.w >> level, 1) &&
        sizey == std::max(params_.h >> level, 1)) {
        // consider this level initialized
        initialized_mips_ |= (1u << level);
    }
}

Ren::SyncFence Ren::Texture2D::SetSubImage(const int level, const int offsetx, const int offsety, const int sizex,
                                           const int sizey, const Ren::eTexFormat format, const Buffer &sbuf,
                                           const int data_off, const int data_len) {
    assert(format == params_.format);
    assert(params_.samples == 1);
    assert(offsetx >= 0 && offsetx + sizex <= std::max(params_.w >> level, 1));
    assert(offsety >= 0 && offsety + sizey <= std::max(params_.h >> level, 1));

    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, sbuf.id());

    if (IsCompressedFormat(format)) {
        ren_glCompressedTextureSubImage2D_Comp(GL_TEXTURE_2D, GLuint(handle_.id), GLint(level), GLint(offsetx),
                                               GLint(offsety), GLsizei(sizex), GLsizei(sizey),
                                               GLInternalFormatFromTexFormat(format, (params_.flags & TexSRGB) != 0),
                                               GLsizei(data_len), reinterpret_cast<const void *>(uintptr_t(data_off)));
    } else {
        ren_glTextureSubImage2D_Comp(GL_TEXTURE_2D, GLuint(handle_.id), level, offsetx, offsety, sizex, sizey,
                                     GLFormatFromTexFormat(format), GLTypeFromTexFormat(format),
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

    Free();

    handle_ = exchange(rhs.handle_, {});
    buf_ = std::move(rhs.buf_);
    params_ = exchange(rhs.params_, {});
    name_ = std::move(rhs.name_);

    RefCounter::operator=(std::move(rhs));

    return (*this);
}

void Ren::Texture1D::Init(BufferRef buf, const eTexFormat format, const uint32_t offset, const uint32_t size,
                          ILog *log) {
    Free();

    GLuint tex_id;
    glGenTextures(1, &tex_id);
    glBindTexture(GL_TEXTURE_BUFFER, tex_id);

    glTexBufferRange(GL_TEXTURE_BUFFER, GLInternalFormatFromTexFormat(format, false /* is_srgb */), GLuint(buf->id()),
                     offset, size);

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

uint32_t Ren::GLFormatFromTexFormat(const eTexFormat format) { return g_gl_formats[size_t(format)]; }

uint32_t Ren::GLInternalFormatFromTexFormat(const eTexFormat format, const bool is_srgb) {
    const uint32_t ret = g_gl_internal_formats[size_t(format)];
    return is_srgb ? Ren::ToSRGBFormat(ret) : ret;
}

uint32_t Ren::GLTypeFromTexFormat(const eTexFormat format) { return g_gl_types[size_t(format)]; }

uint32_t Ren::GLBindTarget(const eBindTarget binding) { return gl_binding_targets[size_t(binding)]; }

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

#ifdef _MSC_VER
#pragma warning(pop)
#endif
