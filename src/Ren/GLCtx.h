#pragma once

#include "SmallVector.h"
#include "Fence.h"

namespace Ren {
struct ApiContext {
    SmallVector<SyncFence, 4> in_flight_fences;
};

} // namespace Ren