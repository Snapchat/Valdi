//
//  ValueConverters.hpp
//  Valdi
//
//  Created by Simon Corsin on 7/8/18.
//  Copyright © 2018 Snap Inc. All rights reserved.
//

#pragma once

#include "valdi/runtime/Attributes/BorderRadius.hpp"
#include "valdi_core/cpp/Attributes/ColorPalette.hpp"
#include "valdi_core/cpp/Utils/Result.hpp"
#include "valdi_core/cpp/Utils/Value.hpp"
#include <vector>

namespace Valdi {

class ColorPalette;
class AttributeParser;

struct ValueConverter {
    [[nodiscard]] static Result<StringBox> toString(const Value& value);

    [[nodiscard]] static Result<int32_t> toInt(const Value& value);

    [[nodiscard]] static Result<Value> toColor(const ColorPalette& colorPalette, const Value& value);

    [[nodiscard]] static Result<Value> toColorValue(const Value& value);

    [[nodiscard]] static Result<double> toDouble(const Value& value);

    [[nodiscard]] static Result<bool> toBool(const Value& value);

    [[nodiscard]] static Result<PercentValue> toPercent(const Value& value);
    static std::optional<PercentValue> parsePercent(AttributeParser& parser);

    [[nodiscard]] static Result<Ref<BorderRadius>> toBorderValues(const Value& value);

    static Error invalidTypeFailure(const Value& value, ValueType expectedType);
};

} // namespace Valdi
