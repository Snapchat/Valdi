//
//  ThemableAsset.cpp
//  valdi
//

#include "valdi/runtime/Resources/ThemableAsset.hpp"
#include "valdi_core/cpp/Attributes/ColorPalette.hpp"

namespace Valdi {

ThemableAsset::ThemableAsset(FlatMap<StringBox, Ref<Asset>> assetsByColorPalette)
    : _assetsByColorPalette(std::move(assetsByColorPalette)) {
    if (!_assetsByColorPalette.empty()) {
        _representativeAsset = _assetsByColorPalette.begin()->second;
    }
}

ThemableAsset::~ThemableAsset() = default;

bool ThemableAsset::canBeMeasured() const {
    return _representativeAsset != nullptr && _representativeAsset->canBeMeasured();
}

StringBox ThemableAsset::getIdentifier() {
    return _representativeAsset != nullptr ? _representativeAsset->getIdentifier() : StringBox();
}

double ThemableAsset::getWidth() const {
    return _representativeAsset != nullptr ? _representativeAsset->getWidth() : 0.0;
}

double ThemableAsset::getHeight() const {
    return _representativeAsset != nullptr ? _representativeAsset->getHeight() : 0.0;
}

Ref<Asset> ThemableAsset::withConfiguration(const AssetConfiguration& configuration) {
    if (configuration.colorPalette == nullptr) {
        return strongSmallRef(this);
    }

    const auto& it = _assetsByColorPalette.find(configuration.colorPalette->getName());
    if (it == _assetsByColorPalette.end()) {
        return nullptr;
    }

    auto remainingConfiguration = configuration;
    remainingConfiguration.colorPalette = nullptr;
    return it->second->withConfiguration(remainingConfiguration);
}

void ThemableAsset::addLoadObserver(const std::shared_ptr<snap::valdi_core::AssetLoadObserver>& /*observer*/,
                                    snap::valdi_core::AssetOutputType /*outputType*/,
                                    int32_t /*preferredWidth*/,
                                    int32_t /*preferredHeight*/,
                                    const Valdi::Value& /*associatedData*/) {
    // No op. ThemableAsset should be resolved through withConfiguration before rendering.
}

void ThemableAsset::removeLoadObserver(
    const std::shared_ptr<snap::valdi_core::AssetLoadObserver>& /*observer*/) {
    // No op. ThemableAsset should be resolved through withConfiguration before rendering.
}

void ThemableAsset::updateLoadObserverPreferredSize(
    const std::shared_ptr<snap::valdi_core::AssetLoadObserver>& /*observer*/,
    int32_t /*preferredWidth*/,
    int32_t /*preferredHeight*/) {
    // No op. ThemableAsset should be resolved through withConfiguration before rendering.
}

std::optional<AssetLocation> ThemableAsset::getResolvedLocation() const {
    return _representativeAsset != nullptr ? _representativeAsset->getResolvedLocation() : std::nullopt;
}

} // namespace Valdi
