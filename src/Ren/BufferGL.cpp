﻿#include "Buffer.h"

#include <algorithm>
#include <cassert>

#include "GL.h"
#include "Log.h"

namespace Ren {
const uint32_t g_gl_buf_targets[] = {
    0xffffffff,               // Undefined
    GL_ARRAY_BUFFER,          // VertexAttribs
    GL_ELEMENT_ARRAY_BUFFER,  // VertexIndices
    GL_TEXTURE_BUFFER,        // Texture
    GL_UNIFORM_BUFFER,        // Uniform
    GL_SHADER_STORAGE_BUFFER, // Storage
    GL_COPY_READ_BUFFER       // Stage
};
static_assert(sizeof(g_gl_buf_targets) / sizeof(g_gl_buf_targets[0]) == size_t(eBufType::_Count), "!");

GLenum GetGLBufUsage(const eBufType type) {
    if (type == eBufType::Stage) {
        return GL_STREAM_COPY;
    } else {
        return GL_STATIC_DRAW;
    }
}

#if !defined(__ANDROID__)
GLbitfield GetGLBufStorageFlags(const eBufType type) {
    GLbitfield flags = 0;

    if (type == eBufType::Stage) {
        flags |= (GL_CLIENT_STORAGE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_READ_BIT | GL_MAP_WRITE_BIT);
    }

    return flags;
}
#endif

} // namespace Ren

int Ren::Buffer::g_GenCounter = 0;

Ren::Buffer::Buffer(const char *name, ApiContext *api_ctx, const eBufType type, const uint32_t initial_size)
    : LinearAlloc(initial_size), name_(name), api_ctx_(api_ctx), type_(type), size_(0) {
    nodes_.reserve(1024);

    nodes_.emplace();
    nodes_[0].size = initial_size;

    Resize(initial_size);
}

Ren::Buffer::~Buffer() { Free(); }

Ren::Buffer &Ren::Buffer::operator=(Buffer &&rhs) noexcept {
    RefCounter::operator=(std::move((RefCounter &)rhs));
    LinearAlloc::operator=(std::move((LinearAlloc &)rhs));

    Free();

    assert(mapped_offset_ == 0xffffffff);
    assert(mapped_ptr_ == nullptr);

    api_ctx_ = exchange(rhs.api_ctx_, nullptr);
    handle_ = exchange(rhs.handle_, {});
    name_ = std::move(rhs.name_);

    type_ = exchange(rhs.type_, eBufType::Undefined);

    size_ = exchange(rhs.size_, 0);
    mapped_ptr_ = exchange(rhs.mapped_ptr_, nullptr);
    mapped_offset_ = exchange(rhs.mapped_offset_, 0xffffffff);

#ifndef NDEBUG
    flushed_ranges_ = std::move(rhs.flushed_ranges_);
#endif

    return (*this);
}

uint32_t Ren::Buffer::AllocRegion(uint32_t req_size, const char *tag, const Buffer *init_buf, void *,
                                  uint32_t init_off) {
    const int i = Alloc_Recursive(0, req_size, tag);
    if (i != -1) {
        Node &n = nodes_[i];
        assert(n.size == req_size);

        if (init_buf) {
            glBindBuffer(GL_COPY_READ_BUFFER, GLuint(init_buf->handle_.id));
            glBindBuffer(GL_COPY_WRITE_BUFFER, GLuint(handle_.id));

            glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, GLintptr(init_off), GLintptr(n.offset),
                                GLsizeiptr(n.size));

            glBindBuffer(GL_COPY_READ_BUFFER, 0);
            glBindBuffer(GL_COPY_WRITE_BUFFER, 0);
        }

        return n.offset;
    } else {
        assert(false && "Not implemented!");
        Resize(size_ + req_size);
        return AllocRegion(req_size, tag, init_buf, nullptr, init_off);
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

    GLuint gl_buffer;
    glGenBuffers(1, &gl_buffer);
    glBindBuffer(g_gl_buf_targets[int(type_)], gl_buffer);
#if !defined(__ANDROID__)
    glBufferStorage(g_gl_buf_targets[int(type_)], size_, nullptr, GetGLBufStorageFlags(type_));
#else
    glBufferData(g_gl_buf_targets[int(type_)], size_, nullptr, GetGLBufUsage(type_));
#endif

    if (handle_.id) {
        glBindBuffer(g_gl_buf_targets[int(type_)], GLuint(handle_.id));
        glBindBuffer(GL_COPY_WRITE_BUFFER, gl_buffer);

        glCopyBufferSubData(g_gl_buf_targets[int(type_)], GL_COPY_WRITE_BUFFER, 0, 0, old_size);

        auto old_buffer = GLuint(handle_.id);
        glDeleteBuffers(1, &old_buffer);

        glBindBuffer(GL_COPY_WRITE_BUFFER, 0);
    }

    handle_.id = uint32_t(gl_buffer);
    handle_.generation = g_GenCounter++;
}

