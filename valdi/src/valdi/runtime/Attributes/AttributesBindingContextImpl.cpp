//
//  AttributesBindingContextImpl.cpp
//  ValdiRuntime
//
//  Created by Simon Corsin on 6/26/18.
//  Copyright © 2018 Snap Inc. All rights reserved.
//

#include "valdi/runtime/Attributes/AttributesBindingContextImpl.hpp"
#include "valdi/runtime/Attributes/CompositeAttribute.hpp"
#include "valdi/runtime/Attributes/ValueConverters.hpp"
#include "valdi/runtime/Context/ViewNode.hpp"
#include "valdi/runtime/Context/ViewNodeTree.hpp"

#include "valdi/runtime/Attributes/AssetAttributes.hpp"
#include "valdi/runtime/Attributes/AttributeHandlerDelegateWithCallable.hpp"
#include "valdi/runtime/Attributes/CompositeAttribute.hpp"
#include "valdi/runtime/Attributes/ScrollAttributes.hpp"
#include "valdi/runtime/Attributes/TextAttributeValueParser.hpp"
#include "valdi/runtime/Attributes/ValueConverters.hpp"
#include "valdi/runtime/Views/MeasureDelegate.hpp"

#include "valdi/runtime/Runtime.hpp"
#include "valdi_core/cpp/Attributes/AttributeUtils.hpp"
#include "valdi_core/cpp/Resources/Asset.hpp"
#include "valdi_core/cpp/Utils/Format.hpp"
#include "valdi_core/cpp/Utils/StringCache.hpp"
#include "valdi_core/cpp/Utils/Trace.hpp"
#include "valdi_core/cpp/Utils/ValueArray.hpp"

#include "valdi_core/CompositeAttributePart.hpp"

#include "utils/platform/BuildOptions.hpp"

