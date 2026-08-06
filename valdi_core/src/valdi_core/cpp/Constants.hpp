//
//  Constants.hpp
//  ValdiRuntime
//
//  Created by Simon Corsin on 5/28/18.
//  Copyright © 2018 Snap Inc. All rights reserved.
//

#pragma once

#include <chrono>
#include <cstdint>

namespace Valdi {

/**
 Maximum time the main thread may block on a synchronous JS call made while handling user
 input: hit test, gesture predicates, gesture actions, and scroll drag end. Past this the
 call is abandoned. It still runs to completion on the JS thread, but its result is dropped
 so the main thread can keep servicing input. Shared by every input deadline so that tuning
 one tunes all of them. Android's PredicateUtils.GESTURE_PREDICATE_TIMEOUT_MS mirrors this.
 */
constexpr std::chrono::milliseconds kInputSyncCallDeadline = std::chrono::milliseconds(250);

extern int64_t valdiVersion;
extern bool traceRenderingPerformance;
extern bool traceReloaderPerformance;
extern bool traceLoadModules;
extern bool traceInitialization;
extern bool forceRetainJsObjects;

#if defined(__GNUC__) || defined(__clang__)
#define VALDI_LIKELY(x) __builtin_expect(!!(x), true)
#define VALDI_UNLIKELY(x) __builtin_expect(!!(x), false)
#else
#define VALDI_LIKELY(x) (x)
#define VALDI_UNLIKELY(x) (x)
#endif

} // namespace Valdi
