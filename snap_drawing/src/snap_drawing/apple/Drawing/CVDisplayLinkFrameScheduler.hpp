//
//  CVDisplayLinkFrameScheduler.hpp
//  snap_drawing-pc
//
//  Created by Simon Corsin on 1/21/22.
//

#pragma once

#include "snap_drawing/apple/Drawing/BaseDisplayLinkFrameScheduler.hpp"

#include <CoreVideo/CVDisplayLink.h>

namespace snap::drawing {

class CVDisplayLinkFrameScheduler : public BaseDisplayLinkFrameScheduler {
public:
    explicit CVDisplayLinkFrameScheduler(Valdi::ILogger& logger);
    ~CVDisplayLinkFrameScheduler() override;

    void setActiveDisplay(CGDirectDisplayID displayId);

protected:
    void onResume(std::unique_lock<Valdi::Mutex>& lock) override;
    void onPause(std::unique_lock<Valdi::Mutex>& lock) override;

private:
    CGDirectDisplayID _activeDisplay = kCGNullDirectDisplay;
    CVDisplayLinkRef _displayLink = nullptr;

    void destroyDisplayLink(std::unique_lock<Valdi::Mutex>& guard);
    void createDisplayLink(std::unique_lock<Valdi::Mutex>& lock);

    static CVReturn displayLinkCallback(CVDisplayLinkRef displayLink,
                                        const CVTimeStamp* inNow,
                                        const CVTimeStamp* inOutputTime,
                                        CVOptionFlags flagsIn,
                                        CVOptionFlags* flagsOut,
                                        void* displayLinkContext);
};

} // namespace snap::drawing
