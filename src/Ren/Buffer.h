#pragma once

#include <vector>

#include "Fence.h"
#include "SmallVector.h"
#include "Storage.h"
#include "String.h"

#if defined(USE_VK_RENDER)
#include "VK.h"
#endif

namespace Ren {
class ILog;
struct VkContext;

enum class eType : uint8_t { Float16, Float32, Uint32, Uint16UNorm, Int16SNorm, Uint8UNorm, Int32, _Count };

enum class eBufType : uint8_t { Undefined, VertexAttribs, VertexIndices, Texture, Uniform, Storage, Stage, _Count };
enum class eBufAccessType : uint8_t { Draw, Read, Copy };
enum class eBufAccessFreq : uint8_t {
    Stream, // modified once, used a few times
    Static, // modified once, used many times
    Dynamic // modified often, used many times
};

const uint8_t BufMapRead = (1u << 0u);
const uint8_t BufMapWrite = (1u << 1u);

struct BufHandle {
#if defined(USE_VK_RENDER)
    VkBuffer buf = VK_NULL_HANDLE;
#elif defined(USE_GL_RENDER) || defined(USE_SW_RENDER)
    uint32_t id = 0;
#endif
    uint32_t generation = 0;
};
inline bool operator==(BufHandle lhs, BufHandle rhs) {
    return
#if defined(USE_VK_RENDER)
        lhs.buf == rhs.buf &&
#elif defined(USE_GL_RENDER) || defined(USE_SW_RENDER)
        lhs.id == rhs.id &&
#endif
        lhs.generation == rhs.generation;
}

struct RangeFence {
    std::pair<uint32_t, uint32_t> range;
    SyncFence fence;

    RangeFence(const std::pair<uint32_t, uint32_t> _range, SyncFence &&_fence)
        : range(_range), fence(std::move(_fence)) {}
};

class Buffer : public RefCounter {
    struct Node {
        bool is_free = true;
        int parent = -1;
        int child[2] = {-1, -1};
        uint32_t offset = 0, size = 0;
#ifndef NDEBUG
        char tag[32] = {};
#endif

        bool has_children() const { return child[0] != -1 || child[1] != -1; }
    };

    BufHandle handle_;
    String name_;
#if defined(USE_VK_RENDER)
    VkContext *ctx_ = nullptr;
    VkDeviceMemory mem_ = VK_NULL_HANDLE;
#endif
    eBufType type_ = eBufType::Undefined;
    eBufAccessType access_;
    eBufAccessFreq freq_;
    uint32_t size_ = 0;
    SparseArray<Node> nodes_;
    bool is_mapped_ = false;
#ifndef NDEBUG
    SmallVector<RangeFence, 4> flushed_ranges_;
#endif

    int Alloc_Recursive(int i, uint32_t req_size, const char *tag);
    int Find_Recursive(int i, uint32_t offset) const;
    bool Free_Node(int i);

    void PrintNode(int i, std::string prefix, bool is_tail, ILog *log);

    static int g_GenCounter;

  public:
    Buffer() = default;
#if defined(USE_VK_RENDER)
    explicit Buffer(const char *name, VkContext *ctx, eBufType type, eBufAccessType access, eBufAccessFreq freq,
                    uint32_t initial_size);
#else
    explicit Buffer(const char *name, eBufType type, eBufAccessType access, eBufAccessFreq freq, uint32_t initial_size);
#endif
    Buffer(const Buffer &rhs) = delete;
    Buffer(Buffer &&rhs) noexcept { (*this) = std::move(rhs); }
    ~Buffer();

    Buffer &operator=(const Buffer &rhs) = delete;
    Buffer &operator=(Buffer &&rhs) noexcept;

    const String &name() const { return name_; }
    eBufType type() const { return type_; }
    eBufAccessType access() const { return access_; }
    eBufAccessFreq freq() const { return freq_; }
    uint32_t size() const { return size_; }

    BufHandle handle() const { return handle_; }
#if defined(USE_VK_RENDER)
    VkBuffer vk_handle() const { return handle_.buf; }
#elif defined(USE_GL_RENDER) || defined(USE_SW_RENDER)
    uint32_t id() const { return handle_.id; }
#endif
    uint32_t generation() const { return handle_.generation; }

    uint32_t AllocRegion(uint32_t size, const char *tag, const Buffer *init_buf = nullptr, void *cmd_buf = nullptr,
                         uint32_t init_off = 0);
    bool FreeRegion(uint32_t offset);

    void Resize(uint32_t new_size);

    uint8_t *Map(const uint8_t dir, const bool persistent = false) { return MapRange(dir, 0, size_, persistent); }
    uint8_t *MapRange(uint8_t dir, uint32_t offset, uint32_t size, bool persistent = false);
    void FlushRange(uint32_t offset, uint32_t size);
    void Unmap();

    void Print(ILog *log);

#if defined(USE_VK_RENDER)
    mutable VkAccessFlags last_access_mask = 0;
    mutable VkPipelineStageFlags last_stage_mask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
#endif
};

#if defined(USE_GL_RENDER)
void GLUnbindBufferUnits(int start, int count);
#endif

typedef StrongRef<Buffer> BufferRef;
typedef Storage<Buffer> BufferStorage;
} // namespace Ren