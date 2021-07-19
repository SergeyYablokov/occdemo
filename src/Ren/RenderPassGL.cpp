#include "RenderPass.h"

void Ren::RenderPass::Destroy() {}

bool Ren::RenderPass::Setup(ApiContext *api_ctx, const RenderTarget color_rts[], int color_rts_count,
                            RenderTarget depth_rt, ILog *log) {
    return true;
}