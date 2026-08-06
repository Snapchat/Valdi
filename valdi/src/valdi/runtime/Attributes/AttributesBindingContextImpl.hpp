//
//  AttributesBindingContextImpl.hpp
//  ValdiRuntime
//
//  Created by Simon Corsin on 6/26/18.
//  Copyright © 2018 Snap Inc. All rights reserved.
//

#pragma once

#include "valdi/runtime/Attributes/AttributeHandler.hpp"
#include "valdi/runtime/Attributes/AttributeIds.hpp"
#include "valdi/runtime/Attributes/AttributesBindingContext.hpp"

#include "valdi_core/cpp/Attributes/ColorPalette.hpp"
#include "valdi_core/cpp/Utils/StringBox.hpp"

namespace Valdi {

class ILogger;

class AttributesBindingContextImpl : public AttributesBindingContext {
public:
    AttributesBindingContextImpl(AttributeIds& attributeIds, const Ref<ColorPalette>& colorPalette, ILogger& logger);
    ~AttributesBindingContextImpl() override;

    void registerPreprocessor(const Valdi::StringBox& attribute,
                              bool enableCache,
                              AttributePreprocessor&& preprocessor) override;

    AttributeId bindStringAttribute(const StringBox& attribute,
                                    bool invalidateLayoutOnChange,
                                    const Ref<AttributeHandlerDelegate>& delegate) override;

    AttributeId bindDoubleAttribute(const StringBox& attribute,
                                    bool invalidateLayoutOnChange,
                                    const Ref<AttributeHandlerDelegate>& delegate) override;

    AttributeId bindPercentAttribute(const StringBox& attribute,
                                     bool invalidateLayoutOnChange,
                                     const Ref<AttributeHandlerDelegate>& delegate) override;

    AttributeId bindBooleanAttribute(const StringBox& attribute,
                                     bool invalidateLayoutOnChange,
                                     const Ref<AttributeHandlerDelegate>& delegate) override;

    AttributeId bindIntAttribute(const StringBox& attribute,
                                 bool invalidateLayoutOnChange,
                                 const Ref<AttributeHandlerDelegate>& delegate) override;

    AttributeId bindColorAttribute(const StringBox& attribute,
                                   bool invalidateLayoutOnChange,
                                   const Ref<AttributeHandlerDelegate>& delegate) override;

    AttributeId bindBorderAttribute(const StringBox& attribute,
                                    bool invalidateLayoutOnChange,
                                    const Ref<AttributeHandlerDelegate>& delegate) override;

    AttributeId bindUntypedAttribute(const StringBox& attribute,
                                     bool invalidateLayoutOnChange,
                                     const Ref<AttributeHandlerDelegate>& delegate) override;

    AttributeId bindTextAttribute(const StringBox& attribute,
                                  bool invalidateLayoutOnChange,
                                  const Ref<AttributeHandlerDelegate>& delegate) override;

    AttributeId bindCompositeAttribute(const StringBox& attribute,
                                       const std::vector<snap::valdi_core::CompositeAttributePart>& parts,
                                       const Ref<AttributeHandlerDelegate>& delegate) override;

    AttributeId bindTransformAttributes(const Ref<AttributeHandlerDelegate>& delegate) override;

    void bindScrollAttributes() override;

    void bindAssetAttributes(snap::valdi_core::AssetOutputType assetOutputType) override;

    void setDefaultDelegate(const Ref<AttributeHandlerDelegate>& delegate) override;
    void setMeasureDelegate(const Ref<MeasureDelegate>& measureDelegate) override;

    const AttributeHandlerById& getHandlers() const;
    const Ref<AttributeHandlerDelegate>& getDefaultDelegate() const;
    const Ref<MeasureDelegate>& getMeasureDelegate() const;

    bool scrollAttributesBound() const;

private:
    AttributeIds& _attributeIds;
    Ref<ColorPalette> _colorPalette;
    ILogger& _logger;
    AttributeHandlerById _handlers;
    Ref<AttributeHandlerDelegate> _defaultDelegate;
    Ref<MeasureDelegate> _measureDelegate;
    bool _scrollAttributesBound = false;

    AttributeHandler& registerHandler(const StringBox& attribute,
                                      bool invalidateLayoutOnChange,
                                      const Ref<AttributeHandlerDelegate>& delegate);

    AttributeHandler& registerHandler(const StringBox& attribute,
                                      bool invalidateLayoutOnChange,
                                      bool requiresView,
                                      const Ref<AttributeHandlerDelegate>& delegate);

    void registerColorPreprocessor(AttributeHandler& attributeHandler);
};

} // namespace Valdi
