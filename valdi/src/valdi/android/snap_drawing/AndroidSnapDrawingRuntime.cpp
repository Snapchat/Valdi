//
//  AndroidSnapDrawingRuntime.cpp
//  valdi-android
//
//  Created by Simon Corsin on 7/23/20.
//

#include "valdi/android/snap_drawing/AndroidSnapDrawingRuntime.hpp"
#include "valdi/android/snap_drawing/AndroidScrollConstantsResolver.hpp"
#include "valdi_core/cpp/Utils/LoggerUtils.hpp"
#include "valdi_core/jni/JNIMethodUtils.hpp"

#include "snap_drawing/cpp/Drawing/GraphicsContext/ANativeWindowGraphicsContext.hpp"
#include "snap_drawing/cpp/Layers/Scroll/SplineScrollPhysics.hpp"

#include "valdi/android/ViewManager.hpp"
#include "valdi/snap_drawing/Graphics/ShaderCache.hpp"

namespace ValdiAndroid {

AndroidSnapDrawingRuntime::AndroidSnapDrawingRuntime(const GlobalRefJavaObjectBase& frameSchedulerJava,
                                                     const snap::drawing::GesturesConfiguration& gesturesConfiguration,
                                                     const Valdi::Ref<Valdi::IDiskCache>& diskCache,
                                                     ViewManager& androidViewManager,
                                                     Valdi::ILogger& logger,
                                                     const Valdi::Ref<Valdi::DispatchQueue>& workerQueue,
                                                     uint64_t maxCacheSizeInBytes)
    : snap::drawing::Runtime(Valdi::makeShared<snap::drawing::AndroidFrameScheduler>(frameSchedulerJava),
                             gesturesConfiguration,
                             diskCache,
                             &androidViewManager,
                             logger,
                             workerQueue,
                             maxCacheSizeInBytes),
      _androidViewManager(androidViewManager) {
    auto scrollConstants = ScrollConstants::resolve(diskCache, logger);

    snap::drawing::SplineScrollPhysics::initialize(scrollConstants.gravity,
                                                   scrollConstants.inflexion,
                                                   scrollConstants.startTension,
                                                   scrollConstants.endTension,
                                                   /* physicalCoeff */ 0,
                                                   scrollConstants.decelerationRate);

    initializeViewManager(Valdi::PlatformTypeAndroid);
}

AndroidSnapDrawingRuntime::~AndroidSnapDrawingRuntime() = default;

Valdi::Ref<snap::drawing::ANativeWindowGraphicsContext> AndroidSnapDrawingRuntime::getNativeWindowGraphicsContext() {
    std::lock_guard<Valdi::Mutex> guard(_mutex);

    auto graphicsContext = Valdi::castOrNull<snap::drawing::ANativeWindowGraphicsContext>(getGraphicsContext());
    if (graphicsContext == nullptr) {
        auto options = Valdi::makeShared<snap::drawing::GrGraphicsContextOptions>(getShaderCache());
        graphicsContext = snap::drawing::ANativeWindowGraphicsContext::create(snap::drawing::DisplayParams(), options);
        setGraphicsContext(graphicsContext);
    }

    return graphicsContext;
}

Valdi::Ref<ValdiAndroid::SnapDrawingLayerRootHost> AndroidSnapDrawingRuntime::createHost(bool disallowSynchronousDraw) {
    auto rootHost = Valdi::makeShared<ValdiAndroid::SnapDrawingLayerRootHost>(
        getDrawLooper(), getResources(), getNativeWindowGraphicsContext(), _androidViewManager);
    rootHost->setDisallowSynchronousDraw(disallowSynchronousDraw);
    return rootHost;
}

int64_t AndroidSnapDrawingRuntime::getMaxRenderTargetSize() {
    return getNativeWindowGraphicsContext()->getMaxRenderTargetSize();
}

} // namespace ValdiAndroid
