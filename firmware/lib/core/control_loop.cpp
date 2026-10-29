#include "control_loop.h"

namespace core {

void ControlLoop::tick(uint32_t nowMs) {
    ++ticks_;
    lastTickMs_ = nowMs;
}

}  // namespace core
