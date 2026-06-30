//
//  View.hpp
//  ValdiRuntime
//
//  Created by Simon Corsin on 6/26/18.
//  Copyright © 2018 Snap Inc. All rights reserved.
//

#pragma once

#include "valdi_core/cpp/Utils/Bytes.hpp"
#include "valdi_core/cpp/Utils/Function.hpp"
#include "valdi_core/cpp/Utils/Result.hpp"
#include "valdi_core/cpp/Utils/StringBox.hpp"
#include "valdi_core/cpp/Utils/ValdiObject.hpp"
#include "valdi_core/cpp/Utils/ValueConvertible.hpp"

#include "valdi/runtime/Attributes/Animator.hpp"
#include "valdi_core/cpp/Views/Frame.hpp"
#include "valdi_core/cpp/Views/Measure.hpp"

#include "valdi_core/Platform.hpp"

#include <memory>
#include <vector>

namespace snap::drawing {
struct ExternalSurfacePresenterState;
}

namespace Valdi {

struct ViewTrace {
    StringBox bundleName;
    StringBox documentPath;
    StringBox nodeId;

    ViewTrace(StringBox bundleName, StringBox documentPath, StringBox nodeId);
};

class ViewNodeTree;
class ViewNode;
class LoadedAsset;
class IBitmap;

enum ViewHitTestResult {
    ViewHitTestResultUseDefault = 0,
    ViewHitTestResultHit = 1,
    ViewHitTestResultNotHit = 2,
};

class View : public ValdiObject {
public:
    View();
    ~View() override;

    virtual bool onTouchEvent(long offsetSinceSourceMs,
                              int touchEventType,
                              float absoluteX,
                              float absoluteY,
                              float x,
                              float y,
                              float dx,
                              float dy,
                              bool isTouchOwner,
                              const Ref<RefCountable>& source);

    virtual ViewHitTestResult hitTest(float x, float y);

    virtual Result<Void> rasterInto(const Ref<IBitmap>& bitmap,
                                    const Frame& frame,
                                    const Matrix& transform,
                                    float rasterScaleX,
                                    float rasterScaleY);

    virtual snap::valdi_core::Platform getPlatform() const = 0;

    bool canBeReused() const;

    void setCanBeReused(bool viewCanBeReused);

private:
    bool _canBeReused = true;
};

} // namespace Valdi
