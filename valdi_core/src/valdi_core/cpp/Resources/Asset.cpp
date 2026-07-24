//
//  Asset.cpp
//  valdi-ios
//
//  Created by Simon Corsin on 9/25/19.
//

#include "valdi_core/cpp/Resources/Asset.hpp"

namespace Valdi {

Asset::Asset() = default;

Asset::~Asset() = default;

AssetConfiguration::AssetConfiguration(Ref<ColorPalette> colorPalette,
                                       std::optional<PlatformType> platformType,
                                       std::optional<bool> rightToLeft)
    : colorPalette(std::move(colorPalette)), platformType(platformType), rightToLeft(rightToLeft) {}

bool Asset::needResolve() const {
    return !getResolvedLocation().has_value();
}

bool Asset::isURL() const {
    auto resolvedLocation = getResolvedLocation();
    if (!resolvedLocation) {
        return false;
    }

    return !resolvedLocation.value().isLocal();
}

bool Asset::isLocalResource() const {
    auto resolvedLocation = getResolvedLocation();
    if (!resolvedLocation) {
        return false;
    }

    return resolvedLocation.value().isLocal();
}

StringBox Asset::getResolvedURL() const {
    auto resolvedLocation = getResolvedLocation();
    if (!resolvedLocation) {
        return StringBox();
    }

    return resolvedLocation.value().getUrl();
}

Ref<Asset> Asset::withConfiguration(const AssetConfiguration& /*configuration*/) {
    return strongSmallRef(this);
}

double Asset::measureWidth(double maxWidth, double maxHeight) {
    return measure(maxWidth, maxHeight).first;
}

double Asset::measureHeight(double maxWidth, double maxHeight) {
    return measure(maxWidth, maxHeight).second;
}

std::pair<double, double> Asset::measure(double maxWidth, double maxHeight) const {
    if (!canBeMeasured()) {
        return std::make_pair(0.0, 0.0);
    }

    auto widthD = getWidth();
    auto heightD = getHeight();

    auto targetWidth = maxWidth;
    if (targetWidth < 0.0) {
        targetWidth = widthD;
    }

    auto targetHeight = maxHeight;
    if (targetHeight < 0.0) {
        targetHeight = heightD;
    }

    if (widthD <= targetWidth && heightD <= targetHeight) {
        // The asset fits within our constraints, use the natural size
        // of our asset.
        return std::make_pair(widthD, heightD);
    }

    // The asset doesn't fit, scale it down

    auto wRatio = widthD / targetWidth;
    auto hRatio = heightD / targetHeight;

    auto ratio = std::max(wRatio, hRatio);

    return std::make_pair(widthD / ratio, heightD / ratio);
}

VALDI_CLASS_IMPL(Asset)

} // namespace Valdi
