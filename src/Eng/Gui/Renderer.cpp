#include "Renderer.h"

int Gui::Renderer::g_instance_count = 0;

const uint8_t Gui::ColorWhite[4] = {255, 255, 255, 255};
const uint8_t Gui::ColorGrey[4] = {127, 127, 127, 255};
const uint8_t Gui::ColorBlack[4] = {0, 0, 0, 255};
const uint8_t Gui::ColorRed[4] = {255, 0, 0, 255};
const uint8_t Gui::ColorGreen[4] = {0, 255, 0, 255};
const uint8_t Gui::ColorBlue[4] = {0, 0, 255, 255};
const uint8_t Gui::ColorCyan[4] = {0, 255, 255, 255};
const uint8_t Gui::ColorMagenta[4] = {255, 0, 255, 255};
const uint8_t Gui::ColorYellow[4] = {255, 255, 0, 255};

void Gui::Renderer::PushClipArea(const Vec2f dims[2]) {
    clip_area_stack_[clip_area_stack_size_][0] = dims[0];
    clip_area_stack_[clip_area_stack_size_][1] = dims[0] + dims[1];
    if (clip_area_stack_size_) {
        clip_area_stack_[clip_area_stack_size_][0] =
            Max(clip_area_stack_[clip_area_stack_size_ - 1][0], clip_area_stack_[clip_area_stack_size_][0]);
        clip_area_stack_[clip_area_stack_size_][1] =
            Min(clip_area_stack_[clip_area_stack_size_ - 1][1], clip_area_stack_[clip_area_stack_size_][1]);
    }
    ++clip_area_stack_size_;
}

void Gui::Renderer::PopClipArea() { --clip_area_stack_size_; }

const Gui::Vec2f *Gui::Renderer::GetClipArea() const {
    if (clip_area_stack_size_) {
        return clip_area_stack_[clip_area_stack_size_ - 1];
    }
    return nullptr;
}

int Gui::Renderer::AcquireVertexData(vertex_t **vertex_data, int *vertex_avail, uint16_t **index_data,
                                     int *index_avail) {
    (*vertex_data) =
        vtx_stage_data_ + size_t(ctx_.frontend_frame) * MaxVerticesPerRange + vtx_count_[ctx_.frontend_frame];
    (*vertex_avail) = MaxVerticesPerRange - vtx_count_[ctx_.frontend_frame];

    (*index_data) =
        ndx_stage_data_ + size_t(ctx_.frontend_frame) * MaxIndicesPerRange + ndx_count_[ctx_.frontend_frame];
    (*index_avail) = MaxIndicesPerRange - ndx_count_[ctx_.frontend_frame];

    return vtx_count_[ctx_.frontend_frame];
}

void Gui::Renderer::SubmitVertexData(const int vertex_count, const int index_count) {
    assert((vtx_count_[ctx_.frontend_frame] + vertex_count) <= MaxVerticesPerRange &&
           (ndx_count_[ctx_.frontend_frame] + index_count) <= MaxIndicesPerRange);

    vtx_count_[ctx_.frontend_frame] += vertex_count;
    ndx_count_[ctx_.frontend_frame] += index_count;
}

