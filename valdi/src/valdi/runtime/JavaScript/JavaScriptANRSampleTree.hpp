#pragma once

#include "valdi_core/cpp/Utils/StringBox.hpp"

#include <chrono>
#include <cstddef>
#include <string>
#include <vector>

namespace Valdi {

/** One distinct breadcrumb stack seen while sampling an ANR window, with the sampled time the JS thread spent in it. */
struct JavaScriptANRSampledStack {
    /** Labels from the outermost frame to the innermost. Empty when the JS thread was in no breadcrumb scope. */
    std::vector<StringBox> labels;
    std::chrono::steady_clock::duration time{};
};

/** A frame of the sample tree, holding the time sampled inside it. */
struct JavaScriptANRSampleTreeNode {
    /**
     * Labels of the frame and any frames it alone leads into for at least 90% of its time, printed "a -> b". Empty
     * for work outside any breadcrumb scope.
     */
    std::vector<StringBox> labels;
    std::chrono::steady_clock::duration time{};
    /** Branches under this frame, busiest first. */
    std::vector<JavaScriptANRSampleTreeNode> children;
};

/**
 * Where the JS thread spent the sampled part of an ANR window: the sampled stacks merged by shared prefix, keeping
 * only branches that held at least a fifth of the sampled time. Busiest branches come first, so the first branch at
 * each level, followed while it holds at least half of the time, is the dominant stack (see JavaScriptTaskScheduler
 * for dominant and deadline stacks).
 */
struct JavaScriptANRSampleTree {
    std::chrono::steady_clock::duration sampledTime{};
    std::vector<JavaScriptANRSampleTreeNode> branches;
};

constexpr size_t kMaxSampleTreeLines = 8;

JavaScriptANRSampleTree buildSampleTree(const std::vector<JavaScriptANRSampledStack>& stacks);

/**
 * The tree as lines appended to an ANR message, so its first line stays unchanged:
 *
 *     Sampled 251 ms of the 300 ms ANR window
 *       251 ms  request.<p1>
 *       141 ms    runtime.trace -> renderComponent.ProfileCore
 *       100 ms      runtime.loadJsModule
 *        80 ms    runtime.loadJsModule
 *
 * `anrWindow` is the detection threshold the report names; sampling starts at the detector's first tick, so it covers
 * less. At most kMaxSampleTreeLines frames are printed. The header has no colon because some report parsers take the
 * text before a message's first colon as its type.
 */
std::string formatSampleTree(const JavaScriptANRSampleTree& tree, std::chrono::steady_clock::duration anrWindow);

} // namespace Valdi
