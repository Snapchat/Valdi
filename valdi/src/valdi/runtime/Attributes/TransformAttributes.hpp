//
//  TransformAttributes.hpp
//  valdi
//

#pragma once

#include "valdi_core/cpp/Utils/Result.hpp"
#include "valdi_core/cpp/Utils/Value.hpp"

namespace Valdi {

class ViewNode;

class TransformAttributes {
public:
    static Result<Value> postprocess(float width, float height, bool isRightToLeft, const Value& value);
    static Result<Value> postprocessViewNode(ViewNode& viewNode, const Value& value);
};

} // namespace Valdi
