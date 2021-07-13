﻿#include "Buffer.h"

#include <algorithm>
#include <cassert>

//#include "GL.h"
#include "Log.h"
#include "VKCtx.h"

namespace Ren {
VkBufferUsageFlags GetVkBufferUsageFlags(const eBufType type) {
    VkBufferUsageFlags flags = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

    if (type == eBufType::VertexAttribs) {
        flags |= (VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    } else if (type == eBufType::VertexIndices) {
        flags |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    } else if (type == eBufType::Texture) {
        flags |= (VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    } else if (type == eBufType::Uniform) {
        flags |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
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
                        VkMemoryPropertyFlags desired_mem_flags);
} // namespace Ren

int Ren::Buffer::g_GenCounter = 0;

Ren::Buffer::Buffer(const char *name, ApiContext *api_ctx, const eBufType type, const uint32_t initial_size)
    : LinearAlloc(initial_size), name_(name), api_ctx_(api_ctx), type_(type), size_(0) {
    nodes_.reserve(1024);
    Resize(initial_size);
}

Ren::Buffer::~Buffer() { Free(); }

Ren::Buffer &Ren::Buffer::operator=(Buffer &&rhs) noexcept {
    RefCounter::operator=(static_cast<RefCounter &&>(rhs));
    LinearAlloc::operator=(static_cast<LinearAlloc &&>(rhs));

    if (handle_.buf != VK_NULL_HANDLE) {
        vkDestroyBuffer(api_ctx_->device, handle_.buf, nullptr);
        vkFreeMemory(api_ctx_->device, mem_, nullptr);
    }

    assert(!mapped_ptr_);
    assert(mapped_offset_ == 0xffffffff);

    api_ctx_ = exchange(rhs.api_ctx_, nullptr);
    handle_ = exchange(rhs.handle_, {});
    name_ = std::move(rhs.name_);
    mem_ = exchange(rhs.mem_, {});

    type_ = exchange(rhs.type_, eBufType::Undefined);

    size_ = exchange(rhs.size_, 0);
    mapped_ptr_ = exchange(rhs.mapped_ptr_, nullptr);
    mapped_offset_ = exchange(rhs.mapped_offset_, 0xffffffff);

#ifndef NDEBUG
    flushed_ranges_ = std::move(rhs.flushed_ranges_);
#endif

    last_access_mask = exchange(rhs.last_access_mask, 0);
    last_stage_mask = exchange(rhs.last_stage_mask, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);

    return (*this);
}

uint32_t Ren::Buffer::AllocRegion(uint32_t req_size, const char *tag, const Buffer *init_buf, void *_cmd_buf,
                                  uint32_t init_off) {
    const int i = Alloc_r(0, req_size, tag);
    if (i != -1) {
        const Node &n = nodes_[i];
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
    }

    return 0xffffffff;
}

bool Ren::Buffer::FreeRegion(uint32_t offset) {
    const int i = Find_r(0, offset);
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
    VkResult res = vkCreateBuffer(api_ctx_->device, &buf_create_info, nullptr, &new_buf);
    assert(res == VK_SUCCESS && "Failed to create vertex buffer!");

    VkMemoryRequirements memory_requirements = {};
    vkGetBufferMemoryRequirements(api_ctx_->device, new_buf, &memory_requirements);

    VkMemoryAllocateInfo buf_alloc_info = {};
    buf_alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    buf_alloc_info.allocationSize = memory_requirements.size;
    buf_alloc_info.memoryTypeIndex =
        FindMemoryType(&api_ctx_->mem_properties, memory_requirements.memoryTypeBits, GetVkMemoryPropertyFlags(type_));

    VkDeviceMemory buffer_mem = {};
    res = vkAllocateMemory(api_ctx_->device, &buf_alloc_info, nullptr, &buffer_mem);
    assert(res == VK_SUCCESS && "Failed to allocate memory!");

    res = vkBindBufferMemory(api_ctx_->device, new_buf, buffer_mem, 0 /* offset */);
    assert(res == VK_SUCCESS && "Failed to bind memory!");

    if (handle_.buf != VK_NULL_HANDLE) {
        { // copy previous buffer contents
            VkCommandBuffer cmd_buf = BegSingleTimeCommands(api_ctx_->device, api_ctx_->temp_command_pool);

            VkBufferCopy region_to_copy = {};
            region_to_copy.size = VkDeviceSize{old_size};

            vkCmdCopyBuffer(cmd_buf, handle_.buf, new_buf, 1, &region_to_copy);

            EndSingleTimeCommands(api_ctx_->device, api_ctx_->graphics_queue, cmd_buf, api_ctx_->temp_command_pool);
        }

        // destroy previous buffer
        vkDestroyBuffer(api_ctx_->device, handle_.buf, nullptr);
        vkFreeMemory(api_ctx_->device, mem_, nullptr);
    }

    handle_.buf = new_buf;
    handle_.generation = g_GenCounter++;
    mem_ = buffer_mem;
}

void Ren::Buffer::Free() {
    assert(mapped_offset_ == 0xffffffff && !mapped_ptr_);
    if (handle_.buf != VK_NULL_HANDLE) {
        api_ctx_->bufs_to_destroy[api_ctx_->backend_frame].push_back(handle_.buf);
        api_ctx_->mem_to_free[api_ctx_->backend_frame].push_back(mem_);

        handle_ = {};
        size_ = 0;
        LinearAlloc::Clear();
    }
}

uint8_t *Ren::Buffer::MapRange(const uint8_t dir, const uint32_t offset, const uint32_t size, const bool persistent) {
    assert(mapped_offset_ == 0xffffffff && !mapped_ptr_);
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

    const uint32_t align_to = uint32_t(api_ctx_->device_properties.limits.nonCoherentAtomSize);
    const uint32_t offset_aligned = offset - (offset % align_to);
    const uint32_t size_aligned = align_to * ((size + (offset % align_to) + align_to - 1) / align_to);

    void *mapped = nullptr;
    VkResult res =
        vkMapMemory(api_ctx_->device, mem_, VkDeviceSize(offset_aligned), VkDeviceSize(size_aligned), 0, &mapped);
    assert(res == VK_SUCCESS && "Failed to map memory!");

    mapped_ptr_ = reinterpret_cast<uint8_t *>(mapped);
    mapped_offset_ = offset;
    return reinterpret_cast<uint8_t *>(mapped) + (offset % align_to);
}

void Ren::Buffer::FlushMappedRange(uint32_t offset, uint32_t size) {
    // offset argument is relative to mapped range
    offset += mapped_offset_;

    const uint32_t align_to = uint32_t(api_ctx_->device_properties.limits.nonCoherentAtomSize);
    const uint32_t offset_aligned = offset - (offset % align_to);
    const uint32_t size_aligned = align_to * ((size + (offset % align_to) + align_to - 1) / align_to);

    VkMappedMemoryRange range;
    range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
    range.memory = mem_;
    range.offset = VkDeviceSize(offset_aligned);
    range.size = VkDeviceSize(size_aligned);
    range.pNext = nullptr;

    VkResult res = vkFlushMappedMemoryRanges(api_ctx_->device, 1, &range);
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
    assert(mapped_offset_ != 0xffffffff && mapped_ptr_);
    vkUnmapMemory(api_ctx_->device, mem_);
    mapped_ptr_ = nullptr;
    mapped_offset_ = 0xffffffff;
}

void Ren::Buffer::Print(ILog *log) {
    log->Info("=================================================================");
    log->Info("Buffer %s, %f MB, %i nodes", name_.c_str(), float(size_) / (1024.0f * 1024.0f), int(nodes_.size()));
    PrintNode(0, "", true, log);
    log->Info("=================================================================");
}

void Ren::CopyBufferToBuffer(Buffer &src, uint32_t src_offset, Buffer &dst, uint32_t dst_offset, uint32_t size,
                             void *_cmd_buf) {
    VkCommandBuffer cmd_buf = reinterpret_cast<VkCommandBuffer>(_cmd_buf);

    int barriers_count = 0;
    VkBufferMemoryBarrier barriers[2] = {};
    barriers[0].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    barriers[0].srcAccessMask = src.last_access_mask;
    barriers[0].dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    barriers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barriers[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barriers[0].buffer = src.handle().buf;
    barriers[0].offset = VkDeviceSize{src_offset};
    barriers[0].size = VkDeviceSize{size};
    ++barriers_count;

    barriers[1].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    barriers[1].srcAccessMask = dst.last_access_mask;
    barriers[1].dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barriers[1].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barriers[1].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barriers[1].buffer = dst.handle().buf;
    barriers[1].offset = VkDeviceSize{dst_offset};
    barriers[1].size = VkDeviceSize{size};
    ++barriers_count;

    vkCmdPipelineBarrier(cmd_buf, src.last_stage_mask | dst.last_stage_mask, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0,
                         nullptr, barriers_count, barriers, 0, nullptr);

    VkBufferCopy region_to_copy = {};
    region_to_copy.srcOffset = VkDeviceSize{src_offset};
    region_to_copy.dstOffset = VkDeviceSize{dst_offset};
    region_to_copy.size = VkDeviceSize{size};

    vkCmdCopyBuffer(cmd_buf, src.handle().buf, dst.handle().buf, 1, &region_to_copy);

    src.last_access_mask = VK_ACCESS_TRANSFER_READ_BIT;
    src.last_stage_mask = VK_PIPELINE_STAGE_TRANSFER_BIT;

    dst.last_access_mask = VK_ACCESS_TRANSFER_WRITE_BIT;
    dst.last_stage_mask = VK_PIPELINE_STAGE_TRANSFER_BIT;
}
