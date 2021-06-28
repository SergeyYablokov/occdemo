#include "Buffer.h"

#include <algorithm>
#include <cassert>

//#include "GL.h"
#include "Log.h"
#include "VKCtx.h"

namespace Ren {
/*const uint32_t g_gl_buf_targets[] = {
    0xffffffff,              // Undefined
    GL_ARRAY_BUFFER,         // VertexAttribs
    GL_ELEMENT_ARRAY_BUFFER, // VertexIndices
    GL_TEXTURE_BUFFER,       // Texture
    GL_UNIFORM_BUFFER,       // Uniform
    GL_SHADER_STORAGE_BUFFER // Storage
};
static_assert(sizeof(g_gl_buf_targets) / sizeof(g_gl_buf_targets[0]) ==
                  size_t(eBufType::_Count),
              "!");

GLenum GetGLBufUsage(const eBufAccessType access, const eBufAccessFreq freq) {
    if (access == eBufAccessType::Draw) {
        if (freq == eBufAccessFreq::Stream) {
            return GL_STREAM_DRAW;
        } else if (freq == eBufAccessFreq::Static) {
            return GL_STATIC_DRAW;
        } else if (freq == eBufAccessFreq::Dynamic) {
            return GL_DYNAMIC_DRAW;
        } else {
            assert(false);
        }
    } else if (access == eBufAccessType::Read) {
        if (freq == eBufAccessFreq::Stream) {
            return GL_STREAM_READ;
        } else if (freq == eBufAccessFreq::Static) {
            return GL_STATIC_READ;
        } else if (freq == eBufAccessFreq::Dynamic) {
            return GL_DYNAMIC_READ;
        } else {
            assert(false);
        }
    } else if (access == eBufAccessType::Copy) {
        if (freq == eBufAccessFreq::Stream) {
            return GL_STREAM_COPY;
        } else if (freq == eBufAccessFreq::Static) {
            return GL_STATIC_COPY;
        } else if (freq == eBufAccessFreq::Dynamic) {
            return GL_DYNAMIC_COPY;
        } else {
            assert(false);
        }
    } else {
        assert(false);
    }
    return 0xffffffff;
}*/

VkBufferUsageFlags GetVkBufferUsageFlags(const eBufType type) {
    VkBufferUsageFlags flags = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

    if (type == eBufType::VertexAttribs) {
        flags |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    } else if (type == eBufType::VertexIndices) {
        flags |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    } else if (type == eBufType::Texture) {
        flags |= VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT;
    } else if (type == eBufType::Storage) {
        flags |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    } else if (type == eBufType::Stage) {
    }

    return flags;
}

VkMemoryPropertyFlags GetVkMemoryPropertyFlags(const eBufType type) {
    return (type == eBufType::Stage) ? VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT : VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
}

uint32_t FindMemoryType(const VkPhysicalDeviceMemoryProperties *mem_properties, uint32_t mem_type_bits,
                        VkMemoryPropertyFlags desired_mem_flags) {
    for (uint32_t i = 0; i < 32; i++) {
        const VkMemoryType mem_type = mem_properties->memoryTypes[i];
        if (mem_type_bits & 1u) {
            if ((mem_type.propertyFlags & desired_mem_flags) == desired_mem_flags) {
                return i;
            }
        }
        mem_type_bits = (mem_type_bits >> 1u);
    }
    return 0xffffffff;
}
} // namespace Ren

int Ren::Buffer::g_GenCounter = 0;

Ren::Buffer::Buffer(const char *name, VkContext *ctx, const eBufType type, const eBufAccessType access,
                    const eBufAccessFreq freq, const uint32_t initial_size)
    : name_(name), ctx_(ctx), type_(type), access_(access), freq_(freq), size_(0) {
    nodes_.reserve(1024);

    nodes_.emplace();
    nodes_[0].size = initial_size;

    Resize(initial_size);
}

Ren::Buffer::~Buffer() {
    if (handle_.buf != VK_NULL_HANDLE) {
        vkDestroyBuffer(ctx_->device, handle_.buf, nullptr);
        vkFreeMemory(ctx_->device, mem_, nullptr);
    }
}

