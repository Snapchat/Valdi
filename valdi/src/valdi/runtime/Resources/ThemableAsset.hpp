//
//  ThemableAsset.hpp
//  valdi
//

#pragma once

#include "valdi_core/cpp/Resources/Asset.hpp"
#include "valdi_core/cpp/Utils/FlatMap.hpp"
#include "valdi_core/cpp/Utils/StringBox.hpp"

namespace Valdi {

class ColorPalette;

/**
 A ThemableAsset holds assets keyed by color palette name and resolves to the
 asset matching the view node's resolved color palette.
 */
class ThemableAsset : public Asset {
public:
    explicit ThemableAsset(FlatMap<StringBox, Ref<Asset>> assetsByColorPalette);
    ~ThemableAsset() override;

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
    FlatMap<StringBox, Ref<Asset>> _assetsByColorPalette;
    Ref<Asset> _representativeAsset;
};

} // namespace Valdi
