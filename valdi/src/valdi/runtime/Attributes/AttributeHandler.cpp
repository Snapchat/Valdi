//
//  AttributeHandler.cpp
//  ValdiRuntime
//
//  Created by Simon Corsin on 6/26/18.
//  Copyright © 2018 Snap Inc. All rights reserved.
//

#include "valdi/runtime/Attributes/AttributeHandler.hpp"
#include "utils/debugging/Assert.hpp"
#include "valdi/runtime/Attributes/AttributeHandlerDelegate.hpp"
#include "valdi/runtime/Attributes/PreprocessorCache.hpp"
#include "valdi/runtime/Context/ViewNode.hpp"
#include "valdi_core/cpp/Constants.hpp"
#include "valdi_core/cpp/Utils/LoggerUtils.hpp"
#include "valdi_core/cpp/Utils/Trace.hpp"

namespace Valdi {

AttributeHandler::AttributeHandler() = default;

AttributeHandler::AttributeHandler(AttributeId id,
                                   StringBox name,
                                   const Ref<AttributeHandlerDelegate>& delegate,
                                   Ref<CompositeAttribute> compositeAttribute,
                                   bool requiresView,
                                   bool shouldInvalidateLayoutOnChange)
    : _id(id),
      _name(std::move(name)),
      _delegate(delegate),
      _compositeAttribute(std::move(compositeAttribute)),
      _requiresView(requiresView),
      _shouldInvalidateLayoutOnChange(shouldInvalidateLayoutOnChange) {}

AttributeHandler::AttributeHandler(AttributeId id,
                                   StringBox name,
                                   Ref<CompositeAttribute> compositeAttribute,
                                   bool requiresView,
                                   bool shouldInvalidateLayoutOnChange)
    : _id(id),
      _name(std::move(name)),
      _compositeAttribute(std::move(compositeAttribute)),
      _requiresView(requiresView),
      _shouldInvalidateLayoutOnChange(shouldInvalidateLayoutOnChange),
      _isCompositePart(_compositeAttribute != nullptr && _compositeAttribute->getAttributeId() != id) {}

AttributeHandler::~AttributeHandler() = default;

Result<Void> AttributeHandler::applyAttribute(ViewTransactionScope& viewTransactionScope,
                                              ViewNode& viewNode,
                                              const Valdi::Value& value,
                                              const SharedAnimator& animator) const {
    SC_ASSERT(!requiresView() || viewNode.hasView());

    Result<Void> result;
    auto postprocessedValue = postprocess(viewNode, value);
    if (postprocessedValue) {
        SC_ASSERT(_delegate != nullptr, "AttributeHandler::applyAttribute called with null delegate");
        if (_delegate == nullptr) {
            return Error("AttributeHandler::applyAttribute called with null delegate");
        }
        result = _delegate->onApply(
            viewTransactionScope, viewNode, viewNode.getView(), _name, postprocessedValue.value(), animator);
    } else {
        result = postprocessedValue.moveError();
    }

    if (!result) {
        return result.error().rethrow(STRING_FORMAT("Failed to apply value {}", value.toString()));
    }

    return result;
}

Result<Value> AttributeHandler::preprocessWithoutCache(const Value& value) const {
    auto current = value;
    for (const auto& preprocessor : _preprocessors) {
        if (!(current.isNull() || current.isUndefined())) {
            auto result = preprocessor(current);
            if (!result) {
                return result.error().rethrow(STRING_FORMAT("Failed to preprocess value {}", _name, value.toString()));
            }
            current = result.moveValue();
        }
    }

    return current;
}

Result<PreprocessedValue> AttributeHandler::preprocess(const Value& value) const {
    if (VALDI_LIKELY(_preprocessingTrivial)) {
        if (_preprocessors.empty()) {
            return PreprocessedValue(value);
        }

        auto result = preprocessWithoutCache(value);
        if (!result) {
            return result.moveError();
        }
        return PreprocessedValue(result.moveValue());
    }

    if (_preprocessorCache != nullptr) {
        auto cacheKey = PreprocessorCacheKey(value);

        auto result = _preprocessorCache->get(cacheKey);
        if (result) {
            return *result;
        }

        auto preprocessResult = preprocessWithoutCache(value);
        if (!preprocessResult) {
            return preprocessResult.moveError();
        }

        return _preprocessorCache->store(cacheKey, preprocessResult.moveValue());
    } else {
        auto result = preprocessWithoutCache(value);
        if (!result) {
            return result.moveError();
        }
        return PreprocessedValue(result.moveValue());
    }
}

Result<Value> AttributeHandler::postprocess(ViewNode& viewNode, const Value& value) const {
    Result<Value> out = value;

    for (const auto& postprocessor : _postprocessors) {
        out = postprocessor(viewNode, out.value());
        if (!out) {
            break;
        }
    }

    return out;
}

void AttributeHandler::resetAttribute(ViewTransactionScope& viewTransactionScope,
                                      ViewNode& viewNode,
                                      const SharedAnimator& animator) const {
    SC_ASSERT(!requiresView() || viewNode.hasView());
    SC_ASSERT(_delegate != nullptr, "AttributeHandler::resetAttribute called with null delegate");

    if (_delegate == nullptr) {
        return;
    }

    _delegate->onReset(viewTransactionScope, viewNode, viewNode.getView(), _name, animator);
}

void AttributeHandler::appendPreprocessor(Result<Value> (*preprocessor)(const Value&), bool isTrivial) {
    appendPreprocessor(AttributePreprocessor(preprocessor), isTrivial);
}

void AttributeHandler::preprendPreprocessor(Result<Value> (*preprocessor)(const Value&), bool isTrivial) {
    preprendPreprocessor(AttributePreprocessor(preprocessor), isTrivial);
}

void AttributeHandler::appendPreprocessor(Valdi::AttributePreprocessor preprocessor, bool isTrivial) {
    _preprocessors.emplace_back(std::move(preprocessor));
    if (!isTrivial) {
        _preprocessingTrivial = false;
    }
}

void AttributeHandler::preprendPreprocessor(AttributePreprocessor preprocessor, bool isTrivial) {
    _preprocessors.emplace(_preprocessors.begin(), std::move(preprocessor));
    if (!isTrivial) {
        _preprocessingTrivial = false;
    }
}

void AttributeHandler::appendPostprocessor(AttributePostprocessor postprocessor) {
    _postprocessors.emplace_back(std::move(postprocessor));
}

bool AttributeHandler::hasPreprocessors() const {
    return !_preprocessors.empty();
}

void AttributeHandler::clearPreprocessorCache() {
    if (_preprocessorCache != nullptr) {
        _preprocessorCache->clear();
    }
}

const Ref<CompositeAttribute>& AttributeHandler::getCompositeAttribute() const {
    return _compositeAttribute;
}

const Ref<AttributeHandlerDelegate>& AttributeHandler::getDelegate() const {
    return _delegate;
}

bool AttributeHandler::requiresView() const {
    return _requiresView;
}

bool AttributeHandler::shouldInvalidateLayoutOnChange() const {
    return _shouldInvalidateLayoutOnChange;
}

const StringBox& AttributeHandler::getName() const {
    return _name;
}

AttributeId AttributeHandler::getId() const {
    return _id;
}

AttributeHandler AttributeHandler::withDelegate(const Ref<AttributeHandlerDelegate>& delegate) const {
    AttributeHandler handler(_id, _name, delegate, _compositeAttribute, _requiresView, _shouldInvalidateLayoutOnChange);

    if (_preprocessorCache != nullptr) {
        handler.setEnablePreprocessorCache(true);
    }

    if (_shouldReevaluateOnColorPaletteChange) {
        handler.setShouldReevaluateOnColorPaletteChange(true);
    }

    handler._preprocessors = _preprocessors;
    if (!_preprocessingTrivial) {
        handler._preprocessingTrivial = false;
    }

    return handler;
}

void AttributeHandler::setEnablePreprocessorCache(bool enablePreprocessorCache) {
    if (enablePreprocessorCache) {
        if (_preprocessorCache == nullptr) {
            _preprocessorCache = Valdi::makeShared<PreprocessorCache>();
        }
    } else {
        _preprocessorCache = nullptr;
    }
}

void AttributeHandler::setShouldReevaluateOnColorPaletteChange(bool reevaluateOnColorPaletteChange) {
    _shouldReevaluateOnColorPaletteChange = reevaluateOnColorPaletteChange;
}

bool AttributeHandler::shouldReevaluateOnColorPaletteChange() const {
    return _shouldReevaluateOnColorPaletteChange;
}

bool AttributeHandler::isCompositePart() const {
    return _isCompositePart;
}

bool AttributeHandler::isPreprocessingTrivial() const {
    return _preprocessingTrivial;
}

} // namespace Valdi
