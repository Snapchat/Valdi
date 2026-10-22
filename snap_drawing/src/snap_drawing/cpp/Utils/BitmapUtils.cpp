//
//  BitmapUtils.cpp
//  snap_drawing-pc
//
//  Created by Simon Corsin on 1/24/22.
//

#include "snap_drawing/cpp/Utils/BitmapUtils.hpp"
#include "include/core/SkImageInfo.h"

#include <limits>

namespace snap::drawing {

static Valdi::ColorType toValdiColorType(SkColorType colorType) {
    switch (colorType) {
        case SkColorType::kUnknown_SkColorType:
            return Valdi::ColorTypeUnknown;
        case SkColorType::kRGBA_8888_SkColorType:
            return Valdi::ColorTypeRGBA8888;
        case SkColorType::kBGRA_8888_SkColorType:
            return Valdi::ColorTypeBGRA8888;
        case SkColorType::kAlpha_8_SkColorType:
            return Valdi::ColorTypeAlpha8;
        case SkColorType::kGray_8_SkColorType:
            return Valdi::ColorTypeGray8;
        case SkColorType::kRGBA_F16_SkColorType:
            return Valdi::ColorTypeRGBAF16;
        case SkColorType::kRGBA_F32_SkColorType:
            return Valdi::ColorTypeRGBAF32;
        default:
            return Valdi::ColorTypeUnknown;
    }
}

static Valdi::AlphaType toValdiAlphaType(SkAlphaType alphaType) {
    switch (alphaType) {
        case SkAlphaType::kOpaque_SkAlphaType:
            return Valdi::AlphaTypeOpaque;
        case SkAlphaType::kPremul_SkAlphaType:
            return Valdi::AlphaTypePremul;
        case SkAlphaType::kUnpremul_SkAlphaType:
            return Valdi::AlphaTypeUnpremul;
        default:
            return Valdi::AlphaTypeOpaque;
    }
}

Valdi::BitmapInfo toBitmapInfo(const SkImageInfo& info) {
    auto colorType = toValdiColorType(info.colorType());
    auto alphaType = toValdiAlphaType(info.alphaType());
    auto rowByteSize = info.minRowBytes();

    return Valdi::BitmapInfo(info.width(), info.height(), colorType, alphaType, rowByteSize);
}

SkColorType toSkiaColorType(Valdi::ColorType colorType) {
    switch (colorType) {
        case Valdi::ColorTypeUnknown:
            return SkColorType::kUnknown_SkColorType;
        case Valdi::ColorTypeRGBA8888:
            return SkColorType::kRGBA_8888_SkColorType;
        case Valdi::ColorTypeBGRA8888:
            return SkColorType::kBGRA_8888_SkColorType;
        case Valdi::ColorTypeAlpha8:
            return SkColorType::kAlpha_8_SkColorType;
        case Valdi::ColorTypeGray8:
            return SkColorType::kGray_8_SkColorType;
        case Valdi::ColorTypeRGBAF16:
            return SkColorType::kRGBA_F16_SkColorType;
        case Valdi::ColorTypeRGBAF32:
            return SkColorType::kRGBA_F32_SkColorType;
    }

    return SkColorType::kUnknown_SkColorType;
}

static SkAlphaType toSkiaAlphaType(Valdi::AlphaType alphaType) {
    switch (alphaType) {
        case Valdi::AlphaTypeOpaque:
            return SkAlphaType::kOpaque_SkAlphaType;
        case Valdi::AlphaTypePremul:
            return SkAlphaType::kPremul_SkAlphaType;
        case Valdi::AlphaTypeUnpremul:
            return SkAlphaType::kUnpremul_SkAlphaType;
    }

    return SkAlphaType::kOpaque_SkAlphaType;
}

SkImageInfo toSkiaImageInfo(const Valdi::BitmapInfo& info) {
    auto colorType = toSkiaColorType(info.colorType);
    auto alphaType = toSkiaAlphaType(info.alphaType);

    return SkImageInfo::Make(info.width, info.height, colorType, alphaType);
}

static void releaseBitmap(const void* /*ptr*/, void* context) {
    auto bitmap = Valdi::unsafeBridge<Valdi::IBitmap>(context);
    bitmap->unlockBytes();
    Valdi::unsafeBridgeRelease(context);
}

bool tryAllocBitmapPixels(SkBitmap& bitmap, const SkImageInfo& info, size_t rowBytes) {
    if (info.isEmpty() || rowBytes == info.minRowBytes()) {
        return bitmap.tryAllocPixels(info, rowBytes);
    }
    if (!info.validRowBytes(rowBytes)) {
        return false;
    }

    const auto height = static_cast<size_t>(info.height());
    if (rowBytes > std::numeric_limits<size_t>::max() / height) {
        return false;
    }
    auto data = SkData::MakeUninitialized(rowBytes * height);
    if (data == nullptr) {
        return false;
    }

    auto* pixels = data->writable_data();
    return bitmap.installPixels(
        info,
        pixels,
        rowBytes,
        [](void* /*addr*/, void* context) { static_cast<SkData*>(context)->unref(); },
        data.release());
}

Valdi::Result<sk_sp<SkData>> bitmapToData(const Valdi::Ref<Valdi::IBitmap>& bitmap,
                                          const Valdi::BitmapInfo& info,
                                          bool copy) {
    auto* bytes = bitmap->lockBytes();
    if (bytes == nullptr) {
        return Valdi::Error("Failed to lock bytes");
    }

    auto size = info.rowBytes * static_cast<size_t>(info.height);

    if (copy) {
        auto data = SkData::MakeWithCopy(bytes, size);
        bitmap->unlockBytes();
        return data;
    } else {
        return SkData::MakeWithProc(bytes, size, &releaseBitmap, Valdi::unsafeBridgeRetain(bitmap.get()));
    }
}

} // namespace snap::drawing
