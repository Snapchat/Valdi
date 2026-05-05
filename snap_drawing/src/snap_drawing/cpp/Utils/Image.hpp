//
//  Image.hpp
//  valdi-desktop-apple
//
//  Created by Simon Corsin on 7/7/20.
//

#pragma once

#include "valdi_core/cpp/Attributes/ImageFilter.hpp"
#include "valdi_core/cpp/Interfaces/IBitmap.hpp"
#include "valdi_core/cpp/Resources/LoadedAsset.hpp"
#include "valdi_core/cpp/Utils/Bytes.hpp"
#include "valdi_core/cpp/Utils/Result.hpp"
#include "valdi_core/cpp/Utils/ValdiObject.hpp"

#include "snap_drawing/cpp/Utils/Aliases.hpp"

#include "include/core/SkImage.h"

namespace snap::drawing {

enum EncodedImageFormat { EncodedImageFormatJPG, EncodedImageFormatPNG, EncodedImageFormatWebP };

class Image : public Valdi::LoadedAsset {
public:
    explicit Image(const sk_sp<SkImage>& skImage);

    ~Image() override;

    int width() const;
    int height() const;

    Valdi::Value getMetadata() const override;

    static void initializeCodecs();

    /**
     Resize the image to the given width/height.
     This will read the entire bitmap of the Image and scale it
     in software, this is an expensive operation.
     */
    Ref<Image> resized(int width, int height) const;

    /**
     Draw the image into the given bytes buffer conforming to the given bitmap info,
     scaling pixels if necessary.
     */
    void draw(const Valdi::BitmapInfo& bitmapInfo, void* bytes);

    /**
     Returns a new Image which will be displayed with the given filter when drawn
     */
    Ref<Image> withFilter(const Ref<Valdi::ImageFilter>& filter);

    const Ref<Valdi::ImageFilter>& getFilter() const;

    Valdi::Result<Valdi::BytesView> toPNG() const;

    Valdi::Result<Valdi::BytesView> encode(EncodedImageFormat format, double qualityRatio) const;

    const sk_sp<SkImage>& getSkValue() const;

    Valdi::Ref<Valdi::IBitmap> getBitmap();

    /**
     Make an Image from bytes representing an encoded image, like in PNG or JPG format.
     */
    static Valdi::Result<Ref<Image>> make(const Valdi::BytesView& data);

    /**
     Make an Image with the raw pixels data in the format specified in the BitmapInfo.
     If shouldCopy is false, the returned Image will use the bytes from the attached BytesView
     and it will retain its source if it has one.
     Otherwise, the bytes will be copied.
     */
    static Valdi::Result<Ref<Image>> makeFromPixelsData(const Valdi::BitmapInfo& bitmapInfo,
                                                        const Valdi::BytesView& pixelsData,
                                                        bool shouldCopy);

    /**
     Make an Image with the raw pixels data provided from the given Bitmap.
     If shouldCopy is false, the returned Image will retain the Bitmap and use its underlying
     buffer directly. Otherwise, the bytes will be copied.
     */
    static Valdi::Result<Ref<Image>> makeFromBitmap(const Valdi::Ref<Valdi::IBitmap>& bitmap, bool shouldCopy);

    static sk_sp<SkData> encodeSKImageToSKData(SkImage& image, EncodedImageFormat format, int quality);

    VALDI_CLASS_HEADER(Image)

private:
    sk_sp<SkImage> _skImage;
    Ref<Image> _sourceImage;
    Ref<Valdi::ImageFilter> _filter;

    static Valdi::Result<Ref<Image>> makeFromPixelsData(const Valdi::BitmapInfo& bitmapInfo,
                                                        const sk_sp<SkData>& pixelsData);
};

} // namespace snap::drawing
