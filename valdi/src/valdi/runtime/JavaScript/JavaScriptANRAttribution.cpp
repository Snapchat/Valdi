#include "valdi/runtime/JavaScript/JavaScriptANRAttribution.hpp"

#include <algorithm>
#include <utility>

namespace Valdi {

static std::string formatFrame(const JavaScriptANRAttribution::Frame& frame,
                               const std::optional<JavaScriptANRAttribution::TimePoint>& now) {
    auto text = frame.label.slowToString();
    if (now.has_value() && frame.startTime != JavaScriptANRAttribution::TimePoint()) {
        auto openMs = std::chrono::duration_cast<std::chrono::milliseconds>(now.value() - frame.startTime).count();
        // Whole milliseconds keep the text sliceable with LIKE patterns.
        text += " (" + std::to_string(openMs) + " ms)";
    }
    return text;
}

StringBox JavaScriptANRAttribution::boundedLabel(const StringBox& label) {
    auto text = label.toStringView();
    if (text.size() <= JavaScriptANRAttribution::kMaxLabelBytes) {
        return label;
    }

    auto length = JavaScriptANRAttribution::kMaxLabelBytes - 3;
    // Keep a complete UTF-8 prefix before the truncation marker.
    while (length > 0 && (static_cast<unsigned char>(text[length]) & 0xc0) == 0x80) {
        --length;
    }
    return StringBox::fromString(std::string(text.substr(0, length)) + "...");
}

StringBox JavaScriptANRAttribution::referenceLabel(const ReferenceInfo& referenceInfo) {
    auto hasName = std::any_of(referenceInfo.begin(), referenceInfo.end(), [](const auto& component) {
        return component.type == ReferenceInfo::Component::Type::Property && component.hasAssignedName;
    });
    return hasName ? boundedLabel(referenceInfo.toFunctionIdentifier()) : StringBox();
}

JavaScriptANRAttribution::RestoreToken JavaScriptANRAttribution::push(const StringBox& label, TimePoint startTime) {
    if (label.isEmpty()) {
        return {};
    }

    auto frameLabel = boundedLabel(label);
    if (_size > 0 && _frames[_size - 1].label == frameLabel) {
        return {};
    }

    Frame frame{std::move(frameLabel), startTime};
    RestoreToken token;
    token.pushed = true;
    if (_size < kMaxFrames) {
        _frames[_size++] = std::move(frame);
    } else {
        token.displacedFrame = std::move(_frames[1]);
        for (size_t i = 1; i + 1 < kMaxFrames; ++i) {
            _frames[i] = std::move(_frames[i + 1]);
        }
        _frames.back() = std::move(frame);
        ++_omittedFrames;
    }
    return token;
}

void JavaScriptANRAttribution::pop(RestoreToken token) {
    if (!token.pushed) {
        return;
    }

    if (!token.displacedFrame.label.isEmpty()) {
        for (size_t i = kMaxFrames - 1; i > 1; --i) {
            _frames[i] = std::move(_frames[i - 1]);
        }
        _frames[1] = std::move(token.displacedFrame);
        --_omittedFrames;
    } else {
        _frames[--_size] = Frame();
    }
}

void JavaScriptANRAttribution::setStartTime(TimePoint startTime) {
    for (size_t i = 0; i < _size; ++i) {
        _frames[i].startTime = startTime;
    }
}

bool JavaScriptANRAttribution::empty() const {
    return _size == 0;
}

size_t JavaScriptANRAttribution::size() const {
    return _size;
}

const StringBox& JavaScriptANRAttribution::getLabel(size_t index) const {
    return _frames[index].label;
}

StringBox JavaScriptANRAttribution::getInnermostLabel() const {
    return _size == 0 ? StringBox() : _frames[_size - 1].label;
}

std::string JavaScriptANRAttribution::format(std::optional<TimePoint> now) const {
    if (empty()) {
        return {};
    }

    std::string result = " [stuck-in: " + _frames[_size - 1].label.slowToString() + "]";
    // A lone frame only repeats the stuck-in label, unless it carries a duration.
    if (_size > 1 || now.has_value()) {
        result += " [attribution: " + formatFrame(_frames[0], now);
        if (_omittedFrames > 0) {
            result += " -> ... (" + std::to_string(_omittedFrames) + " frames omitted)";
        }
        for (size_t i = 1; i < _size; ++i) {
            result += " -> " + formatFrame(_frames[i], now);
        }
        result += "]";
    }
    return result;
}

} // namespace Valdi
