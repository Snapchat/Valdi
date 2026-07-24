//
//  Asset.hpp
//  valdi-ios
//
//  Created by Simon Corsin on 9/25/19.
//

#pragma once

#include "valdi_core/Asset.hpp"
#include "valdi_core/cpp/Attributes/ColorPalette.hpp"
#include "valdi_core/cpp/Context/PlatformType.hpp"
#include "valdi_core/cpp/Resources/AssetLocation.hpp"
#include "valdi_core/cpp/Utils/ValdiObject.hpp"
#include <optional>

namespace Valdi {

struct AssetConfiguration {
    AssetConfiguration(Ref<ColorPalette> colorPalette,
                       std::optional<PlatformType> platformType,
                       std::optional<bool> rightToLeft);

    Ref<ColorPalette> colorPalette;
    std::optional<PlatformType> platformType;
    std::optional<bool> rightToLeft;
};

class Asset : public ValdiObject, public snap::valdi_core::Asset {
public:
    Asset();
    ~Asset() override;

    /**
     Return the resolved location for where this Asset is stored.
     */
    virtual std::optional<AssetLocation> getResolvedLocation() const = 0;

    virtual bool canBeMeasured() const = 0;

    virtual Ref<Asset> withConfiguration(const AssetConfiguration& configuration);

    virtual double getWidth() const = 0;
    virtual double getHeight() const = 0;

    std::pair<double, double> measure(double maxWidth, double maxHeight) const;

    double measureWidth(double maxWidth, double maxHeight) final;

    double measureHeight(double maxWidth, double maxHeight) final;

    bool isURL() const;

    bool isLocalResource() const;

    bool needResolve() const;

    StringBox getResolvedURL() const;

    VALDI_CLASS_HEADER(Asset)
};

} // namespace Valdi
