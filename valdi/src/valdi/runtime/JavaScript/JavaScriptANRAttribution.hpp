#pragma once

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
