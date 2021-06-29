#pragma once

#include "SmallVector.h"
#include "Fence.h"

namespace Ren {
struct GLContext {
    SmallVector<SyncFence, 4> in_flight_fences;
};

} // namespace Ren