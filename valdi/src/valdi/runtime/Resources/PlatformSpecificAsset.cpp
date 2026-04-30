//
//  PlatformSpecificAsset.cpp
//  valdi
//
//  Created by Tom Humphrey on 6/20/23.
//

#include "valdi/runtime/Resources/PlatformSpecificAsset.hpp"
#include <cstddef>
#include <iostream>

namespace Valdi {

// Add platforms to this initialiser as and when customisation is needed on them.
// Until then, unknown platforms will use `defaultAsset`.
// `defaultAsset` also used for sizing/measurement purposes.
PlatformSpecificAsset::PlatformSpecificAsset(const Ref<Asset>& defaultAsset,
                                             const Ref<Asset>& iOSAsset,
                                             const Ref<Asset>& androidAsset)
    : _defaultAsset(defaultAsset), _iOSAsset(iOSAsset), _androidAsset(androidAsset) {}
PlatformSpecificAsset::~PlatformSpecificAsset() = default;

bool PlatformSpecificAsset::canBeMeasured() const {
    return _defaultAsset->canBeMeasured();
}

StringBox PlatformSpecificAsset::getIdentifier() {
    return _defaultAsset->getIdentifier();
}

double PlatformSpecificAsset::getWidth() const {
    return _defaultAsset->getWidth();
}

double PlatformSpecificAsset::getHeight() const {
    return _defaultAsset->getHeight();
}

Ref<Asset> PlatformSpecificAsset::withPlatform(PlatformType platformType) {
    switch (platformType) {
        case PlatformTypeAndroid:
            if (_androidAsset == nullptr) {
                return _defaultAsset;
            }
            return _androidAsset;
        case PlatformTypeIOS:
        case PlatformTypeMacOS:
        case PlatformTypeWeb:
            if (_iOSAsset == nullptr) {
                return _defaultAsset;
            }
            return _iOSAsset;
        default:
            return _defaultAsset;
    }
}

void PlatformSpecificAsset::addLoadObserver(const std::shared_ptr<snap::valdi_core::AssetLoadObserver>& observer,
                                            snap::valdi_core::AssetOutputType outputType,
                                            int32_t preferredWidth,
                                            int32_t preferredHeight,
                                            const Valdi::Value& associatedData) {
    // No op. If this method is called it is a mistake.
    // addLoadObserver is called during rendering,
    // but a PlatformSpecificAsset should never be rendered.
    // Rather it should be resolved (via `withPlatform`) prior to rendering,
    // back to a normal asset.
}

void PlatformSpecificAsset::removeLoadObserver(const std::shared_ptr<snap::valdi_core::AssetLoadObserver>& observer) {
    // No op. If this method is called it is a mistake.
    // removeLoadObserver is called during rendering,
    // but a PlatformSpecificAsset should never be rendered.
    // Rather it should be resolved (via `withPlatform`) prior to rendering,
    // back to a normal asset.
}

void PlatformSpecificAsset::updateLoadObserverPreferredSize(
    const std::shared_ptr<snap::valdi_core::AssetLoadObserver>& observer,
    int32_t preferredWidth,
    int32_t preferredHeight) {
    // No op. If this method is called it is a mistake.
    // updateLoadObserverPreferredSize is called during rendering,
    // but a PlatformSpecificAsset should never be rendered.
    // Rather it should be resolved (via `withPlatform`) prior to rendering,
    // back to a normal asset.
}

std::optional<AssetLocation> PlatformSpecificAsset::getResolvedLocation() const {
    return _defaultAsset->getResolvedLocation();
}

} // namespace Valdi
