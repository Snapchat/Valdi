//
//  DefaultMeasureDelegate.hpp
//  valdi
//
//  Created by Simon Corsin on 6/10/22.
//

#pragma once

#include "valdi/runtime/Views/MeasureDelegate.hpp"

namespace Valdi {

class DefaultMeasureDelegate : public MeasureDelegate {
public:
    DefaultMeasureDelegate();
    ~DefaultMeasureDelegate() override;

    Size measure(ViewNode& viewNode, float width, MeasureMode widthMode, float height, MeasureMode heightMode) final;

    virtual Valdi::Size onMeasure(const Valdi::Ref<Valdi::ValueMap>& attributes,
                                  float width,
                                  Valdi::MeasureMode widthMode,
                                  float height,
                                  Valdi::MeasureMode heightMode,
                                  bool isRightToLeft) = 0;
};

} // namespace Valdi
