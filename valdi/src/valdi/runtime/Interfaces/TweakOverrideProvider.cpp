//
//  TweakOverrideProvider.cpp
//  valdi
//

#include "valdi/runtime/Interfaces/TweakOverrideProvider.hpp"

#include "valdi_core/cpp/Utils/StringCache.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstdlib>

namespace Valdi {

namespace {

std::string_view trim(std::string_view value) {
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front()))) {
        value.remove_prefix(1);
    }
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back()))) {
        value.remove_suffix(1);
    }
    return value;
}

bool equalsIgnoreCase(std::string_view lhs, std::string_view rhs) {
    return lhs.size() == rhs.size() && std::equal(lhs.begin(), lhs.end(), rhs.begin(), [](char a, char b) {
               return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
           });
}

} // namespace

TweakOverrideProvider::TweakOverrideProvider(TweakOverrideMap overrides, Shared<ITweakValueProvider> base)
    : _overrides(std::move(overrides)), _base(std::move(base)) {}

TweakOverrideProvider::~TweakOverrideProvider() = default;

const std::string* TweakOverrideProvider::find(const StringBox& key) const {
    auto it = _overrides.find(key.toStringView());
    return it != _overrides.end() ? &it->second : nullptr;
}

StringBox TweakOverrideProvider::getString(const StringBox& key, const StringBox& fallback) {
    if (const auto* value = find(key)) {
        return StringCache::getGlobal().makeString(*value);
    }
    return _base != nullptr ? _base->getString(key, fallback) : fallback;
}

bool TweakOverrideProvider::getBool(const StringBox& key, bool fallback) {
    if (const auto* value = find(key)) {
        for (const auto* truthy : {"1", "true", "yes", "on"}) {
            if (equalsIgnoreCase(*value, truthy)) {
                return true;
            }
        }
        for (const auto* falsy : {"0", "false", "no", "off"}) {
            if (equalsIgnoreCase(*value, falsy)) {
                return false;
            }
        }
    }
    return _base != nullptr ? _base->getBool(key, fallback) : fallback;
}

float TweakOverrideProvider::getFloat(const StringBox& key, float fallback) {
    if (const auto* value = find(key)) {
        char* end = nullptr;
        const float parsed = std::strtof(value->c_str(), &end);
        if (end != value->c_str() && *end == '\0') {
            return parsed;
        }
    }
    return _base != nullptr ? _base->getFloat(key, fallback) : fallback;
}

int32_t TweakOverrideProvider::getInt(const StringBox& key, int32_t fallback) {
    if (const auto* value = find(key)) {
        int32_t parsed = 0;
        const auto* begin = value->data();
        const auto* end = begin + value->size();
        const auto result = std::from_chars(begin, end, parsed);
        if (result.ec == std::errc() && result.ptr == end) {
            return parsed;
        }
    }
    return _base != nullptr ? _base->getInt(key, fallback) : fallback;
}

Value TweakOverrideProvider::getBinary(const StringBox& key, const Value& fallback) {
    return _base != nullptr ? _base->getBinary(key, fallback) : fallback;
}

TweakOverrideMap TweakOverrideProvider::parse(std::string_view spec) {
    TweakOverrideMap overrides;
    while (!spec.empty()) {
        const auto comma = spec.find(',');
        const auto entry = spec.substr(0, comma);
        spec = comma == std::string_view::npos ? std::string_view() : spec.substr(comma + 1);

        const auto equals = entry.find('=');
        if (equals == std::string_view::npos) {
            continue;
        }
        const auto key = trim(entry.substr(0, equals));
        if (key.empty()) {
            continue;
        }
        overrides[std::string(key)] = std::string(trim(entry.substr(equals + 1)));
    }
    return overrides;
}

} // namespace Valdi