Ren::Buffer &Ren::Buffer::operator=(Buffer &&rhs) noexcept {
    RefCounter::operator=(std::move((RefCounter &)rhs));

    if (handle_.buf != VK_NULL_HANDLE) {
        vkDestroyBuffer(ctx_->device, handle_.buf, nullptr);
        vkFreeMemory(ctx_->device, mem_, nullptr);
    }

    assert(!is_mapped_);

    handle_ = exchange(rhs.handle_, {});
    name_ = std::move(rhs.name_);
    ctx_ = exchange(rhs.ctx_, nullptr);
    mem_ = exchange(rhs.mem_, {});

    type_ = exchange(rhs.type_, eBufType::Undefined);

    access_ = rhs.access_;
    freq_ = rhs.freq_;

    size_ = exchange(rhs.size_, 0);
    nodes_ = std::move(rhs.nodes_);
    is_mapped_ = exchange(rhs.is_mapped_, false);

#ifndef NDEBUG
    flushed_ranges_ = std::move(rhs.flushed_ranges_);
#endif

    last_access_mask = exchange(rhs.last_access_mask, 0);
    last_stage_mask = exchange(rhs.last_stage_mask, 0);

    return (*this);
}

int Ren::Buffer::Alloc_Recursive(const int i, const uint32_t req_size, const char *tag) {
    if (!nodes_[i].is_free || req_size > nodes_[i].size) {
        return -1;
    }

    int ch0 = nodes_[i].child[0], ch1 = nodes_[i].child[1];

    if (ch0 != -1) {
        const int new_node = Alloc_Recursive(ch0, req_size, tag);
        if (new_node != -1) {
            return new_node;
        }

        return Alloc_Recursive(ch1, req_size, tag);
    } else {
        if (req_size == nodes_[i].size) {
#ifndef NDEBUG
            strncpy(nodes_[i].tag, tag, 31);
#endif
            nodes_[i].is_free = false;
            return i;
        }

        nodes_[i].child[0] = ch0 = nodes_.emplace();
        nodes_[i].child[1] = ch1 = nodes_.emplace();

        Node &n = nodes_[i];

        nodes_[ch0].offset = n.offset;
        nodes_[ch0].size = req_size;
        nodes_[ch1].offset = n.offset + req_size;
        nodes_[ch1].size = n.size - req_size;
        nodes_[ch0].parent = nodes_[ch1].parent = i;

        return Alloc_Recursive(ch0, req_size, tag);
    }
}

int Ren::Buffer::Find_Recursive(const int i, const uint32_t offset) const {
    if ((nodes_[i].is_free && !nodes_[i].has_children()) || offset < nodes_[i].offset ||
        offset > (nodes_[i].offset + nodes_[i].size)) {
        return -1;
    }

    const int ch0 = nodes_[i].child[0], ch1 = nodes_[i].child[1];

    if (ch0 != -1) {
        const int ndx = Find_Recursive(ch0, offset);
        if (ndx != -1) {
            return ndx;
        }
        return Find_Recursive(ch1, offset);
    } else {
        if (offset == nodes_[i].offset) {
            return i;
        } else {
            return -1;
        }
    }
}

bool Ren::Buffer::Free_Node(int i) {
    if (i == -1 || nodes_[i].is_free) {
        return false;
    }

    nodes_[i].is_free = true;

    int par = nodes_[i].parent;
    while (par != -1) {
        int ch0 = nodes_[par].child[0], ch1 = nodes_[par].child[1];

        if (!nodes_[ch0].has_children() && nodes_[ch0].is_free && !nodes_[ch1].has_children() && nodes_[ch1].is_free) {

            nodes_.erase(ch0);
            nodes_.erase(ch1);

            nodes_[par].child[0] = nodes_[par].child[1] = -1;

            i = par;
            par = nodes_[par].parent;
        } else {
            par = -1;
        }
    }

    { // merge empty nodes
        int par = nodes_[i].parent;
        while (par != -1 && nodes_[par].child[0] == i && !nodes_[i].has_children()) {
            int gr_par = nodes_[par].parent;
            if (gr_par != -1 && nodes_[gr_par].has_children()) {
                int ch0 = nodes_[gr_par].child[0], ch1 = nodes_[gr_par].child[1];

                if (!nodes_[ch0].has_children() && nodes_[ch0].is_free && ch1 == par) {
                    assert(nodes_[ch0].offset + nodes_[ch0].size == nodes_[i].offset);
                    nodes_[ch0].size += nodes_[i].size;
                    nodes_[gr_par].child[1] = nodes_[par].child[1];
                    nodes_[nodes_[par].child[1]].parent = gr_par;

                    nodes_.erase(i);
                    nodes_.erase(par);

                    i = ch0;
                    par = gr_par;
                } else {
                    break;
                }
            } else {
                break;
            }
        }
    }

    return true;
}

