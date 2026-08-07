//
//  PlatformSpecificAsset.hpp
//  valdi
//
//  Created by Tom Humphrey on 6/20/23.
//

#pragma once

#include "valdi/runtime/Resources/AssetKey.hpp"
#include "valdi_core/cpp/Resources/Asset.hpp"

namespace Valdi {

/**
 A PlatformSpecificAsset holds a default asset and can hold additional
 assets that should only be used on specific platforms (e.g. iOS, Android).

 The concrete intention when it was created was to allow the 'same' coreui icon to
 be rendered using default valdi bitmap rendering mechanisms on desktop + web, and
 custom native vector asset rendering on iOS + Android.

 ie. This class exists for when you need to divide and conquer asset rendering by Valdi platform.
 */
class PlatformSpecificAsset : public Asset {
public:
    PlatformSpecificAsset(const Ref<Asset>& defaultAsset, const Ref<Asset>& iOSAsset, const Ref<Asset>& androidAsset);
    ~PlatformSpecificAsset() override;

    bool canBeMeasured() const final;

    StringBox getIdentifier() final;

    double getWidth() const final;
    double getHeight() const final;

    Ref<Asset> withConfiguration(const AssetConfiguration& configuration) final;

    void addLoadObserver(const std::shared_ptr<snap::valdi_core::AssetLoadObserver>& observer,
                         snap::valdi_core::AssetOutputType outputType,
                         int32_t preferredWidth,
                         int32_t preferredHeight,
                         const Valdi::Value& associatedData) final;

    void removeLoadObserver(const std::shared_ptr<snap::valdi_core::AssetLoadObserver>& observer) final;

    void updateLoadObserverPreferredSize(const std::shared_ptr<snap::valdi_core::AssetLoadObserver>& observer,
                                         int32_t preferredWidth,
                                         int32_t preferredHeight) final;

    std::optional<AssetLocation> getResolvedLocation() const final;

private:
    Ref<Asset> _defaultAsset;
    Ref<Asset> _iOSAsset;
    Ref<Asset> _androidAsset;
};

} // namespace Valdi