void Gui::Renderer::PushImageQuad(const eDrawMode draw_mode, const int tex_layer, const Vec2f pos[2],
                                  const Vec2f uvs_px[2]) {
    const Vec2f uvs_scale = 1.0f / Vec2f{float(Ren::TextureAtlasWidth), float(Ren::TextureAtlasHeight)};
    Vec4f pos_uvs[2] = {Vec4f{pos[0][0], pos[0][1], uvs_px[0][0] * uvs_scale[0], uvs_px[0][1] * uvs_scale[1]},
                        Vec4f{pos[1][0], pos[1][1], uvs_px[1][0] * uvs_scale[0], uvs_px[1][1] * uvs_scale[1]}};

    vertex_t *vtx_data;
    int vtx_avail;
    uint16_t *ndx_data;
    int ndx_avail;
    int ndx_offset = AcquireVertexData(&vtx_data, &vtx_avail, &ndx_data, &ndx_avail);
    assert(vtx_avail >= 4 && ndx_avail >= 6);

    vertex_t *cur_vtx = vtx_data;
    uint16_t *cur_ndx = ndx_data;

    const uint16_t u16_tex_layer = f32_to_u16((1.0f / 16.0f) * float(tex_layer));

    static const uint16_t u16_draw_mode[] = {0, 32767, 65535};

    if (clip_area_stack_size_ && !ClipQuadToArea(pos_uvs, clip_area_stack_[clip_area_stack_size_ - 1])) {
        return;
    }

    cur_vtx->pos[0] = pos_uvs[0][0];
    cur_vtx->pos[1] = pos_uvs[0][1];
    cur_vtx->pos[2] = 0.0f;
    cur_vtx->col[0] = cur_vtx->col[1] = cur_vtx->col[2] = cur_vtx->col[3] = 255;
    cur_vtx->uvs[0] = f32_to_u16(pos_uvs[0][2]);
    cur_vtx->uvs[1] = f32_to_u16(pos_uvs[0][3]);
    cur_vtx->uvs[2] = u16_tex_layer;
    cur_vtx->uvs[3] = u16_draw_mode[int(draw_mode)];
    ++cur_vtx;

    cur_vtx->pos[0] = pos_uvs[1][0];
    cur_vtx->pos[1] = pos_uvs[0][1];
    cur_vtx->pos[2] = 0.0f;
    cur_vtx->col[0] = cur_vtx->col[1] = cur_vtx->col[2] = cur_vtx->col[3] = 255;
    cur_vtx->uvs[0] = f32_to_u16(pos_uvs[1][2]);
    cur_vtx->uvs[1] = f32_to_u16(pos_uvs[0][3]);
    cur_vtx->uvs[2] = u16_tex_layer;
    cur_vtx->uvs[3] = u16_draw_mode[int(draw_mode)];
    ++cur_vtx;

    cur_vtx->pos[0] = pos_uvs[1][0];
    cur_vtx->pos[1] = pos_uvs[1][1];
    cur_vtx->pos[2] = 0.0f;
    cur_vtx->col[0] = cur_vtx->col[1] = cur_vtx->col[2] = cur_vtx->col[3] = 255;
    cur_vtx->uvs[0] = f32_to_u16(pos_uvs[1][2]);
    cur_vtx->uvs[1] = f32_to_u16(pos_uvs[1][3]);
    cur_vtx->uvs[2] = u16_tex_layer;
    cur_vtx->uvs[3] = u16_draw_mode[int(draw_mode)];
    ++cur_vtx;

    cur_vtx->pos[0] = pos_uvs[0][0];
    cur_vtx->pos[1] = pos_uvs[1][1];
    cur_vtx->pos[2] = 0.0f;
    cur_vtx->col[0] = cur_vtx->col[1] = cur_vtx->col[2] = cur_vtx->col[3] = 255;
    cur_vtx->uvs[0] = f32_to_u16(pos_uvs[0][2]);
    cur_vtx->uvs[1] = f32_to_u16(pos_uvs[1][3]);
    cur_vtx->uvs[2] = u16_tex_layer;
    cur_vtx->uvs[3] = u16_draw_mode[int(draw_mode)];
    ++cur_vtx;

    (*cur_ndx++) = ndx_offset + 0;
    (*cur_ndx++) = ndx_offset + 1;
    (*cur_ndx++) = ndx_offset + 2;

    (*cur_ndx++) = ndx_offset + 0;
    (*cur_ndx++) = ndx_offset + 2;
    (*cur_ndx++) = ndx_offset + 3;

    SubmitVertexData(int(cur_vtx - vtx_data), int(cur_ndx - ndx_data));
}