void Ren::Buffer::PrintNode(int i, std::string prefix, bool is_tail, ILog *log) {
    const auto &node = nodes_[i];
    if (is_tail) {
        if (!node.has_children() && node.is_free) {
            log->Info("%s+- [0x%08x..0x%08x) <free>", prefix.c_str(), node.offset, node.offset + node.size);
        } else {
#ifndef NDEBUG
            log->Info("%s+- [0x%08x..0x%08x) <%s>", prefix.c_str(), node.offset, node.offset + node.size, node.tag);
#else
            log->Info("%s+- [0x%08x..0x%08x) <occupied>", prefix.c_str(), node.offset, node.offset + node.size);
#endif
        }
        prefix += "   ";
    } else {
        if (!node.has_children() && node.is_free) {
            log->Info("%s|- [0x%08x..0x%08x) <free>", prefix.c_str(), node.offset, node.offset + node.size);
        } else {
#ifndef NDEBUG
            log->Info("%s|- [0x%08x..0x%08x) <%s>", prefix.c_str(), node.offset, node.offset + node.size, node.tag);
#else
            log->Info("%s|- [0x%08x..0x%08x) <occupied>", prefix.c_str(), node.offset, node.offset + node.size);
#endif
        }
        prefix += "|  ";
    }

    if (node.child[0] != -1) {
        PrintNode(node.child[0], prefix, false, log);
    }

    if (node.child[1] != -1) {
        PrintNode(node.child[1], prefix, true, log);
    }
}

