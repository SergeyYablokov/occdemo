#include "TextureRegion.h"

#include "TextureAtlas.h"

#include "stb/stb_image.h"

Ren::TextureRegion::TextureRegion(const char *name, TextureAtlasArray *atlas, const int texture_pos[3])
    : name_(name), atlas_(atlas) {
    std::memcpy(texture_pos_, texture_pos, 3 * sizeof(int));
}

Ren::TextureRegion::TextureRegion(const char *name, const void *data, int size, const Tex2DParams &p,
                                  TextureAtlasArray *atlas, eTexLoadStatus *load_status)
    : name_(name) {
    Init(data, size, p, atlas, load_status);
}

Ren::TextureRegion::~TextureRegion() {
    if (atlas_) {
        atlas_->Free(texture_pos_);
    }
}

Ren::TextureRegion &Ren::TextureRegion::operator=(TextureRegion &&rhs) noexcept {
    RefCounter::operator=(std::move(rhs));

    if (atlas_) {
        atlas_->Free(texture_pos_);
    }

    name_ = std::move(rhs.name_);
    atlas_ = exchange(rhs.atlas_, nullptr);
    std::memcpy(texture_pos_, rhs.texture_pos_, 3 * sizeof(int));
    params_ = rhs.params_;
    ready_ = rhs.ready_;

    return (*this);
}

void Ren::TextureRegion::Init(const void *data, const int size, const Tex2DParams &p, TextureAtlasArray *atlas,
                              eTexLoadStatus *load_status) {
    if (!data) {
        const unsigned char cyan[40] = {0, 255, 255, 255};
        Tex2DParams _p;
        _p.w = _p.h = 1;
        _p.format = eTexFormat::RawRGBA8888;
        InitFromRAWData(cyan, 4, _p, atlas);
        // mark it as not ready
        ready_ = false;
        if (load_status) {
            (*load_status) = eTexLoadStatus::CreatedDefault;
        }
    } else {
        if (atlas_) {
            atlas_->Free(texture_pos_);
        }

        if (name_.EndsWith(".tga") != 0 || name_.EndsWith(".TGA") != 0) {
            InitFromTGAFile(data, size, p, atlas);
        } else if (name_.EndsWith(".png") != 0 || name_.EndsWith(".PNG") != 0) {
            InitFromPNGFile(data, size, p, atlas);
        } else {
            InitFromRAWData(data, size, p, atlas);
        }
        ready_ = true;
        if (load_status) {
            (*load_status) = eTexLoadStatus::CreatedFromData;
        }
    }
}

void Ren::TextureRegion::InitFromRAWData(const void *data, const int /*size*/, const Tex2DParams &p,
                                         TextureAtlasArray *atlas) {
    const int res[2] = {p.w, p.h};
    const int node = atlas->Allocate(data, p.format, res, texture_pos_, 1);
    if (node != -1) {
        atlas_ = atlas;
        params_ = p;
    }
}

void Ren::TextureRegion::InitFromTGAFile(const void *data, const int /*size*/, const Tex2DParams &p,
                                         TextureAtlasArray *atlas) {
    int w = 0, h = 0;
    eTexFormat format = eTexFormat::Undefined;
    std::unique_ptr<uint8_t[]> image_data = ReadTGAFile(data, w, h, format);

    Tex2DParams _p = p;
    _p.w = w;
    _p.h = h;
    _p.format = format;

    InitFromRAWData(image_data.get(), 0, _p, atlas);
}

void Ren::TextureRegion::InitFromPNGFile(const void *data, const int size, const Tex2DParams &p,
                                         TextureAtlasArray *atlas) {
    int w, h, channels;
    unsigned char *const image_data = stbi_load_from_memory((const uint8_t *)data, size, &w, &h, &channels, 0);
    if (image_data) {
        Tex2DParams _p = p;
        _p.w = w;
        _p.h = h;
        _p.format = eTexFormat::RawRGBA8888;

        InitFromRAWData(image_data, 0, _p, atlas);

        free(image_data);
    }
}