//
//  TweakOverrideProvider.hpp
//  valdi
//

#pragma once

#include "valdi/runtime/Interfaces/ITweakValueProvider.hpp"
#include "valdi_core/cpp/Utils/Shared.hpp"

#include <functional>
#include <map>
#include <string>
#include <string_view>

namespace Valdi {

using TweakOverrideMap = std::map<std::string, std::string, std::less<>>;

/**
 * Answers a fixed set of string overrides and defers every other key to a base provider, or to the
 * caller's fallback when there is none. Values are parsed per getter: `1/true/yes/on` and
 * `0/false/no/off` for bools, decimal for ints and floats. A value that doesn't parse as the
 * requested type is ignored, so the base provider (or the fallback) answers instead.
 *
 * Immutable after construction, so safe to read from any thread.
 */
class TweakOverrideProvider : public SharedPtrRefCountable, public ITweakValueProvider {
public:
    TweakOverrideProvider(TweakOverrideMap overrides, Shared<ITweakValueProvider> base);
    ~TweakOverrideProvider() override;

    StringBox getString(const StringBox& key, const StringBox& fallback) override;
    bool getBool(const StringBox& key, bool fallback) override;
    float getFloat(const StringBox& key, float fallback) override;
    int32_t getInt(const StringBox& key, int32_t fallback) override;
    Value getBinary(const StringBox& key, const Value& fallback) override;

    /**
     * Parses `KEY=VALUE,KEY2=VALUE2`. Whitespace around keys and values is trimmed. Entries without
     * an `=` or with an empty key are skipped. A later entry replaces an earlier one with the same key.
     */
    static TweakOverrideMap parse(std::string_view spec);

private:
    const std::string* find(const StringBox& key) const;

    TweakOverrideMap _overrides;
    Shared<ITweakValueProvider> _base;
};

} // namespace Valdi
