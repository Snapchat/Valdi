//
//  LayerClass.cpp
//  valdi-desktop-apple
//
//  Created by Simon Corsin on 6/28/20.
//

#include "valdi/snap_drawing/Layers/Interfaces/ILayerClass.hpp"

namespace snap::drawing {

ILayerClass::ILayerClass(const Ref<Resources>& resources,
                         const char* iosClassName,
                         const char* androidClassName,
                         const Ref<ILayerClass>& parentClass,
                         bool canBeMeasured)
    : _resources(resources),
      _iosClassName(iosClassName),
      _androidClassName(androidClassName),
      _parentClass(parentClass),
      _canBeMeasured(canBeMeasured) {}

ILayerClass::~ILayerClass() = default;

const char* ILayerClass::getIOSClassName() const {
    return _iosClassName;
}

const char* ILayerClass::getAndroidClassName() const {
    return _androidClassName;
}

bool ILayerClass::isFallback() const {
    return false;
}

const Ref<ILayerClass>& ILayerClass::getParent() const {
    return _parentClass;
}

bool ILayerClass::canBeMeasured() const {
    return _canBeMeasured;
}

Valdi::Size ILayerClass::onMeasure(const Valdi::Ref<Valdi::ValueMap>& layoutAttributes,
                                   float width,
                                   Valdi::MeasureMode widthMode,
                                   float height,
                                   Valdi::MeasureMode heightMode,
                                   bool isRightToLeft) {
    if (!_canBeMeasured) {
        return Valdi::Size();
    }

    auto constrainedWidth =
        (widthMode == Valdi::MeasureModeUnspecified) ? std::numeric_limits<Scalar>::max() : static_cast<Scalar>(width);
    auto constrainedHeight = (heightMode == Valdi::MeasureModeUnspecified) ? std::numeric_limits<Scalar>::max() :
                                                                             static_cast<Scalar>(height);

    auto measuredSize =
        onMeasure(Valdi::Value(layoutAttributes), Size::make(constrainedWidth, constrainedHeight), isRightToLeft);

    return Valdi::Size(static_cast<float>(measuredSize.width), static_cast<float>(measuredSize.height));
}

Size ILayerClass::onMeasure(const Valdi::Value& attributes, Size maxSize, bool isRightToLeft) {
    return Size::makeEmpty();
}

void ILayerClass::bindAttributes(Valdi::AttributesBindingContext& binder) {}

const Ref<Resources>& ILayerClass::getResources() const {
    return _resources;
}

} // namespace snap::drawing
