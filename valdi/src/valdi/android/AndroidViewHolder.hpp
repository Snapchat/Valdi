//
//  AndroidViewHolder.hpp
//  valdi-android
//
//  Created by Simon Corsin on 4/26/21.
//

#pragma once

#include "valdi/runtime/Views/Measure.hpp"
#include "valdi/runtime/Views/View.hpp"
#include "valdi_core/jni/GlobalRefJavaObject.hpp"

namespace Valdi {
class IViewManager;
}

namespace ValdiAndroid {

class DeferredViewOperations;
class ViewManager;

class AndroidViewHolder : public Valdi::View, public GlobalRefJavaObjectBase {
public:
    explicit AndroidViewHolder(const JavaObject& object, ViewManager& viewManager);
    ~AndroidViewHolder() override;

    bool onTouchEvent(long offsetSinceSourceMs,
                      int touchEventType,
                      float absoluteX,
                      float absoluteY,
                      float x,
                      float y,
                      float dx,
                      float dy,
                      bool isTouchOwner,
                      const Valdi::Ref<Valdi::RefCountable>& source) override;

    Valdi::ViewHitTestResult hitTest(float x, float y) override;

    Valdi::Result<Valdi::Void> rasterInto(const Valdi::Ref<Valdi::IBitmap>& bitmap,
                                          const Valdi::Frame& frame,
                                          const Valdi::Matrix& transform,
                                          float rasterScaleX,
                                          float rasterScaleY) override;

    snap::valdi_core::Platform getPlatform() const override;

    VALDI_CLASS_HEADER(AndroidViewHolder)

    bool isRecyclable();
    int32_t convertPointsToPixels(float point) const;

private:
    ViewManager& _viewManager;
    std::optional<bool> _isRecyclable;
};

ViewType fromValdiView(const Valdi::Ref<Valdi::View>& view);
Valdi::Ref<Valdi::View> toValdiView(const ViewType& viewType, ViewManager& viewManager);

} // namespace ValdiAndroid