uint32_t Ren::Buffer::AllocRegion(uint32_t req_size, const char *tag, const Buffer *init_buf, void *_cmd_buf,
                                  uint32_t init_off) {
    const int i = Alloc_Recursive(0, req_size, tag);
    if (i != -1) {
        Node &n = nodes_[i];
        assert(n.size == req_size);

        if (init_buf) {
            assert(init_buf->type_ == eBufType::Stage);
            VkCommandBuffer cmd_buf = reinterpret_cast<VkCommandBuffer>(_cmd_buf);

            int barriers_count = 0;
            VkBufferMemoryBarrier barriers[2] = {};
            barriers[0].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
            barriers[0].srcAccessMask = init_buf->last_access_mask;
            barriers[0].dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            barriers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barriers[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barriers[0].buffer = init_buf->handle().buf;
            barriers[0].offset = VkDeviceSize{init_off};
            barriers[0].size = VkDeviceSize{req_size};
            ++barriers_count;

            if (last_access_mask) {
                barriers[1].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
                barriers[1].srcAccessMask = this->last_access_mask;
                barriers[1].dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                barriers[1].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barriers[1].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barriers[1].buffer = handle_.buf;
                barriers[1].offset = VkDeviceSize{n.offset};
                barriers[1].size = VkDeviceSize{n.size};
                ++barriers_count;
            }

            vkCmdPipelineBarrier(cmd_buf, init_buf->last_stage_mask | this->last_stage_mask,
                                 VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, barriers_count, barriers, 0, nullptr);

            VkBufferCopy region_to_copy = {};
            region_to_copy.srcOffset = VkDeviceSize{init_off};
            region_to_copy.dstOffset = VkDeviceSize{n.offset};
            region_to_copy.size = VkDeviceSize{n.size};

            vkCmdCopyBuffer(cmd_buf, init_buf->handle_.buf, handle_.buf, 1, &region_to_copy);

            init_buf->last_access_mask = VK_ACCESS_TRANSFER_READ_BIT;
            init_buf->last_stage_mask = VK_PIPELINE_STAGE_TRANSFER_BIT;

            this->last_access_mask = VK_ACCESS_TRANSFER_WRITE_BIT;
            this->last_stage_mask = VK_PIPELINE_STAGE_TRANSFER_BIT;
        }

        return n.offset;
    } else {
        assert(false && "Not implemented!");
        Resize(size_ + req_size);
        return AllocRegion(req_size, tag, init_buf, _cmd_buf, init_off);
    }
}

bool Ren::Buffer::FreeRegion(uint32_t offset) {
    const int i = Find_Recursive(0, offset);
    return Free_Node(i);
}

void Ren::Buffer::Resize(uint32_t new_size) {
    if (size_ >= new_size) {
        return;
    }

    const uint32_t old_size = size_;

    if (!size_) {
        size_ = new_size;
        assert(size_ > 0);
    }

    while (size_ < new_size) {
        size_ *= 2;
    }

    VkBufferCreateInfo buf_create_info = {};
    buf_create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buf_create_info.size = VkDeviceSize(new_size);
    buf_create_info.usage = GetVkBufferUsageFlags(type_);
    buf_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkBuffer new_buf = {};
    VkResult res = vkCreateBuffer(ctx_->device, &buf_create_info, nullptr, &new_buf);
    assert(res == VK_SUCCESS && "Failed to create vertex buffer!");

    VkMemoryRequirements memory_requirements = {};
    vkGetBufferMemoryRequirements(ctx_->device, new_buf, &memory_requirements);

    VkMemoryAllocateInfo buf_alloc_info = {};
    buf_alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    buf_alloc_info.allocationSize = memory_requirements.size;
    buf_alloc_info.memoryTypeIndex =
        FindMemoryType(&ctx_->mem_properties, memory_requirements.memoryTypeBits, GetVkMemoryPropertyFlags(type_));

    VkDeviceMemory buffer_mem = {};
    res = vkAllocateMemory(ctx_->device, &buf_alloc_info, nullptr, &buffer_mem);
    assert(res == VK_SUCCESS && "Failed to allocate memory!");

    res = vkBindBufferMemory(ctx_->device, new_buf, buffer_mem, 0 /* offset */);
    assert(res == VK_SUCCESS && "Failed to bind memory!");

    if (handle_.buf != VK_NULL_HANDLE) {
        { // copy previous buffer contents
            VkCommandBuffer cmd_buf = BegSingleTimeCommands(ctx_->device, ctx_->temp_command_pool);

            VkBufferCopy region_to_copy = {};
            region_to_copy.size = VkDeviceSize{old_size};

            vkCmdCopyBuffer(cmd_buf, handle_.buf, new_buf, 1, &region_to_copy);

            EndSingleTimeCommands(ctx_->device, ctx_->graphics_queue, cmd_buf, ctx_->temp_command_pool);
        }

        // destroy previous buffer
        vkDestroyBuffer(ctx_->device, handle_.buf, nullptr);
        vkFreeMemory(ctx_->device, mem_, nullptr);
    }

    handle_.buf = new_buf;
    handle_.generation = g_GenCounter++;
    mem_ = buffer_mem;
}

uint8_t *Ren::Buffer::MapRange(const uint8_t dir, const uint32_t offset, const uint32_t size, const bool persistent) {
    assert(!is_mapped_);
    assert(offset + size <= size_);
    assert(type_ == eBufType::Stage);

#ifndef NDEBUG
    for (auto it = std::begin(flushed_ranges_); it != std::end(flushed_ranges_);) {
        if (offset + size >= it->range.first && offset < it->range.first + it->range.second) {
            const WaitResult res = it->fence.ClientWaitSync(0);
            assert(res == WaitResult::Success);
            it = flushed_ranges_.erase(it);
        } else {
            ++it;
        }
    }
#endif

    const uint32_t align_to = uint32_t(ctx_->device_properties.limits.nonCoherentAtomSize);
    const uint32_t offset_aligned = offset - (offset % align_to);
    const uint32_t size_aligned = align_to * ((size + (offset % align_to) + align_to - 1) / align_to);

    void *mapped = nullptr;
    VkResult res =
        vkMapMemory(ctx_->device, mem_, VkDeviceSize(offset_aligned), VkDeviceSize(size_aligned), 0, &mapped);
    assert(res == VK_SUCCESS && "Failed to map memory!");

    is_mapped_ = true;
    return reinterpret_cast<uint8_t *>(mapped) + (offset % align_to);
}

void Ren::Buffer::FlushRange(uint32_t offset, uint32_t size) {
    const uint32_t align_to = uint32_t(ctx_->device_properties.limits.nonCoherentAtomSize);
    const uint32_t offset_aligned = offset - (offset % align_to);
    const uint32_t size_aligned = align_to * ((size + (offset % align_to) + align_to - 1) / align_to);

    VkMappedMemoryRange range;
    range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
    range.memory = mem_;
    range.offset = VkDeviceSize(offset_aligned);
    range.size = VkDeviceSize(size_aligned);
    range.pNext = nullptr;

    VkResult res = vkFlushMappedMemoryRanges(ctx_->device, 1, &range);
    assert(res == VK_SUCCESS && "Failed to flush memory range!");

    // res = vkInvalidateMappedMemoryRanges(ctx_->device, 1, &range);
    // assert(res == VK_SUCCESS && "Failed to invalidate memory range!");

#ifndef NDEBUG
    // flushed_ranges_.emplace_back(std::make_pair(offset, size),
    //                             SyncFence{glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0)});
#endif

    last_access_mask |= VK_ACCESS_HOST_WRITE_BIT;
    last_stage_mask |= VK_PIPELINE_STAGE_HOST_BIT;
}

void Ren::Buffer::Unmap() {
    assert(is_mapped_);
    vkUnmapMemory(ctx_->device, mem_);
    is_mapped_ = false;
}

void Ren::Buffer::Print(ILog *log) {
    log->Info("=================================================================");
    log->Info("Buffer %s, %f MB, %i nodes", name_.c_str(), float(size_) / (1024.0f * 1024.0f), int(nodes_.size()));
    PrintNode(0, "", true, log);
    log->Info("=================================================================");
}