void Ren::Buffer::Free() {
    assert(mapped_offset_ == 0xffffffff && !mapped_ptr_);
    if (handle_.id) {
        auto gl_buf = GLuint(handle_.id);
        glDeleteBuffers(1, &gl_buf);
        handle_ = {};
        size_ = 0;
        nodes_.clear();
    }
}

uint8_t *Ren::Buffer::MapRange(const uint8_t dir, const uint32_t offset, const uint32_t size, const bool persistent) {
    assert(mapped_offset_ == 0xffffffff && !mapped_ptr_);
    assert(offset + size <= size_);

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

    GLbitfield buf_map_range_flags = GLbitfield(GL_MAP_UNSYNCHRONIZED_BIT) | GLbitfield(GL_MAP_FLUSH_EXPLICIT_BIT);

    if (persistent) {
        buf_map_range_flags |= GLbitfield(GL_MAP_PERSISTENT_BIT);
    }

    if (dir & BufMapRead) {
        buf_map_range_flags |= GLbitfield(GL_MAP_READ_BIT);
    }

    if (dir & BufMapWrite) {
        buf_map_range_flags |= GLbitfield(GL_MAP_WRITE_BIT);
        if ((dir & BufMapRead) == 0) {
            // write only case
            buf_map_range_flags |= GLbitfield(GL_MAP_INVALIDATE_RANGE_BIT);
        }
    }

    glBindBuffer(g_gl_buf_targets[int(type_)], GLuint(handle_.id));
    auto *ret = (uint8_t *)glMapBufferRange(g_gl_buf_targets[int(type_)], GLintptr(offset), GLsizeiptr(size),
                                            buf_map_range_flags);
    glBindBuffer(g_gl_buf_targets[int(type_)], GLuint(0));

    mapped_offset_ = offset;
    mapped_ptr_ = ret;

    return ret;
}

void Ren::Buffer::FlushMappedRange(uint32_t offset, uint32_t size) {
    assert(mapped_offset_ != 0xffffffff && mapped_ptr_);
    glBindBuffer(g_gl_buf_targets[int(type_)], GLuint(handle_.id));
    glFlushMappedBufferRange(g_gl_buf_targets[int(type_)], GLintptr(offset), GLsizeiptr(size));
#ifndef NDEBUG
    flushed_ranges_.emplace_back(std::make_pair(mapped_offset_ + offset, size),
                                 SyncFence{glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0)});
#endif
    glBindBuffer(g_gl_buf_targets[int(type_)], GLuint(0));
}

void Ren::Buffer::Unmap() {
    assert(mapped_offset_ != 0xffffffff && mapped_ptr_);
    glBindBuffer(g_gl_buf_targets[int(type_)], GLuint(handle_.id));
    glUnmapBuffer(g_gl_buf_targets[int(type_)]);
    glBindBuffer(g_gl_buf_targets[int(type_)], GLuint(0));
    mapped_offset_ = 0xffffffff;
    mapped_ptr_ = nullptr;
}

void Ren::CopyBufferToBuffer(Buffer &src, uint32_t src_offset, Buffer &dst, uint32_t dst_offset, uint32_t size,
                             void *cmd_buf) {
    glBindBuffer(GL_COPY_READ_BUFFER, GLuint(src.id()));
    glBindBuffer(GL_COPY_WRITE_BUFFER, GLuint(dst.id()));
    glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, src_offset /* readOffset */,
                        dst_offset /* writeOffset */, size);
    glBindBuffer(GL_COPY_READ_BUFFER, 0);
    glBindBuffer(GL_COPY_WRITE_BUFFER, 0);
}

void Ren::GLUnbindBufferUnits(int start, int count) {
    for (int i = start; i < start + count; i++) {
        glBindBufferBase(GL_UNIFORM_BUFFER, i, 0);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, i, 0);
    }
}

void Ren::Buffer::Print(ILog *log) {
    log->Info("=================================================================");
    log->Info("Buffer %s, %f MB, %i nodes", name_.c_str(), float(size_) / (1024.0f * 1024.0f), int(nodes_.size()));
    PrintNode(0, "", true, log);
    log->Info("=================================================================");
}
