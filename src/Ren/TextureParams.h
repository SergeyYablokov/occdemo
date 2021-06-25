#pragma once

#include <cstdint>

#include "SamplingParams.h"

namespace Ren {
enum class eTexFormat : uint8_t {
    Undefined,
    RawRGB888,
    RawRGBA8888,
    RawRGBA8888Snorm,
    RawR32F,
    RawR16F,
    RawR8,
    RawRG88,
    RawRGB32F,
    RawRGBA32F,
    RawRGBE8888,
    RawRGB16F,
    RawRGBA16F,
    RawRG16,
    RawRG16U,
    RawRG16F,
    RawRG32F,
    RawRG32U,
    RawRGB10_A2,
    RawRG11F_B10F,
    Depth16,
    Depth24Stencil8,
#ifndef __ANDROID__
    Depth32,
#endif
    Compressed_DXT1,
    Compressed_DXT3,
    Compressed_DXT5,
    Compressed_ASTC,
    None,
    _Count
};

inline bool IsDepthFormat(const eTexFormat format) {
    return format == eTexFormat::Depth16 || format == eTexFormat::Depth24Stencil8
#ifndef __ANDROID__
           || format == eTexFormat::Depth32;
#else
        ;
#endif
}

#if defined(__ANDROID__)
const Ren::eTexFormat DefaultCompressedRGBA = Ren::eTexFormat::Compressed_ASTC;
#else
const Ren::eTexFormat DefaultCompressedRGBA = Ren::eTexFormat::Compressed_DXT5;
#endif

enum class eTexBlock : uint8_t {
    _4x4,
    _5x4,
    _5x5,
    _6x5,
    _6x6,
    _8x5,
    _8x6,
    _8x8,
    _10x5,
    _10x6,
    _10x8,
    _10x10,
    _12x10,
    _12x12,
    _None
};

struct Tex2DParams {
    uint16_t w = 0, h = 0;
    uint16_t flags = 0;
    uint8_t mip_count = 0;
    uint8_t _pad = 0;
    uint8_t cube = 0;
    uint8_t samples = 1;
    uint8_t fallback_color[4] = {0, 255, 255, 255};
    eTexFormat format = eTexFormat::Undefined;
    eTexBlock block = eTexBlock::_None;
    SamplingParams sampling;
};
static_assert(sizeof(Tex2DParams) == 22, "!");

inline bool operator==(const Tex2DParams &lhs, const Tex2DParams &rhs) {
    return lhs.w == rhs.w && lhs.h == rhs.h && lhs.flags == rhs.flags && lhs.mip_count == rhs.mip_count &&
           lhs.cube == rhs.cube && lhs.samples == rhs.samples && lhs.fallback_color[0] == rhs.fallback_color[0] &&
           lhs.fallback_color[1] == rhs.fallback_color[1] && lhs.fallback_color[2] == rhs.fallback_color[2] &&
           lhs.fallback_color[3] == rhs.fallback_color[3] && lhs.format == rhs.format && lhs.sampling == rhs.sampling;
}
inline bool operator!=(const Tex2DParams &lhs, const Tex2DParams &rhs) { return !operator==(lhs, rhs); }

enum class eTexLoadStatus { Found, Reinitialized, CreatedDefault, CreatedFromData };

} // namespace Ren