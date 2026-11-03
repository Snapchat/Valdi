//
//  DeviceModule.hpp
//  valdi-skia
//
//  Created by Simon Corsin on 6/28/20.
//

#pragma once

#include "valdi_core/ModuleFactory.hpp"
#include "valdi_core/cpp/Utils/Mutex.hpp"
#include "valdi_core/cpp/Utils/Shared.hpp"
#include "valdi_core/cpp/Utils/ValueFunction.hpp"

#include <map>

namespace Valdi {

class DeviceModule : public Valdi::SharedPtrRefCountable, public snap::valdi_core::ModuleFactory {
public:
    DeviceModule();
    ~DeviceModule() override;

    Valdi::StringBox getModulePath() override;
    Valdi::Value loadModule() override;

    void setDisplaySize(double width, double height, double scale);
    void setWindowSize(double windowWidth, double windowHeight);

    double getDisplayWidth();
    double getDisplayHeight();
    double getDisplayScale();
    double getDynamicTypeScale();

    double getWindowWidth();
    double getWindowHeight();

    double getDisplayLeftInset();
    double getDisplayRightInset();
    double getDisplayBottomInset();
    double getDisplayTopInset();

    Valdi::StringBox getSystemType();

    Valdi::Value getDeviceLocales();

    Valdi::Value performHapticFeedback();

    bool isDesktop();

    // Hosts supply appearance; the standalone runtime does not discover a desktop.
    // Unconfigured/headless hosts default to light. Repeated values are ignored.
    // Hosts must use ValdiStandaloneRuntime::setDarkMode(), which serializes updates
    // on the JS queue. This bridge entry point invokes observers while holding
    // _darkModeMutex and must not introduce synchronous cross-thread dependencies.
    void setDarkMode(bool darkMode);
    Valdi::Value observeDarkMode(const Valdi::ValueFunctionCallContext& callContext);
    Valdi::Value observeDisplayInsetChange();

    Valdi::Value writeFile(const Valdi::ValueFunctionCallContext& callContext);
    Valdi::Value readFileData(const Valdi::ValueFunctionCallContext& callContext);
    Valdi::Value readFileString(const Valdi::ValueFunctionCallContext& callContext);
    Valdi::Value readDirectory(const Valdi::ValueFunctionCallContext& callContext);

private:
    mutable Valdi::Mutex _mutex;

    // Serialize registration and notification order across host and JS threads.
    // Native observers can cancel/reenter; JS observers are never called synchronously
    // across threads while this lock is held.
    Valdi::RecursiveMutex _darkModeMutex;
    bool _darkMode = false;
    uint64_t _darkModeRevision = 0;
    uint64_t _nextDarkModeObserverId = 0;
    std::map<uint64_t, Valdi::Ref<Valdi::ValueFunction>> _darkModeObservers;

    double _displayWidth = 0.0;
    double _displayHeight = 0.0;
    double _displayScale = 1.0;

    double _windowWidth = 0.0;
    double _windowHeight = 0.0;
};

} // namespace Valdi
