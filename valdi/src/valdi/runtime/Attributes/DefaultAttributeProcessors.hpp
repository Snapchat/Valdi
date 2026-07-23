//
//  DefaultAttributeProcessors.hpp
//  ValdiRuntime
//
//  Created by Simon Corsin on 6/27/18.
//  Copyright © 2018 Snap Inc. All rights reserved.
//

#pragma once

#include "valdi/runtime/Attributes/AttributesManager.hpp"

namespace Valdi {

class ColorPalette;

Result<Value> preprocessBorder(const Value& in);
Result<Value> preprocessBoxShadow(const Value& in);
Result<Value> preprocessTextShadow(const Value& in);
Result<Value> preprocessBorderRadius(const Value& in);
Result<Value> preprocessGradient(const Value& in);

Result<Value> postprocessBorder(ViewNode& viewNode, const Value& in);
Result<Value> postprocessBoxShadow(ViewNode& viewNode, const Value& in);
Result<Value> postprocessTextShadow(ViewNode& viewNode, const Value& in);
Result<Value> postprocessGradient(ViewNode& viewNode, const Value& in);
Result<Value> postprocessBorderRadius(ViewNode& viewNode, const Value& in);
Result<Value> postprocessBoxShadow(bool isRightToLeft, const Value& in);
Result<Value> postprocessBorderRadius(bool isRightToLeft, const Value& in);
Result<Value> postprocessGradient(bool isRightToLeft, const Value& in);
Result<Value> postprocessGradient(bool isRightToLeft, const ColorPalette& colorPalette, const Value& in);

void registerDefaultProcessors(AttributesManager& attributesManager);

} // namespace Valdi