namespace Valdi {

static Result<Value> preprocessDouble(const Value& value) {
    return ValueConverter::toDouble(value).map<Value>();
}

static Result<Value> preprocessInt(const Value& value) {
    return ValueConverter::toInt(value).map<Value>();
}

static Result<Value> preprocessBoolean(const Value& value) {
    return ValueConverter::toBool(value).map<Value>();
}

static Result<Value> preprocessString(const Value& value) {
    return ValueConverter::toString(value).map<Value>();
}

static Result<Value> postprocessColor(ViewNode& viewNode, const Value& value) {
    if (!value.isString()) {
        return value;
    }

    const auto& colorPalette = viewNode.getResolvedColorPalette();
    if (colorPalette == nullptr) {
        return Error("ViewNode has no resolved ColorPalette");
    }
    return ValueConverter::toColor(*colorPalette, value);
}

AttributesBindingContextImpl::AttributesBindingContextImpl(AttributeIds& attributeIds,
                                                           const Ref<ColorPaletteManager>& colorPaletteManager,
                                                           ILogger& logger)
    : _attributeIds(attributeIds), _colorPaletteManager(colorPaletteManager), _logger(logger) {}

AttributesBindingContextImpl::~AttributesBindingContextImpl() = default;

void AttributesBindingContextImpl::registerPreprocessor(const Valdi::StringBox& attribute,
                                                        bool enableCache,
                                                        AttributePreprocessor&& preprocessor) {
    auto attributeId = _attributeIds.getIdForName(attribute);
    auto it = _handlers.find(attributeId);
    SC_ASSERT(it != _handlers.end(), "Preprocessors must be registered after an attribute has been bound.");
    it->second.appendPreprocessor(std::move(preprocessor), false);
    if (enableCache) {
        it->second.setEnablePreprocessorCache(enableCache);
    }
}

AttributeId AttributesBindingContextImpl::bindStringAttribute(const StringBox& attribute,
                                                              bool invalidateLayoutOnChange,
                                                              const Ref<AttributeHandlerDelegate>& delegate) {
    auto& registeredHandler = registerHandler(attribute, invalidateLayoutOnChange, delegate);
    registeredHandler.appendPreprocessor(&preprocessString, true);
    return registeredHandler.getId();
}

AttributeId AttributesBindingContextImpl::bindIntAttribute(const StringBox& attribute,
                                                           bool invalidateLayoutOnChange,
                                                           const Ref<AttributeHandlerDelegate>& delegate) {
    auto& registeredHandler = registerHandler(attribute, invalidateLayoutOnChange, delegate);
    registeredHandler.appendPreprocessor(&preprocessInt, true);
    return registeredHandler.getId();
}

AttributeId AttributesBindingContextImpl::bindColorAttribute(const StringBox& attribute,
                                                             bool invalidateLayoutOnChange,
                                                             const Ref<AttributeHandlerDelegate>& delegate) {
    auto& registeredHandler = registerHandler(attribute, invalidateLayoutOnChange, delegate);

    registerColorPreprocessor(registeredHandler);
    return registeredHandler.getId();
}

AttributeId AttributesBindingContextImpl::bindDoubleAttribute(const StringBox& attribute,
                                                              bool invalidateLayoutOnChange,
                                                              const Ref<AttributeHandlerDelegate>& delegate) {
    auto& registeredHandler = registerHandler(attribute, invalidateLayoutOnChange, delegate);
    registeredHandler.appendPreprocessor(&preprocessDouble, true);
    return registeredHandler.getId();
}

AttributeId AttributesBindingContextImpl::bindPercentAttribute(const StringBox& attribute,
                                                               bool invalidateLayoutOnChange,
                                                               const Ref<AttributeHandlerDelegate>& delegate) {
    auto& registeredHandler = registerHandler(attribute, invalidateLayoutOnChange, delegate);
    return registeredHandler.getId();
}

AttributeId AttributesBindingContextImpl::bindTextAttribute(const StringBox& attribute,
                                                            bool invalidateLayoutOnChange,
                                                            const Ref<AttributeHandlerDelegate>& delegate) {
    auto& registeredHandler = registerHandler(attribute, invalidateLayoutOnChange, delegate);
    registeredHandler.appendPostprocessor([logger = &_logger](ViewNode& viewNode, const Value& value) -> Result<Value> {
        if (value.isString()) {
            return value;
        }
        // strict parsing for non production build
        auto strict = !snap::kIsAppstoreBuild;
        const auto& colorPalette = viewNode.getResolvedColorPalette();
        if (colorPalette == nullptr) {
            return Error("ViewNode has no resolved ColorPalette");
        }
        return TextAttributeValueParser::parse(*colorPalette, value, *logger, strict);
    });
    registeredHandler.setShouldReevaluateOnColorPaletteChange(true);

    return registeredHandler.getId();
}

AttributeId AttributesBindingContextImpl::bindBooleanAttribute(const StringBox& attribute,
                                                               bool invalidateLayoutOnChange,
                                                               const Ref<AttributeHandlerDelegate>& delegate) {
    auto& registeredHandler = registerHandler(attribute, invalidateLayoutOnChange, delegate);
    registeredHandler.appendPreprocessor(&preprocessBoolean, true);
    return registeredHandler.getId();
}

AttributeId AttributesBindingContextImpl::bindBorderAttribute(const StringBox& attribute,
                                                              bool invalidateLayoutOnChange,
                                                              const Ref<AttributeHandlerDelegate>& delegate) {
    auto& registeredHandler = registerHandler(attribute, invalidateLayoutOnChange, delegate);

    return registeredHandler.getId();
}

AttributeId AttributesBindingContextImpl::bindUntypedAttribute(const StringBox& attribute,
                                                               bool invalidateLayoutOnChange,
                                                               const Ref<AttributeHandlerDelegate>& delegate) {
    auto& registeredHandler = registerHandler(attribute, invalidateLayoutOnChange, delegate);

    return registeredHandler.getId();
}

AttributeId AttributesBindingContextImpl::bindCompositeAttribute(
    const StringBox& attribute,
    const std::vector<snap::valdi_core::CompositeAttributePart>& parts,
    const Ref<AttributeHandlerDelegate>& delegate) {
    std::vector<CompositeAttributePart> outParts;
    outParts.reserve(parts.size());

    for (const auto& part : parts) {
        outParts.emplace_back(_attributeIds.getIdForName(part.attribute), part.optional, part.invalidateLayoutOnChange);
    }

    auto compositeAttribute =
        Valdi::makeShared<CompositeAttribute>(_attributeIds.getIdForName(attribute), attribute, std::move(outParts));

    for (const auto& part : parts) {
        auto attributeId = _attributeIds.getIdForName(part.attribute);

        auto handler =
            AttributeHandler(attributeId, part.attribute, compositeAttribute, false, part.invalidateLayoutOnChange);
        AttributePreprocessor preprocessor;
        switch (part.type) {
            case snap::valdi_core::AttributeType::String:
                handler.appendPreprocessor(&preprocessString, true);
                break;
            case snap::valdi_core::AttributeType::Double:
                handler.appendPreprocessor(&preprocessDouble, true);
                break;
            case snap::valdi_core::AttributeType::Int:
                handler.appendPreprocessor(&preprocessInt, true);
                break;
            case snap::valdi_core::AttributeType::Boolean:
                handler.appendPreprocessor(&preprocessBoolean, true);
                break;
            case snap::valdi_core::AttributeType::Color:
                registerColorPreprocessor(handler);
                break;
            case snap::valdi_core::AttributeType::Untyped:
                break;
        }

        _handlers[attributeId] = handler;
    }

    auto attributeId = _attributeIds.getIdForName(attribute);

    _handlers[attributeId] = AttributeHandler(attributeId,
                                              attribute,
                                              delegate,
                                              compositeAttribute,
                                              true,
                                              compositeAttribute->shouldInvalidateLayoutOnChange());

    return attributeId;
}

void AttributesBindingContextImpl::bindScrollAttributes() {
    ScrollAttributes scrollAttributes(_attributeIds);
    scrollAttributes.bind(_handlers);

    _scrollAttributesBound = true;
}

void AttributesBindingContextImpl::bindAssetAttributes(snap::valdi_core::AssetOutputType assetOutputType) {
    AssetAttributes assetAttributes(_attributeIds, assetOutputType);
    assetAttributes.bind(_handlers, _measureDelegate);
}

bool AttributesBindingContextImpl::scrollAttributesBound() const {
    return _scrollAttributesBound;
}

void AttributesBindingContextImpl::setDefaultDelegate(const Ref<AttributeHandlerDelegate>& delegate) {
    _defaultDelegate = delegate;
}

const Ref<AttributeHandlerDelegate>& AttributesBindingContextImpl::getDefaultDelegate() const {
    return _defaultDelegate;
}

void AttributesBindingContextImpl::setMeasureDelegate(const Ref<MeasureDelegate>& measureDelegate) {
    _measureDelegate = measureDelegate;
}

const Ref<MeasureDelegate>& AttributesBindingContextImpl::getMeasureDelegate() const {
    return _measureDelegate;
}

AttributeHandler& AttributesBindingContextImpl::registerHandler(const StringBox& attribute,
                                                                bool invalidateLayoutOnChange,
                                                                const Ref<AttributeHandlerDelegate>& delegate) {
    return registerHandler(attribute, invalidateLayoutOnChange, true, delegate);
}

AttributeHandler& AttributesBindingContextImpl::registerHandler(const StringBox& attribute,
                                                                bool invalidateLayoutOnChange,
                                                                bool requiresView,
                                                                const Ref<AttributeHandlerDelegate>& delegate) {
    auto attributeId = _attributeIds.getIdForName(attribute);
    auto& createdHandler = _handlers[attributeId] =
        AttributeHandler(attributeId, attribute, delegate, nullptr, requiresView, invalidateLayoutOnChange);

    return createdHandler;
}

const AttributeHandlerById& AttributesBindingContextImpl::getHandlers() const {
    return _handlers;
}

void AttributesBindingContextImpl::registerColorPreprocessor(AttributeHandler& handler) {
    handler.appendPreprocessor(&ValueConverter::toColorValue, false);
    handler.appendPostprocessor(&postprocessColor);
    handler.setShouldReevaluateOnColorPaletteChange(true);
}

} // namespace Valdi