void Gui::Renderer::PushLine(eDrawMode draw_mode, int tex_layer, const uint8_t color[4], const Vec4f &p0,
                             const Vec4f &p1, const Vec2f &d0, const Vec2f &d1, const Vec4f &thickness) {
    const Vec2f uvs_scale = 1.0f / Vec2f{(float)Ren::TextureAtlasWidth, (float)Ren::TextureAtlasHeight};

    const uint16_t u16_tex_layer = f32_to_u16((1.0f / 16.0f) * float(tex_layer));

    static const uint16_t u16_draw_mode[] = {0, 32767, 65535};

    const Vec4f perp[2] = {Vec4f{thickness} * Vec4f{-d0[1], d0[0], 1.0f, 0.0f},
                           Vec4f{thickness} * Vec4f{-d1[1], d1[0], 1.0f, 0.0f}};

    Vec4f pos_uvs[8] = {p0 - perp[0], p1 - perp[1], p1 + perp[1], p0 + perp[0]};
    int vertex_count = 4;

    for (int i = 0; i < vertex_count; i++) {
        pos_uvs[i][2] *= uvs_scale[0];
        pos_uvs[i][3] *= uvs_scale[1];
    }

    if (clip_area_stack_size_ &&
        !(vertex_count = ClipPolyToArea(pos_uvs, vertex_count, clip_area_stack_[clip_area_stack_size_ - 1]))) {
        return;
    }
    assert(vertex_count < 8);

    vertex_t *vtx_data;
    int vtx_avail;
    uint16_t *ndx_data;
    int ndx_avail;
    int ndx_offset = AcquireVertexData(&vtx_data, &vtx_avail, &ndx_data, &ndx_avail);
    assert(vtx_avail >= vertex_count && ndx_avail >= 3 * (vertex_count - 2));

    vertex_t *cur_vtx = vtx_data;
    uint16_t *cur_ndx = ndx_data;

    for (int i = 0; i < vertex_count; i++) {
        cur_vtx->pos[0] = pos_uvs[i][0];
        cur_vtx->pos[1] = pos_uvs[i][1];
        cur_vtx->pos[2] = 0.0f;
        memcpy(cur_vtx->col, color, 4);
        cur_vtx->uvs[0] = f32_to_u16(pos_uvs[i][2]);
        cur_vtx->uvs[1] = f32_to_u16(pos_uvs[i][3]);
        cur_vtx->uvs[2] = u16_tex_layer;
        cur_vtx->uvs[3] = u16_draw_mode[int(draw_mode)];
        ++cur_vtx;
    }

    for (int i = 0; i < vertex_count - 2; i++) {
        (*cur_ndx++) = ndx_offset + 0;
        (*cur_ndx++) = ndx_offset + i + 1;
        (*cur_ndx++) = ndx_offset + i + 2;
    }

    SubmitVertexData(int(cur_vtx - vtx_data), int(cur_ndx - ndx_data));
}

void Gui::Renderer::PushCurve(eDrawMode draw_mode, int tex_layer, const uint8_t color[4], const Vec4f &p0,
                              const Vec4f &p1, const Vec4f &p2, const Vec4f &p3, const Vec4f &thickness) {
    const float tolerance = 0.000001f;

    const Vec4f p01 = 0.5f * (p0 + p1), p12 = 0.5f * (p1 + p2), p23 = 0.5f * (p2 + p3), p012 = 0.5f * (p01 + p12),
                p123 = 0.5f * (p12 + p23), p0123 = 0.5f * (p012 + p123);

    const Vec2f d = Vec2f{p3} - Vec2f{p0};
    const float d2 = std::abs((p1[0] - p3[0]) * d[1] - (p1[1] - p3[1]) * d[0]),
                d3 = std::abs((p2[0] - p3[0]) * d[1] - (p2[1] - p3[1]) * d[0]);

    if ((d2 + d3) * (d2 + d3) < tolerance * (d[0] * d[0] + d[1] * d[1])) {
        PushLine(draw_mode, tex_layer, color, p0, p3, Normalize(Vec2f{p1 - p0}), Normalize(Vec2f{p3 - p2}), thickness);
    } else {
        PushCurve(draw_mode, tex_layer, color, p0, p01, p012, p0123, thickness);
        PushCurve(draw_mode, tex_layer, color, p0123, p123, p23, p3, thickness);
    }
}