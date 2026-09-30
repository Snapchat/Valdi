#include "valdi/runtime/JavaScript/JavaScriptANRAttribution.hpp"

#include <utility>

namespace Valdi {

static StringBox boundedLabel(const StringBox& label) {
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

JavaScriptANRAttribution::RestoreToken JavaScriptANRAttribution::push(const StringBox& label) {
    if (label.isEmpty()) {
        return {};
    }

    auto frame = boundedLabel(label);
    if (_size > 0 && _frames[_size - 1] == frame) {
        return {};
    }

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

    if (!token.displacedFrame.isEmpty()) {
        for (size_t i = kMaxFrames - 1; i > 1; --i) {
            _frames[i] = std::move(_frames[i - 1]);
        }
        _frames[1] = std::move(token.displacedFrame);
        --_omittedFrames;
    } else {
        _frames[--_size] = StringBox();
    }
}

bool JavaScriptANRAttribution::empty() const {
    return _size == 0;
}

std::string JavaScriptANRAttribution::format() const {
    if (empty()) {
        return {};
    }

    std::string result = " [stuck-in: " + _frames[_size - 1].slowToString() + "]";
    if (_size > 1) {
        result += " [attribution: " + _frames[0].slowToString();
        if (_omittedFrames > 0) {
            result += " -> ... (" + std::to_string(_omittedFrames) + " frames omitted)";
        }
        for (size_t i = 1; i < _size; ++i) {
            result += " -> " + _frames[i].slowToString();
        }
        result += "]";
    }
    return result;
}

} // namespace Valdi
