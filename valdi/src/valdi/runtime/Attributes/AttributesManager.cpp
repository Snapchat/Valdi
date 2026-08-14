//
//  AttributesManager.cpp
//  ValdiRuntime
//
//  Created by Simon Corsin on 6/26/18.
//  Copyright © 2018 Snap Inc. All rights reserved.
//

#include "valdi/runtime/Attributes/AttributesManager.hpp"
#include "valdi/runtime/Attributes/AttributesBindingContextImpl.hpp"
#include "valdi/runtime/Attributes/DefaultAttributes.hpp"
#include "valdi/runtime/Attributes/Yoga/YogaAttributes.hpp"
#include "valdi/runtime/Interfaces/IViewManager.hpp"
#include "valdi/runtime/Views/MeasureDelegate.hpp"
#include "valdi_core/cpp/Utils/Trace.hpp"

#include "valdi_core/cpp/Utils/LoggerUtils.hpp"

namespace Valdi {

AttributesManager::AttributesManager(IViewManager& viewManager,
                                     AttributeIds& attributeIds,
                                     const Ref<ColorPaletteManager>& colorPaletteManager,
                                     ILogger& logger,
                                     std::shared_ptr<YGConfig> yogaConfig)
    : _viewManager(viewManager),
      _attributeIds(attributeIds),
      _logger(logger),
      _yogaConfig(std::move(yogaConfig)),
      _colorPaletteManager(colorPaletteManager) {}

void AttributesManager::registerPreprocessor(AttributeId attributeId,
                                             Result<Value> (*preprocessor)(const Ref<ColorPaletteManager>&,
                                                                           const Value&)) {
    registerPreprocessor(
        attributeId, [colorPaletteManager = _colorPaletteManager, preprocessor](const Value& value) -> Result<Value> {
            return preprocessor(colorPaletteManager, value);
        });
}

void AttributesManager::registerPreprocessor(AttributeId attributeId,
                                             const Valdi::AttributePreprocessor& preprocessor) {
    std::lock_guard<std::mutex> guard(_mutex);
    auto it = _preprocessors.find(attributeId);
    if (it == _preprocessors.end()) {
        _preprocessors[attributeId].emplace_back(preprocessor);
    } else {
        it->second.emplace_back(preprocessor);
    }
}

void AttributesManager::registerPostprocessor(AttributeId attributeId,
                                              Result<Value> (*postprocessor)(ViewNode& viewNode, const Value& value)) {
    std::lock_guard<std::mutex> guard(_mutex);
    _postprocessors[attributeId].emplace_back(postprocessor);
}

FlatMap<StringBox, SharedBoundAttributes> AttributesManager::getAllBoundAttributes() const {
    std::lock_guard<std::mutex> guard(_mutex);
    return _boundAttributesByClass;
}

SharedBoundAttributes AttributesManager::getAttributesForClass(const StringBox& className) noexcept {
    std::lock_guard<std::mutex> guard(_mutex);
    return lockFreeGetAttributesForClass(className);
}

SharedBoundAttributes AttributesManager::lockFreeGetAttributesForClass(const StringBox& className) noexcept {
    auto it = _boundAttributesByClass.find(className);
    if (it != _boundAttributesByClass.end()) {
        return it->second;
    }

    VALDI_TRACE_META("Valdi.bindAttributes", className);

    auto scrollAttributesBound = false;

    AttributeHandlerById handlerByAttributeName;
    Ref<AttributeHandlerDelegate> defaultAttributeHandlerDelegate;
    Ref<MeasureDelegate> measureDelegate;
    populateAttributeHandlerById(
        handlerByAttributeName, className, defaultAttributeHandlerDelegate, measureDelegate, scrollAttributesBound);

    auto boundAttributes = Valdi::makeShared<BoundAttributes>(className,
                                                              std::move(handlerByAttributeName),
                                                              std::move(defaultAttributeHandlerDelegate),
                                                              measureDelegate,
                                                              _attributeIds,
                                                              scrollAttributesBound);

    _boundAttributesByClass[className] = boundAttributes;
    return boundAttributes;
}

void AttributesManager::populateAttributeHandlerById(AttributeHandlerById& attributeHandlerByName,
                                                     const StringBox& className,
                                                     Ref<AttributeHandlerDelegate>& defaultAttributeHandlerDelegate,
                                                     Ref<MeasureDelegate>& measureDelegate,
                                                     bool& scrollAttributesBound) {
    std::vector<StringBox> classHierarchy;

    auto isLayoutClass = false;

    if (className == AttributesManager::getLayoutPlaceholderClassName()) {
        isLayoutClass = true;
        classHierarchy.push_back(className);
    } else {
        classHierarchy = _viewManager.getClassHierarchy(className);
        classHierarchy.push_back(AttributesManager::getLayoutPlaceholderClassName());
    }

    if (classHierarchy.size() > 1) {
        for (auto i = classHierarchy.size() - 1; i > 0; i--) {
            auto childAttributesApplier = lockFreeGetAttributesForClass(classHierarchy[i]);
            for (const auto& it : childAttributesApplier->getHandlers()) {
                attributeHandlerByName[it.first] = it.second;
            }
            if (childAttributesApplier->isBackingClassScrollable()) {
                scrollAttributesBound = true;
            }
            if (childAttributesApplier->getMeasureDelegate() != nullptr) {
                measureDelegate = childAttributesApplier->getMeasureDelegate();
            }
        }
    } else {
        // Bind the default attributes on the root class
        DefaultAttributes defaultAttributes(_yogaConfig.get(), _attributeIds, _viewManager.getPointScale());
        defaultAttributes.bind(attributeHandlerByName);
    }

    if (!isLayoutClass) {
        AttributesBindingContextImpl binder(_attributeIds, _colorPaletteManager, _logger);
        _viewManager.bindAttributes(className, binder);

        if (binder.getDefaultDelegate() != nullptr) {
            defaultAttributeHandlerDelegate = binder.getDefaultDelegate();
        }
        if (binder.getMeasureDelegate() != nullptr) {
            measureDelegate = binder.getMeasureDelegate();
        }
        if (binder.scrollAttributesBound()) {
            scrollAttributesBound = true;
        }

        std::vector<StringBox> allAttributeNames;
        allAttributeNames.reserve(binder.getHandlers().size());

        for (const auto& it : binder.getHandlers()) {
            auto attributeHandler = it.second;

            const auto& preprocessors = _preprocessors.find(it.first);
            if (preprocessors != _preprocessors.end()) {
                auto index = preprocessors->second.size();
                while (index > 0) {
                    index--;
                    attributeHandler.preprendPreprocessor(preprocessors->second[index], false);
                }
                // Enable preprocessor cache for all default preprocessors.
                attributeHandler.setEnablePreprocessorCache(true);
                attributeHandler.setShouldReevaluateOnColorPaletteChange(true);
            }

            const auto& postprocessors = _postprocessors.find(it.first);
            if (postprocessors != _postprocessors.end()) {
                for (const auto& postprocessor : postprocessors->second) {
                    attributeHandler.appendPostprocessor(postprocessor);
                }
            }

            allAttributeNames.emplace_back(attributeHandler.getName());
            attributeHandlerByName[it.first] = attributeHandler;
        }

        VALDI_DEBUG(_logger,
                    "Bound attributes for backend '{}' of View class '{}' with parent class '{}': [{}]",
                    getBackendString(_viewManager.getRenderingBackendType()),
                    className,
                    classHierarchy.size() > 1 ? classHierarchy[1] : STRING_LITERAL("<none>"),
                    StringBox::join(allAttributeNames, ", "));
    }
}

IViewManager& AttributesManager::getViewManager() const {
    return _viewManager;
}

YGConfig* AttributesManager::getYogaConfig() const {
    return _yogaConfig.get();
}

ILogger& AttributesManager::getLogger() const {
    return _logger;
}

AttributeIds& AttributesManager::getAttributeIds() const {
    return _attributeIds;
}

const Ref<ColorPaletteManager>& AttributesManager::getColorPaletteManager() const {
    return _colorPaletteManager;
}

const StringBox& AttributesManager::getLayoutPlaceholderClassName() {
    static StringBox kClassName = STRING_LITERAL("Layout");
    return kClassName;
}

const StringBox& AttributesManager::getDeferredClassName() {
    static StringBox kClassName = STRING_LITERAL("Deferred");
    return kClassName;
}

} // namespace Valdi
