//
//  AssetAttributes.cpp
//  valdi
//
//  Created by Simon Corsin on 8/10/21.
//

#include "valdi/runtime/Attributes/AssetAttributes.hpp"

#include "valdi/runtime/Attributes/CompositeAttribute.hpp"
#include "valdi/runtime/Attributes/CompositeAttributeUtils.hpp"
#include "valdi/runtime/Resources/AssetResolver.hpp"
#include "valdi/runtime/Views/DefaultMeasureDelegate.hpp"

#include "valdi/runtime/Context/ViewNode.hpp"
#include "valdi/runtime/Context/ViewNodeAssetHandler.hpp"

#include "valdi_core/AssetLoadObserver.hpp"
#include "valdi_core/cpp/Attributes/ImageFilter.hpp"
#include "valdi_core/cpp/Resources/LoadedAsset.hpp"

namespace Valdi {

static bool isLoadedAsset(const Value& value) {
    return value.getTypedRef<LoadedAsset>() != nullptr;
}

class AssetMeasureDelegate : public DefaultMeasureDelegate {
public:
    AssetMeasureDelegate() = default;
    ~AssetMeasureDelegate() override = default;

    Valdi::Size onMeasure(const Valdi::Ref<Valdi::ValueMap>& attributes,
                          float width,
                          Valdi::MeasureMode widthMode,
                          float height,
                          Valdi::MeasureMode heightMode,
                          bool isRightToLeft) override {
        const auto* compositeValue = Value(attributes).getMapValue("srcOnLoad").getArray();
        if (compositeValue == nullptr || compositeValue->empty()) {
            return Size();
        }

        auto asset = castOrNull<Asset>((*compositeValue)[0].getValdiObject());
        if (asset == nullptr) {
            return Size();
        }

        asset = asset->withConfiguration(AssetConfiguration(nullptr, std::nullopt, isRightToLeft));
        if (asset == nullptr) {
            return Size();
        }

        double maxWidth = widthMode == MeasureMode::MeasureModeUnspecified ? -1 : static_cast<double>(width);
        double maxHeight = heightMode == MeasureMode::MeasureModeUnspecified ? -1 : static_cast<double>(height);

        auto size = asset->measure(maxWidth, maxHeight);

        return Size(static_cast<float>(size.first), static_cast<float>(size.second));
    }
};

class AssetSrcAttributeHandlerDelegate : public AttributeHandlerDelegate {
public:
    explicit AssetSrcAttributeHandlerDelegate(snap::valdi_core::AssetOutputType assetOutputType)
        : _assetOutputType(assetOutputType) {}
    ~AssetSrcAttributeHandlerDelegate() override = default;

    Result<Void> onApply(ViewTransactionScope& viewTransactionScope,
                         ViewNode& viewNode,
                         const Ref<View>& view,
                         [[maybe_unused]] const StringBox& name,
                         const Value& value,
                         [[maybe_unused]] const Ref<Animator>& animator) override {
        const auto* compositeValue = value.getArray();
        if (compositeValue == nullptr || compositeValue->size() != 4) {
            return Error("Expecting 4 values");
        }

        auto assetValue = (*compositeValue)[0];
        auto callback = (*compositeValue)[1].getFunctionRef();
        auto flipOnRtl = (*compositeValue)[2].toBool();
        auto associatedData = (*compositeValue)[3];

        if (isLoadedAsset(assetValue)) {
            getViewNodeAssetHandler(viewNode)->setLoadedAsset(
                viewTransactionScope, view, assetValue.getTypedRef<LoadedAsset>(), callback, associatedData, flipOnRtl);
        } else {
            Ref<Asset> asset;
            if (!assetValue.isNullOrUndefined()) {
                auto result = AssetResolver::resolve(viewNode, assetValue);
                if (!result) {
                    return result.moveError();
                }
                asset = result.value()->withConfiguration(AssetConfiguration(
                    viewNode.getResolvedColorPalette(), viewNode.getPlatformType(), viewNode.isRightToLeft()));
            }

            setAsset(viewTransactionScope, viewNode, view, asset, callback, associatedData, flipOnRtl);
        }

        return Void();
    }

    void onReset(ViewTransactionScope& viewTransactionScope,
                 ViewNode& viewNode,
                 const Ref<View>& view,
                 [[maybe_unused]] const StringBox& name,
                 [[maybe_unused]] const Ref<Animator>& animator) override {
        setAsset(viewTransactionScope, viewNode, view, nullptr, nullptr, Value(), false);
    }

private:
    void setAsset(ViewTransactionScope& viewTransactionScope,
                  ViewNode& viewNode,
                  const Ref<View>& view,
                  const Ref<Asset>& asset,
                  const Ref<ValueFunction>& onLoadCallback,
                  const Value& associatedData,
                  bool flipOnRtl) {
        getViewNodeAssetHandler(viewNode)->setAsset(
            viewTransactionScope, view, asset, onLoadCallback, associatedData, flipOnRtl);
    }

