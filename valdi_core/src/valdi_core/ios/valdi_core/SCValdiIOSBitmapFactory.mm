#import "valdi_core/SCValdiIOSBitmapFactory.hpp"

#import "valdi_core/SCValdiBitmap.h"
#include "valdi_core/cpp/Utils/Shared.hpp"

#include <cstdlib>

namespace ValdiIOS {

class IOSBitmap : public Valdi::IBitmap {
public:
    explicit IOSBitmap(const Valdi::BitmapInfo& info) : _info(info) {
        _bytes = std::malloc(_info.bytesLength());
    }

    ~IOSBitmap() override {
        dispose();
    }

    void dispose() override {
        if (_bytes != nullptr) {
            std::free(_bytes);
            _bytes = nullptr;
        }
    }

    Valdi::BitmapInfo getInfo() const override {
        return _info;
    }

    void* lockBytes() override {
        return _bytes;
    }

    void unlockBytes() override {}

private:
    Valdi::BitmapInfo _info;
    void* _bytes = nullptr;
};

class IOSBitmapFactory : public Valdi::IBitmapFactory {
public:
    Valdi::Result<Valdi::Ref<Valdi::IBitmap>> createBitmap(int width, int height) override {
        if (width <= 0 || height <= 0) {
            return Valdi::Error("Invalid iOS bitmap size");
        }

        auto info = Valdi::BitmapInfo(
            width, height, Valdi::ColorType::ColorTypeBGRA8888, Valdi::AlphaType::AlphaTypePremul, width * 4);
        auto bitmap = Valdi::makeShared<IOSBitmap>(info);
        if (bitmap->lockBytes() == nullptr) {
            return Valdi::Error("Failed to allocate iOS bitmap");
        }

        return Valdi::Ref<Valdi::IBitmap>(bitmap);
    }
};

const Valdi::Ref<Valdi::IBitmapFactory>& getIOSBitmapFactory() {
    static const Valdi::Ref<Valdi::IBitmapFactory> kInstance = Valdi::makeShared<IOSBitmapFactory>();
    return kInstance;
}

static void releaseBitmapProviderData(void* info, const void* /*data*/, size_t /*size*/) {
    auto bitmap = Valdi::unsafeBridgeTransfer<Valdi::IBitmap>(info);
    bitmap->unlockBytes();
}

Valdi::Result<ObjCObjectDirectRef> imageFromBitmap(const Valdi::Ref<Valdi::IBitmap>& bitmap) {
    if (bitmap == nullptr) {
        return Valdi::Error("Cannot create iOS image from a null bitmap");
    }

    auto bitmapInfo = bitmap->getInfo();
    if (bitmapInfo.colorType != Valdi::ColorType::ColorTypeBGRA8888 &&
        bitmapInfo.colorType != Valdi::ColorType::ColorTypeRGBA8888) {
        return Valdi::Error("Unsupported iOS bitmap color type");
    }

    auto* bytes = bitmap->lockBytes();
    if (bytes == nullptr) {
        return Valdi::Error("Failed to lock iOS bitmap bytes");
    }

    auto* retainedBitmap = Valdi::unsafeBridgeRetain(bitmap.get());
    CGDataProviderRef dataProvider =
        CGDataProviderCreateWithData(retainedBitmap, bytes, bitmapInfo.bytesLength(), releaseBitmapProviderData);
    if (dataProvider == nullptr) {
        bitmap->unlockBytes();
        Valdi::unsafeBridgeRelease(retainedBitmap);
        return Valdi::Error("Failed to create iOS image data provider");
    }

    CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
    if (colorSpace == nullptr) {
        CGDataProviderRelease(dataProvider);
        return Valdi::Error("Failed to create iOS image color space");
    }

    auto bitsPerPixel = Valdi::BitmapInfo::bytesPerPixelForColorType(bitmapInfo.colorType) * 8;
    CGImageRef cgImage = CGImageCreate(bitmapInfo.width,
                                       bitmapInfo.height,
                                       8,
                                       bitsPerPixel,
                                       bitmapInfo.rowBytes,
                                       colorSpace,
                                       CGBitmapInfoFromValdiBitmapInfoCpp(bitmapInfo),
                                       dataProvider,
                                       nullptr,
                                       false,
                                       kCGRenderingIntentDefault);
    CGColorSpaceRelease(colorSpace);
    CGDataProviderRelease(dataProvider);

    if (cgImage == nullptr) {
        return Valdi::Error("Failed to create iOS CGImage");
    }

    UIImage* uiImage = [UIImage imageWithCGImage:cgImage];
    CGImageRelease(cgImage);

    if (uiImage == nil) {
        return Valdi::Error("Failed to create iOS UIImage");
    }

    SCValdiImage* image = [SCValdiImage imageWithUIImage:uiImage];
    if (image == nil) {
        return Valdi::Error("Failed to create iOS Valdi image");
    }

    return ObjCObjectDirectRef(image);
}

} // namespace ValdiIOS
