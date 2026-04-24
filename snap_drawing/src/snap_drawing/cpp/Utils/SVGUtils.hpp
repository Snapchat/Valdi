#pragma once

#include "include/core/SkRefCnt.h"
#include "valdi_core/cpp/Utils/Bytes.hpp"
#include "valdi_core/cpp/Utils/Result.hpp"

class SkSVGDOM;

namespace snap::drawing {

Valdi::Result<sk_sp<SkSVGDOM>> makeSVGDOM(const Valdi::BytesView& data);

} // namespace snap::drawing
