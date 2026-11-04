#pragma once

#include "valdi_core/cpp/Utils/ReferenceInfo.hpp"
#include "valdi_core/cpp/Utils/StringBox.hpp"

#include <array>
#include <cstddef>
#include <string>

namespace Valdi {

/**
 * Bounded attribution snapshot. Keeps the origin and most recent scopes when full.
 * Consecutive duplicate labels share a frame.
 * Snapshots own their frames without retaining a chain of parent snapshots.
 */
class JavaScriptANRAttribution {
public:
    static constexpr size_t kMaxFrames = 16;
    static constexpr size_t kMaxLabelBytes = 128;

    /** Restore tokens belong to one push and must be popped in reverse order. */
    struct RestoreToken {
        StringBox displacedFrame;
        bool pushed = false;
    };

    /** Limits a stored label to kMaxLabelBytes without splitting UTF-8 characters. */
    static StringBox boundedLabel(const StringBox& label);

    /**
     * Bounded label for a bridge reference (e.g. "<anon>.friendmojiProvider.forUsers.<p1>"). Empty when no
     * component has an assigned name: "<anon>.<p0>" would put unrelated work under one label.
     */
    static StringBox referenceLabel(const ReferenceInfo& referenceInfo);

    RestoreToken push(const StringBox& label);
    void pop(RestoreToken token);

    bool empty() const;
    std::string format() const;

private:
    std::array<StringBox, kMaxFrames> _frames;
    size_t _size = 0;
    size_t _omittedFrames = 0;
};

} // namespace Valdi
