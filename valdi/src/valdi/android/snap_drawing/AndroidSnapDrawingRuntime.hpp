//
//  AndroidSnapDrawingRuntime.hpp
//  valdi-android
//
//  Created by Simon Corsin on 7/23/20.
//

#pragma once

#include "valdi/runtime/Views/View.hpp"

#include "valdi/snap_drawing/Runtime.hpp"

#include "valdi_core/cpp/Utils/Mutex.hpp"

#include "snap_drawing/android/AndroidFrameScheduler.hpp"

#include "valdi/android/snap_drawing/SnapDrawingLayerRootHost.hpp"

namespace snap::drawing {
class ANativeWindowGraphicsContext;
};

namespace ValdiAndroid {

class ViewManager;

class AndroidSnapDrawingRuntime : public snap::drawing::Runtime {
public:
    AndroidSnapDrawingRuntime(const GlobalRefJavaObjectBase& frameSchedulerJava,
                              const snap::drawing::GesturesConfiguration& gesturesConfiguration,
                              const Valdi::Ref<Valdi::IDiskCache>& diskCache,
                              ViewManager& androidViewManager,
                              Valdi::ILogger& logger,
                              const Valdi::Ref<Valdi::DispatchQueue>& workerQueue,
                              uint64_t maxCacheSizeInBytes);
    ~AndroidSnapDrawingRuntime() override;

    Valdi::Ref<ValdiAndroid::SnapDrawingLayerRootHost> createHost(bool disallowSynchronousDraw);

    int64_t getMaxRenderTargetSize();

private:
    Valdi::Mutex _mutex;
    ViewManager& _androidViewManager;

    Valdi::Ref<snap::drawing::ANativeWindowGraphicsContext> getNativeWindowGraphicsContext();
};

} // namespace ValdiAndroid
