#include "RpSampleBrightness.h"

#include <Ren/Context.h>

#include "../../Utils/ShaderLoader.h"

void RpSampleBrightness::Setup(RpBuilder &builder, Ren::WeakTex2DRef tex_to_sample, const char reduced_tex[]) {
    tex_to_sample_ = tex_to_sample;

    { // aux buffer which gathers frame luminance
        Ren::Tex2DParams params;
        params.w = 16;
        params.h = 8;
        params.format = Ren::eTexFormat::RawR16F;
        params.sampling.filter = Ren::eTexFilter::BilinearNoMipmap;
        params.sampling.repeat = Ren::eTexRepeat::ClampToEdge;

        reduced_tex_ = builder.WriteTexture(reduced_tex, params, *this);
    }
}

void RpSampleBrightness::LazyInit(Ren::Context &ctx, ShaderLoader &sh, RpAllocTex &reduced_tex) {
    if (!initialized_) {
        blit_red_prog_ = sh.LoadProgram(ctx, "blit_red", "internal/blit.vert.glsl", "internal/blit_reduced.frag.glsl");
        assert(blit_red_prog_->ready());

        readback_buf_ = ctx.CreateBuffer("Brightness Readback", Ren::eBufType::Stage,
                                         GLsizeiptr(4) * res_[0] * res_[1] * sizeof(float) * Ren::MaxFramesInFlight);

        initialized_ = true;
    }

    if (!reduced_fb_.Setup(ctx.api_ctx(), nullptr, reduced_tex.desc.w, reduced_tex.desc.h, reduced_tex.ref, {}, {},
                           false)) {
        ctx.log()->Error("RpSampleBrightness: reduced_fb_ init failed!");
    }
}