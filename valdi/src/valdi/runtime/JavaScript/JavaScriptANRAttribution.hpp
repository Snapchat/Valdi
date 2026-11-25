#pragma once

#include "valdi_core/cpp/Utils/ReferenceInfo.hpp"
#include "valdi_core/cpp/Utils/StringBox.hpp"

#include <array>
#include <chrono>
#include <cstddef>
#include <optional>
#include <string>

namespace Valdi {

/**
 * Bounded attribution snapshot. Keeps the origin and most recent scopes when full.
 * Consecutive duplicate labels share a frame.
 * Snapshots own their frames without retaining a chain of parent snapshots.
 */
class JavaScriptANRAttribution {
public:
    using TimePoint = std::chrono::steady_clock::time_point;

    static constexpr size_t kMaxFrames = 16;
    static constexpr size_t kMaxLabelBytes = 128;

    struct Frame {
        StringBox label;
        /** When the frame became current on the JS thread. Default-constructed when not recorded. */
        TimePoint startTime;
    };

    /** Restore tokens belong to one push and must be popped in reverse order. */
    struct RestoreToken {
        Frame displacedFrame;
        bool pushed = false;
    };

    /** Limits a stored label to kMaxLabelBytes without splitting UTF-8 characters. */
    static StringBox boundedLabel(const StringBox& label);

    /**
     * Bounded label for a bridge reference (e.g. "<anon>.friendmojiProvider.forUsers.<p1>"). Empty when no
     * component has an assigned name: "<anon>.<p0>" would put unrelated work under one label.
     */
    static StringBox referenceLabel(const ReferenceInfo& referenceInfo);

    RestoreToken push(const StringBox& label, TimePoint startTime = TimePoint());
    void pop(RestoreToken token);

    /** Records every frame as starting at `startTime`, e.g. when inherited frames enter a new task. */
    void setStartTime(TimePoint startTime);

    bool empty() const;
    size_t size() const;
    const StringBox& getLabel(size_t index) const;
    /** The innermost frame's label, or empty when there are no frames. */
    StringBox getInnermostLabel() const;

    /** When `now` is set, frames with a recorded start time report how long they have been open. */
    std::string format(std::optional<TimePoint> now = std::nullopt) const;

private:
    std::array<Frame, kMaxFrames> _frames;
    size_t _size = 0;
    size_t _omittedFrames = 0;
};

} // namespace Valdi
