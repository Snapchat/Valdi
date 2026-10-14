#include "utils/debugging/Assert.hpp"

#include <atomic>
#include <chrono>
#include <cstdio>

namespace snap {
namespace {
// Global variable that we set by setEnableReleaseAsserts(). Enabled by default for development builds.
std::atomic_bool gReleaseAssertsEnabled{kAssertsCompiledIn};
// Global variable indicating whether fused assertions are enabled. Disabled by
// default. Should be enabled by production host apps.
std::atomic_bool gFusedAssertionEnabled{false};
} // namespace

bool releaseAssertsEnabled() {
    return gReleaseAssertsEnabled.load(std::memory_order_relaxed);
}

void setEnableReleaseAsserts(bool enable) {
    gReleaseAssertsEnabled.store(enable, std::memory_order_relaxed);
}

bool fusedAssertsEnabled() {
    return gFusedAssertionEnabled.load(std::memory_order_relaxed);
}

void setEnableFusedAsserts(bool enable) {
    gFusedAssertionEnabled.store(enable, std::memory_order_relaxed);
}

} // namespace snap
