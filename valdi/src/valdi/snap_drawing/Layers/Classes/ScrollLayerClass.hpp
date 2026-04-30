//
//  ScrollLayerClass.hpp
//  valdi-desktop-apple
//
//  Created by Simon Corsin on 1/12/22.
//

#pragma once

#include "snap_drawing/cpp/Layers/ScrollLayer.hpp"
#include "valdi/snap_drawing/Layers/Classes/LayerClass.hpp"
#include "valdi/snap_drawing/Layers/Interfaces/ILayerClass.hpp"

namespace snap::drawing {

class ScrollLayerClass : public ILayerClass {
public:
    ScrollLayerClass(const Ref<Resources>& resources, bool useAndroidStyleScroller, const Ref<LayerClass>& parentClass);

    Valdi::Ref<Layer> instantiate() override;
    void bindAttributes(Valdi::AttributesBindingContext& binder) override;

    // NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
    Valdi::Result<Valdi::Void> apply_showsVerticalScrollIndicator(ScrollLayer& view,
                                                                  bool value,
                                                                  const AttributeContext& context);
    // NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
    void reset_showsVerticalScrollIndicator(ScrollLayer& view, const AttributeContext& context);

    // NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
    Valdi::Result<Valdi::Void> apply_showsHorizontalScrollIndicator(ScrollLayer& view,
                                                                    bool value,
                                                                    const AttributeContext& context);
    // NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
    void reset_showsHorizontalScrollIndicator(ScrollLayer& view, const AttributeContext& context);

    // NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
    Valdi::Result<Valdi::Void> apply_bouncesVerticalWithSmallContent(ScrollLayer& view,
                                                                     bool value,
                                                                     const AttributeContext& context);
    // NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
    void reset_bouncesVerticalWithSmallContent(ScrollLayer& view, const AttributeContext& context);

    // NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
    Valdi::Result<Valdi::Void> apply_bouncesHorizontalWithSmallContent(ScrollLayer& view,
                                                                       bool value,
                                                                       const AttributeContext& context);
    // NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
    void reset_bouncesHorizontalWithSmallContent(ScrollLayer& view, const AttributeContext& context);

    // NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
    Valdi::Result<Valdi::Void> apply_bounces(ScrollLayer& view, bool value, const AttributeContext& context);
    // NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
    void reset_bounces(ScrollLayer& view, const AttributeContext& context);

    // NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
    Valdi::Result<Valdi::Void> apply_pagingEnabled(ScrollLayer& view, bool value, const AttributeContext& context);
    // NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
    void reset_pagingEnabled(ScrollLayer& view, const AttributeContext& context);

    DECLARE_UNTYPED_ATTRIBUTE(ScrollLayer, scrollPerfLoggerBridge)

    DECLARE_BOOLEAN_ATTRIBUTE(ScrollLayer, cancelsTouchesOnScroll)

    DECLARE_BOOLEAN_ATTRIBUTE(ScrollLayer, dismissKeyboardOnDrag)

    DECLARE_DOUBLE_ATTRIBUTE(ScrollLayer, fadingEdgeLength)

    DECLARE_BOOLEAN_ATTRIBUTE(ScrollLayer, fadingEdgeStart)

    DECLARE_BOOLEAN_ATTRIBUTE(ScrollLayer, fadingEdgeEnd)

    // NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
    Valdi::Result<Valdi::Value> preprocess_scrollPerfLoggerBridge(const Valdi::Value& value);

private:
    Ref<ScrollLayerListener> _scrollLayerListener;
    bool _useAndroidStyleScroller;
};

} // namespace snap::drawing
