#pragma once

#include "SmallVector.h"
#include "Fence.h"

namespace Ren {
struct ApiContext {
    SmallVector<SyncFence, 4> in_flight_fences;

    int backend_frame = 0;
};

} // namespace Ren