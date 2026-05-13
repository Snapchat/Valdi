//
//  MacOSSnapDrawingRuntime.cpp
//  valdi-desktop-apple
//
//  Created by Simon Corsin on 7/27/22.
//

#import "valdi/macos/MacOSSnapDrawingRuntime.h"
#import <Metal/Metal.h>
#import "snap_drawing/cpp/Drawing/GraphicsContext/MetalGraphicsContext.hpp"
#import "valdi/snap_drawing/Graphics/ShaderCache.hpp"
#import "valdi/snap_drawing/Layers/Classes/LayerClass.hpp"
#import "valdi/snap_drawing/Layers/Classes/LayoutLayerClass.hpp"
#import "snap_drawing/cpp/Touches/GesturesConfiguration.hpp"

namespace ValdiMacOS {

MacOSSnapDrawingRuntime::MacOSSnapDrawingRuntime(const Valdi::Ref<Valdi::IDiskCache>& diskCache,
                                                 Valdi::IViewManager& hostViewManager,
                                                 Valdi::ILogger& logger,
                                                 const Valdi::Ref<Valdi::DispatchQueue>& workerQueue,
                                                 uint64_t maxCacheSizeInBytes):
snap::drawing::Runtime(Valdi::makeShared<snap::drawing::CVDisplayLinkFrameScheduler>(logger), snap::drawing::GesturesConfiguration::getDefault(), diskCache, &hostViewManager, logger, workerQueue, maxCacheSizeInBytes) {

    initializeViewManager(Valdi::PlatformTypeMacOS);

    id<MTLDevice> device = MTLCreateSystemDefaultDevice();
    id<MTLCommandQueue> commandQueue = [device newCommandQueue];

    auto options = Valdi::makeShared<snap::drawing::GrGraphicsContextOptions>(getShaderCache());

    setGraphicsContext(Valdi::makeShared<snap::drawing::MetalGraphicsContext>((__bridge CFTypeRef)device,
                                                                                 (__bridge CFTypeRef)commandQueue,
                                                                                 options));
}

MacOSSnapDrawingRuntime::~MacOSSnapDrawingRuntime() = default;

void MacOSSnapDrawingRuntime::initializeViewManager(Valdi::PlatformType platformType) {
    snap::drawing::Runtime::initializeViewManager(platformType);
    auto viewManager = getViewManager();
    auto baseLayerClass = Valdi::castOrNull<snap::drawing::LayerClass>(viewManager->getBaseLayerClass());
    if (baseLayerClass != nullptr) {
        viewManager->registerLayerClass(Valdi::makeShared<snap::drawing::LayoutLayerClass>(getResources(), baseLayerClass));
    }
}

void MacOSSnapDrawingRuntime::setActiveDisplay(CGDirectDisplayID displayId) {
    dynamic_cast<snap::drawing::CVDisplayLinkFrameScheduler &>(*getFrameScheduler()).setActiveDisplay(displayId);
}

snap::drawing::Ref<snap::drawing::MetalGraphicsContext> MacOSSnapDrawingRuntime::getMetalGraphicsContext() const {
    return Valdi::castOrNull<snap::drawing::MetalGraphicsContext>(getGraphicsContext());
}

}
