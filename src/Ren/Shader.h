#pragma once

#include "String.h"

namespace Ren {
struct Descr {
    String name;
    int loc = -1;
};
inline bool operator==(const Descr &lhs, const Descr &rhs) { return lhs.loc == rhs.loc && lhs.name == rhs.name; }
typedef Descr Attribute;
typedef Descr Uniform;
typedef Descr UniformBlock;

enum class eShaderType { None, Vert, Frag, Tesc, Tese, Comp, _Count };
enum class eShaderLoadStatus { Found, SetToDefault, CreatedFromData };
} // namespace Ren

#if defined(USE_GL_RENDER)
#include "ShaderGL.h"
#elif defined(USE_VK_RENDER)
#include "ShaderVK.h"
#elif defined(USE_SW_RENDER)
#error "TODO"
#endif