    Ref<ViewNodeAssetHandler> getViewNodeAssetHandler(ViewNode& viewNode) {
        auto assetHandler = castOrNull<ViewNodeAssetHandler>(viewNode.getAssetHandler());
        if (assetHandler == nullptr) {
            assetHandler = makeShared<ViewNodeAssetHandler>(viewNode, _assetOutputType);
            viewNode.setAssetHandler(assetHandler);
        }

        return assetHandler;
    }

    snap::valdi_core::AssetOutputType _assetOutputType;
};

static Result<Value> postprocessAsset(ViewNode& viewNode, const Value& value) {
    if (value.isNullOrUndefined()) {
        return value;
    }

    if (isLoadedAsset(value)) {
        return value;
    }

    auto result = AssetResolver::resolve(viewNode, value);
    if (!result) {
        return result.moveError();
    }
    return Value(result.moveValue());
}

static Error invalidFilterError() {
    return Error("Invalid filter");
}

static Result<Value> preprocessFilter(const Value& value) {
    if (value.isNullOrUndefined()) {
        return value;
    }

    const auto* array = value.getArray();
    if (array == nullptr) {
        return invalidFilterError();
    }

    if (array->empty()) {
        return Value();
    }

    static constexpr int32_t kFilterTypeBlur = 1;
    static constexpr int32_t kFilterTypeColorMatrix = 2;

    Ref<ImageFilter> filter = makeShared<ImageFilter>();

    size_t i = 0;
    size_t end = array->size();

    while (i < end) {
        switch ((*array)[i++].toInt()) {
            case kFilterTypeBlur:
                if (i + 1 > end) {
                    return invalidFilterError();
                }
                if (filter->getBlurRadius() != 0.0f) {
                    return Error("Multiple blur filters are not supported");
                }
                filter->setBlurRadius((*array)[i++].toFloat());
                break;
            case kFilterTypeColorMatrix:
                if (i + ImageFilter::kColorMatrixSize > end) {
                    return invalidFilterError();
                }
                float colorMatrix[ImageFilter::kColorMatrixSize];
                for (float& j : colorMatrix) {
                    j = (*array)[i++].toFloat();
                }

                filter->concatColorMatrix(colorMatrix);
                break;
            default:
                return invalidFilterError();
        }
    }

    return Value(filter);
}

AssetAttributes::AssetAttributes(AttributeIds& attributeIds, snap::valdi_core::AssetOutputType assetOutputType)
    : _attributeIds(attributeIds), _assetOutputType(assetOutputType) {}

void AssetAttributes::bind(AttributeHandlerById& attributes, Ref<MeasureDelegate>& outMeasureDelegate) {
    auto srcAttributeId = _attributeIds.getIdForName("src");
    auto filterAttributeId = _attributeIds.getIdForName("filter");
    auto fontProviderAttributeId = _attributeIds.getIdForName("fontProvider");

    std::vector<CompositeAttributePart> parts;
    parts.emplace_back(srcAttributeId, true, true);
    parts.emplace_back(_attributeIds.getIdForName("onAssetLoad"), true);
    parts.emplace_back(_attributeIds.getIdForName("flipOnRtl"), true);

    if (_assetOutputType == snap::valdi_core::AssetOutputType::Lottie) {
        parts.emplace_back(fontProviderAttributeId, true);
    } else {
        parts.emplace_back(filterAttributeId, true);
    }

    auto srcOnLoad = STRING_LITERAL("srcOnLoad");
    auto compositeAttribute =
        Valdi::makeShared<CompositeAttribute>(_attributeIds.getIdForName(srcOnLoad), srcOnLoad, std::move(parts));

    Valdi::bindCompositeAttribute(_attributeIds,
                                  attributes,
                                  compositeAttribute,
                                  makeShared<AssetSrcAttributeHandlerDelegate>(_assetOutputType),
                                  true);

    auto& srcOnLoadHandler = attributes[_attributeIds.getIdForName(srcOnLoad)];
    srcOnLoadHandler.setShouldReevaluateOnColorPaletteChange(true);

    auto& srcHandler = attributes[srcAttributeId];
    srcHandler.appendPostprocessor(postprocessAsset);
    srcHandler.setShouldReevaluateOnColorPaletteChange(true);

    if (_assetOutputType != snap::valdi_core::AssetOutputType::Lottie) {
        auto& filterHandler = attributes[filterAttributeId];
        filterHandler.appendPreprocessor(preprocessFilter, false);
    }

    outMeasureDelegate = makeShared<AssetMeasureDelegate>();
}

} // namespace Valdi
