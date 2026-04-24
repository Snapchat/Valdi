#pragma once

#include "snap_drawing/cpp/Utils/Aliases.hpp"
#include "snap_drawing/cpp/Utils/AnimatedImage.hpp"
#include "snap_drawing/cpp/Utils/Duration.hpp"
#include "snap_drawing/cpp/Utils/Geometry.hpp"
#include "valdi_core/cpp/Utils/Mutex.hpp"
#include "valdi_core/cpp/Utils/Result.hpp"

#include "modules/svg/include/SkSVGDOM.h"

namespace snap::drawing {

class SVGAnimatedImage : public AnimatedImage {
public:
    explicit SVGAnimatedImage(const sk_sp<SkSVGDOM>& dom);
    ~SVGAnimatedImage() override;

    Duration getCurrentTime() const override;
    const Duration& getDuration() const override;
    const Size& getSize() const override;
    double getFrameRate() const override;
    Valdi::Value getMetadata() const override;

    static Valdi::Result<Ref<SVGAnimatedImage>> make(const Valdi::Byte* data, size_t length);

    VALDI_CLASS_HEADER(SVGAnimatedImage)

protected:
    void doDraw(SkCanvas* canvas,
                const Rect& drawBounds,
                const Duration& time,
                FittingSizeMode fittingSizeMode) override;

private:
    mutable Valdi::Mutex _mutex;
    sk_sp<SkSVGDOM> _dom;
    Duration _duration;
    Duration _currentTime;
    Size _size;
};

} // namespace snap::drawing
