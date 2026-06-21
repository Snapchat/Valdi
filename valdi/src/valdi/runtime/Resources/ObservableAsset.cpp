//
//  ObservableAsset.cpp
//  valdi
//
//  Created by Simon Corsin on 6/28/21.
//

#include "valdi/runtime/Resources/ObservableAsset.hpp"
#include "valdi/runtime/Context/Context.hpp"
#include "valdi/runtime/Resources/AssetsManager.hpp"

#include "valdi/runtime/Interfaces/IViewManager.hpp"

#include "valdi_core/cpp/Views/Measure.hpp"

namespace Valdi {

ObservableAsset::ObservableAsset(const AssetKey& assetKey, Weak<AssetsManager> assetsManager)
    : _assetKey(assetKey), _assetsManager(std::move(assetsManager)) {}

ObservableAsset::~ObservableAsset() {
    auto assetsManager = getAssetsManager();
    if (assetsManager != nullptr) {
        assetsManager->onObservableDestroyed(_assetKey);
    }
}

void ObservableAsset::setExpectedSize(int expectedWidth, int expectedHeight) {
    _expectedWidth = expectedWidth;
    _expectedHeight = expectedHeight;
}

StringBox ObservableAsset::getIdentifier() {
    return _assetKey.toString();
}

void ObservableAsset::addLoadObserver(const std::shared_ptr<snap::valdi_core::AssetLoadObserver>& observer,
                                      snap::valdi_core::AssetOutputType outputType,
                                      int32_t preferredWidth,
                                      int32_t preferredHeight,
                                      const Valdi::Value& attachedData) {
    auto assetsManager = getAssetsManager();
    if (assetsManager != nullptr) {
        assetsManager->addAssetLoadObserver(
            _assetKey, observer, Context::currentRef(), outputType, preferredWidth, preferredHeight, attachedData);
    }
}

void ObservableAsset::removeLoadObserver(const std::shared_ptr<snap::valdi_core::AssetLoadObserver>& observer) {
    auto assetsManager = getAssetsManager();
    if (assetsManager != nullptr) {
        assetsManager->removeAssetLoadObserver(_assetKey, observer);
    }
}

void ObservableAsset::updateLoadObserverPreferredSize(
    const std::shared_ptr<snap::valdi_core::AssetLoadObserver>& observer,
    int32_t preferredWidth,
    int32_t preferredHeight) {
    auto assetsManager = getAssetsManager();
    if (assetsManager != nullptr) {
        assetsManager->updateAssetLoadObserverPreferredSize(_assetKey, observer, preferredWidth, preferredHeight);
    }
}

double ObservableAsset::getWidth() const {
    return static_cast<double>(_expectedWidth);
}

double ObservableAsset::getHeight() const {
    return static_cast<double>(_expectedHeight);
}

Shared<AssetsManager> ObservableAsset::getAssetsManager() const {
    return _assetsManager.lock();
}

bool ObservableAsset::canBeMeasured() const {
    return _expectedWidth > 0 && _expectedHeight > 0;
}

std::optional<AssetLocation> ObservableAsset::getResolvedLocation() const {
    auto assetsManager = getAssetsManager();
    if (assetsManager == nullptr) {
        return std::nullopt;
    }

    return assetsManager->getResolvedAssetLocation(_assetKey);
}

} // namespace Valdi
