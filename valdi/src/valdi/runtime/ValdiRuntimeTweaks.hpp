//
//  ValdiRuntimeTweaks.hpp
//  valdi
//
//  Created by Simon Corsin on 3/1/22.
//

#pragma once

#include "valdi/runtime/Interfaces/ITweakValueProvider.hpp"
#include "valdi_core/cpp/Context/PlatformType.hpp"
#include "valdi_core/cpp/Utils/Shared.hpp"
#include "valdi_core/cpp/Utils/Value.hpp"

namespace Valdi {

class ValdiRuntimeTweaks : public SimpleRefCountable {
public:
    ValdiRuntimeTweaks(const Shared<ITweakValueProvider>& tweakValueProvider);
    ~ValdiRuntimeTweaks() override;

    bool enableAccessibility() const;
    bool enableDeferredGC() const;
    bool enableCommonJsModuleLoader() const;
    bool disableHotReloaderLazyDenylist() const;
    bool disableSyncCallsInCallingThread() const;
    bool enableTSN() const;
    bool enableTSNForModule(const StringBox& moduleName) const;
    bool shouldCrashOnANR() const;
    bool disableAnimationRemoveOnCompleteIos() const;
    bool shouldNudgeJSThread() const;
    bool disablePersistentStoreEncryption() const;
    bool enableScopedContextStackTraceCapture() const;
    bool disableJsLogging() const;
    bool enableRenderRequestContextFix() const;
    bool disableHitTestSyncDeadline() const;
    // True when VALDI_MAX_VIEW_OPERATIONS_PROCESSING_TIME > 0 (throttling enabled). Gates top-down move order in TS.
    bool useTopDownMoveOrder() const;

private:
    Shared<ITweakValueProvider> _tweakValueProvider;

    bool getConfigKey(const char* key) const;
};

} // namespace Valdi
