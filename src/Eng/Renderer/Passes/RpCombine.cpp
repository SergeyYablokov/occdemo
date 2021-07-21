#include "RpCombine.h"

#include <cassert>

#include <Ren/Context.h>
#include <Ren/Program.h>
#include <Ren/RastState.h>

#include "../../Utils/ShaderLoader.h"
#include "../PrimDraw.h"
#include "../Renderer_Structs.h"

#include "../assets/shaders/internal/blit_combine_interface.glsl"

void RpCombine::Setup(RpBuilder &builder, const ViewState *view_state, const float gamma, const float exposure,
                      const float fade, bool const tonemap, const char color_tex_name[], const char blur_tex_name[],
                      const char output_tex_name[]) {
    view_state_ = view_state;
    gamma_ = gamma;
    exposure_ = exposure;
    fade_ = fade;
    tonemap_ = tonemap;

    color_tex_ = builder.ReadTexture(color_tex_name, *this);

    if (blur_tex_name) {
        blur_tex_ = builder.ReadTexture(blur_tex_name, *this);
    } else {
        blur_tex_ = {};
    }

    if (output_tex_name) {
        Ren::Tex2DParams params;
        params.w = view_state->scr_res[0];
        params.h = view_state->scr_res[1];
        params.format = Ren::eTexFormat::RawRGB888;
        params.sampling.filter = Ren::eTexFilter::BilinearNoMipmap;
        params.sampling.repeat = Ren::eTexRepeat::ClampToEdge;

        output_tex_ = builder.WriteTexture(output_tex_name, params, *this);
    } else {
        output_tex_ = {};
    }
}

void RpCombine::Execute(RpBuilder &builder) {
    RpAllocTex &color_tex = builder.GetReadTexture(color_tex_);
    RpAllocTex *blur_tex = nullptr;
    if (blur_tex_) {
        blur_tex = &builder.GetReadTexture(blur_tex_);
    }
    RpAllocTex *output_tex = nullptr;
    if (output_tex_) {
        output_tex = &builder.GetWriteTexture(output_tex_);
    }

    LazyInit(builder.ctx(), builder.sh(), output_tex);

    Ren::RastState rast_state;
    rast_state.cull_face.enabled = true;

    if (output_tex_) {
        rast_state.viewport[2] = view_state_->act_res[0];
        rast_state.viewport[3] = view_state_->act_res[1];
    } else {
        rast_state.viewport[2] = view_state_->scr_res[0];
        rast_state.viewport[3] = view_state_->scr_res[1];
    }

    rast_state.Apply();
    Ren::RastState applied_state = rast_state;

    BlitCombine::Params uniform_params;
    uniform_params.transform = Ren::Vec4f{0.0f, 0.0f, float(view_state_->act_res[0]), float(view_state_->act_res[1])};
    uniform_params.tex_size = Ren::Vec2f{float(view_state_->scr_res[0]), float(view_state_->scr_res[1])};
    uniform_params.tonemap = tonemap_ ? 1.0f : 0.0f;
    uniform_params.gamma = gamma_;
    uniform_params.exposure = tonemap_ ? exposure_ : 1.0f;
    uniform_params.fade = fade_;

    const PrimDraw::Binding bindings[] = {
        {Ren::eBindTarget::Tex2D, HDR_TEX_SLOT, *color_tex.ref},
        {Ren::eBindTarget::Tex2D, BLURED_TEX_SLOT, blur_tex ? *blur_tex->ref : *dummy_black_}};

    prim_draw_.DrawPrim(PrimDraw::ePrim::Quad, blit_combine_prog_.get(), output_fb_[builder.ctx().backend_frame()],
                        render_pass_[builder.ctx().backend_frame()], bindings, 2, &uniform_params,
                        sizeof(BlitCombine::Params), 0);
}

void RpCombine::LazyInit(Ren::Context &ctx, ShaderLoader &sh, RpAllocTex *output_tex) {
    if (!initialized) {
        blit_combine_prog_ =
            sh.LoadProgram(ctx, "blit_combine2", "internal/blit_combine.vert.glsl", "internal/blit_combine.frag.glsl");
        assert(blit_combine_prog_->ready());

        static const uint8_t black[] = {0, 0, 0, 0};

        Ren::Tex2DParams p;
        p.w = p.h = 1;
        p.format = Ren::eTexFormat::RawRGBA8888;
        p.sampling.repeat = Ren::eTexRepeat::ClampToEdge;

        Ren::eTexLoadStatus status;
        dummy_black_ = ctx.LoadTexture2D("dummy_black", black, sizeof(black), p, ctx.default_stage_bufs(),
                                         ctx.default_mem_allocs(), &status);
        assert(status == Ren::eTexLoadStatus::CreatedFromData || status == Ren::eTexLoadStatus::Found);

        initialized = true;
    }

    const Ren::WeakTex2DRef output = output_tex ? output_tex->ref : ctx.backbuffer_ref();
    const Ren::RenderTarget render_target = {output, Ren::eLoadOp::DontCare, Ren::eStoreOp::Store};
    const Ren::RenderTarget render_targets[] = {{output, Ren::eLoadOp::DontCare, Ren::eStoreOp::Store}};

    if (!render_pass_[ctx.backend_frame()].Setup(ctx.api_ctx(), render_targets, 1, {}, ctx.log())) {
        ctx.log()->Error("RpCombine: render_pass_ init failed!");
    }

    if (!output_fb_[ctx.backend_frame()].Setup(ctx.api_ctx(), render_pass_[ctx.backend_frame()].handle(),
                                               output_tex ? output_tex->desc.w : ctx.w(),
                                               output_tex ? output_tex->desc.h : ctx.h(), output, {}, {}, false)) {
        ctx.log()->Error("RpCombine: output_fb_ init failed!");
    }
}
