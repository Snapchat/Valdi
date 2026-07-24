//
//  AttributesManager.hpp
//  ValdiRuntime
//
//  Created by Simon Corsin on 6/26/18.
//  Copyright © 2018 Snap Inc. All rights reserved.
//

#pragma once

#include <string>
#include <vector>

#include "valdi/runtime/Attributes/AttributeHandler.hpp"
#include "valdi/runtime/Attributes/AttributeIds.hpp"
#include "valdi/runtime/Attributes/BoundAttributes.hpp"
#include "valdi_core/cpp/Attributes/ColorPalette.hpp"
#include "valdi_core/cpp/Utils/FlatMap.hpp"
#include "valdi_core/cpp/Utils/StringBox.hpp"
#include <mutex>

struct YGConfig;

namespace Valdi {

class IViewManager;
class ILogger;

class AttributesManager {
public:
    AttributesManager(IViewManager& viewManager,
                      AttributeIds& attributeIds,
                      const Ref<ColorPaletteManager>& colorPaletteManager,
                      ILogger& logger,
                      std::shared_ptr<YGConfig> yogaConfig);

    void registerPreprocessor(AttributeId attributeId, const AttributePreprocessor& preprocessor);
    void registerPreprocessor(AttributeId attributeId,
                              Result<Value> (*preprocessor)(const Ref<ColorPaletteManager>&, const Value&));
    void registerPostprocessor(AttributeId attributeId,
                               Result<Value> (*postprocessor)(ViewNode& viewNode, const Value& value));

    SharedBoundAttributes getAttributesForClass(const StringBox& className) noexcept;
    FlatMap<StringBox, SharedBoundAttributes> getAllBoundAttributes() const;
    IViewManager& getViewManager() const;
    ILogger& getLogger() const;
    YGConfig* getYogaConfig() const;

    AttributeIds& getAttributeIds() const;
    const Ref<ColorPaletteManager>& getColorPaletteManager() const;

    static const StringBox& getLayoutPlaceholderClassName();
    static const StringBox& getDeferredClassName();

private:
    IViewManager& _viewManager;
    AttributeIds& _attributeIds;
    ILogger& _logger;
    std::shared_ptr<YGConfig> _yogaConfig;
    FlatMap<StringBox, SharedBoundAttributes> _boundAttributesByClass;
    FlatMap<AttributeId, std::vector<AttributePreprocessor>> _preprocessors;
    FlatMap<AttributeId, std::vector<AttributePostprocessor>> _postprocessors;
    Ref<ColorPaletteManager> _colorPaletteManager;
    mutable std::mutex _mutex;

    SharedBoundAttributes lockFreeGetAttributesForClass(const StringBox& className) noexcept;

    void populateAttributeHandlerById(AttributeHandlerById& attributeHandlerByName,
                                      const StringBox& className,
                                      Ref<AttributeHandlerDelegate>& defaultAttributeHandlerDelegate,
                                      Ref<MeasureDelegate>& measureDelegate,
                                      bool& scrollAttributesBound);
};

} // namespace Valdi
