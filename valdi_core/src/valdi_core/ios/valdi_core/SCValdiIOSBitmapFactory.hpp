#pragma once

#ifdef __cplusplus

#import "valdi_core/SCValdiImage.h"
#import "valdi_core/SCValdiObjCConversionUtils.h"
#include "valdi_core/cpp/Interfaces/IBitmap.hpp"
#include "valdi_core/cpp/Interfaces/IBitmapFactory.hpp"
#include "valdi_core/cpp/Utils/Result.hpp"

namespace ValdiIOS {

const Valdi::Ref<Valdi::IBitmapFactory>& getIOSBitmapFactory();

Valdi::Result<ObjCObjectDirectRef> imageFromBitmap(const Valdi::Ref<Valdi::IBitmap>& bitmap);

} // namespace ValdiIOS

#endif
