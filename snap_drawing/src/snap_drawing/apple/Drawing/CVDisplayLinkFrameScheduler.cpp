//
//  CVDisplayLinkFrameScheduler.cpp
//  snap_drawing-pc
//
//  Created by Simon Corsin on 1/21/22.
//

#include "utils/platform/TargetPlatform.hpp"

#if __APPLE__ && !SC_IOS
#include "snap_drawing/apple/Drawing/CVDisplayLinkFrameScheduler.hpp"
#include "valdi_core/cpp/Utils/LoggerUtils.hpp"

extern "C" {
void* objc_autoreleasePoolPush(void);
void objc_autoreleasePoolPop(void* pool);
}

namespace snap::drawing {

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

CVDisplayLinkFrameScheduler::CVDisplayLinkFrameScheduler(Valdi::ILogger& logger)
    : BaseDisplayLinkFrameScheduler(logger) {}

CVDisplayLinkFrameScheduler::~CVDisplayLinkFrameScheduler() {
    auto guard = lock();
    destroyDisplayLink(guard);
}

void CVDisplayLinkFrameScheduler::setActiveDisplay(CGDirectDisplayID displayId) {
    auto guard = lock();

    if (displayId != _activeDisplay) {
        destroyDisplayLink(guard);
        // Re-acquire if destroyDisplayLink released the lock
        if (!guard.owns_lock()) {
            guard.lock();
        }
        // Re-check after re-acquiring — another thread may have changed _activeDisplay
        // while the lock was released during destroyDisplayLink.
        if (displayId == _activeDisplay) {
            return;
        }
        _activeDisplay = displayId;
        createDisplayLink(guard);
        onDisplayLinkChanged(guard);
    }
}

void CVDisplayLinkFrameScheduler::destroyDisplayLink(std::unique_lock<Valdi::Mutex>& guard) {
    if (_activeDisplay != kCGNullDirectDisplay) {
        // Capture and nil the display link while still holding the lock,
        // then unlock before stopping. CVDisplayLinkStop waits for any
        // in-flight callback to finish, and that callback acquires this
        // same lock — so holding it during stop would deadlock.
        auto displayLink = _displayLink;
        _displayLink = nil;
        _activeDisplay = kCGNullDirectDisplay;

        if (displayLink) {
            guard.unlock();
            CVDisplayLinkStop(displayLink);
            CVDisplayLinkRelease(displayLink);
        }
    }
}

void CVDisplayLinkFrameScheduler::createDisplayLink(std::unique_lock<Valdi::Mutex>& lock) {
    if (_activeDisplay == kCGNullDirectDisplay) {
        return;
    }

    auto code = CVDisplayLinkCreateWithCGDisplay(_activeDisplay, &_displayLink);
    if (code != kCVReturnSuccess) {
        VALDI_ERROR(getLogger(), "Failed to create DisplayLink: {}", code);
        return;
    }

    CVDisplayLinkSetOutputCallback(_displayLink, &CVDisplayLinkFrameScheduler::displayLinkCallback, this);
}

void CVDisplayLinkFrameScheduler::onResume(std::unique_lock<Valdi::Mutex>& lock) {
    auto displayLink = _displayLink;

    if (displayLink) {
        lock.unlock();
        CVDisplayLinkStart(displayLink);
    }
}

void CVDisplayLinkFrameScheduler::onPause(std::unique_lock<Valdi::Mutex>& lock) {
    auto displayLink = _displayLink;

    if (displayLink) {
        lock.unlock();
        CVDisplayLinkStop(displayLink);
    }
}

CVReturn CVDisplayLinkFrameScheduler::displayLinkCallback(CVDisplayLinkRef displayLink,
                                                          const CVTimeStamp* inNow,
                                                          const CVTimeStamp* inOutputTime,
                                                          CVOptionFlags flagsIn,
                                                          CVOptionFlags* flagsOut,
                                                          void* displayLinkContext) {
    auto pool = objc_autoreleasePoolPush();

    auto frameScheduler = Valdi::strongSmallRef(reinterpret_cast<CVDisplayLinkFrameScheduler*>(displayLinkContext));

    frameScheduler->onVSync();

    objc_autoreleasePoolPop(pool);

    return kCVReturnSuccess;
}

#pragma clang diagnostic pop

} // namespace snap::drawing

#endif
