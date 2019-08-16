#pragma once

#include <cstdint>

#include "Fixed.h"

namespace Ren {
using Fixed8 = Fixed<int8_t, 3>;

enum class eTexFilter : uint8_t { NoFilter, Bilinear, Trilinear, BilinearNoMipmap, _Count };
enum class eTexRepeat : uint8_t { Repeat, ClampToEdge, ClampToBorder, WrapModesCount };

#undef Always
enum class eTexCompare : uint8_t { None, LEqual, GEqual, Less, Greater, Equal, NotEqual, Always, Never, _Count };

struct SamplingParams {
    eTexFilter filter = eTexFilter::NoFilter;
    eTexRepeat repeat = eTexRepeat::Repeat;
    eTexCompare compare = eTexCompare::None;
    Fixed8 lod_bias;
    Fixed8 min_lod = Fixed8::lowest(), max_lod = Fixed8::max();

    SamplingParams() = default;
    SamplingParams(eTexFilter _filter, eTexRepeat _repeat, eTexCompare _compare, Fixed8 _lod_bias, Fixed8 _min_lod,
                   Fixed8 _max_lod)
        : filter(_filter), repeat(_repeat), compare(_compare), lod_bias(_lod_bias), min_lod(_min_lod),
          max_lod(_max_lod) {}
};
static_assert(sizeof(SamplingParams) == 6, "!");

inline bool operator==(const SamplingParams &lhs, const SamplingParams &rhs) {
    return lhs.filter == rhs.filter && lhs.repeat == rhs.repeat && lhs.compare == rhs.compare &&
           lhs.lod_bias == rhs.lod_bias && lhs.min_lod == rhs.min_lod && lhs.max_lod == rhs.max_lod;
}

enum class eSamplerLoadStatus { Found, Created };

enum class eBindTarget : uint16_t { Tex2D, Tex2DMs, TexCubeArray, TexBuf, UBuf, _Count };
} // namespace Ren