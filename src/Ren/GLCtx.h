#pragma once

#include "SmallVector.h"
#include "Fence.h"

namespace Ren {
struct ApiContext {
    SmallVector<SyncFence, 4> in_flight_fences;

    int active_present_image = 0;

    int backend_frame = 0;
    Tex2DRef present_image_refs[MaxFramesInFlight];
};

} // namespace Ren