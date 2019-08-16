#pragma once

#include <cstdint>

#include <Ren/Texture.h>
#include <Sys/SmallVector.h>

class ProbeStorage {
public:
    ProbeStorage();
    ~ProbeStorage();

    ProbeStorage(const ProbeStorage &rhs) = delete;
    ProbeStorage &operator=(const ProbeStorage &rhs) = delete;

    int Allocate();
    void Free(int i);
    void Clear();

    Ren::eTexFormat format() const { return format_; }
    int res() const { return res_; }
    int size() const { return size_; }
    int capacity() const { return capacity_; }
    int max_level() const { return max_level_; }
    int reserved_temp_layer() const { return reserved_temp_layer_; }

    Ren::TexHandle handle() const { return handle_; }

    void Resize(Ren::eTexFormat format, int res, int capacity, Ren::ILog *log);

    bool SetPixelData(int level, int layer, int face, Ren::eTexFormat format,
            const uint8_t *data, int data_len, Ren::ILog *log);
    bool GetPixelData(int level, int layer, int face, int buf_size, uint8_t *out_pixels, Ren::ILog *log) const;
private:
    Ren::eTexFormat format_;
    int res_, size_, capacity_, max_level_;
    int reserved_temp_layer_;
    Sys::SmallVector<int, 32> free_indices_;
    Ren::TexHandle handle_;
};