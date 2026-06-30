//
//  LayerClass.hpp
//  valdi-desktop-apple
//
//  Created by Simon Corsin on 6/28/20.
//

#pragma once

#include "valdi_core/cpp/Utils/StringBox.hpp"
#include "valdi_core/cpp/Utils/StringCache.hpp"

#include "valdi/runtime/Attributes/AttributesBindingContext.hpp"
#include "valdi/runtime/Views/DefaultMeasureDelegate.hpp"

#include "snap_drawing/cpp/Layers/Layer.hpp"
#include "snap_drawing/cpp/Resources.hpp"

#include "valdi/snap_drawing/Utils/AttributesBinderMacros.hpp"

namespace snap::drawing {

class ILayerClass : public Valdi::DefaultMeasureDelegate {
public:
    ILayerClass(const Ref<Resources>& resources,
                const char* iosClassName,
                const char* androidClassName,
                const Ref<ILayerClass>& parentClass,
                bool canBeMeasured);
    ~ILayerClass() override;

    const char* getIOSClassName() const;
    const char* getAndroidClassName() const;
    virtual bool isFallback() const;
    virtual bool managesChildFrames() const;

    const Ref<ILayerClass>& getParent() const;

    virtual Valdi::Ref<Layer> instantiate() = 0;

    bool canBeMeasured() const;

    const Ref<Resources>& getResources() const;

    Valdi::Size onMeasure(const Valdi::Ref<Valdi::ValueMap>& attributes,
                          float width,
                          Valdi::MeasureMode widthMode,
                          float height,
                          Valdi::MeasureMode heightMode,
                          bool isRightToLeft) final;

    virtual Size onMeasure(const Valdi::Value& attributes, Size maxSize, bool isRightToLeft);

    virtual void bindAttributes(Valdi::AttributesBindingContext& binder);

private:
    Ref<Resources> _resources;
    const char* _iosClassName;
    const char* _androidClassName;
    Ref<ILayerClass> _parentClass;
    bool _canBeMeasured;
};

} // namespace snap::drawing
