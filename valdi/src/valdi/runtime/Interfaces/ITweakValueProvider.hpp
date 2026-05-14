//
//  TweakValueProvider.hpp
//  valdi
//
//  Created by Simon Corsin on 3/1/22.
//

#pragma once

#include "valdi_core/cpp/Utils/StringBox.hpp"
#include "valdi_core/cpp/Utils/Value.hpp"

namespace Valdi {

class ITweakValueProvider {
public:
    virtual ~ITweakValueProvider() = default;
    virtual StringBox getString(const StringBox& key, const StringBox& fallback) = 0;
    virtual bool getBool(const StringBox& key, bool fallback) = 0;
    virtual float getFloat(const StringBox& key, float fallback) = 0;
    virtual int32_t getInt(const StringBox& key, int32_t fallback) = 0;
    virtual Value getBinary(const StringBox& key, const Value& fallback) = 0;
};

} // namespace Valdi
