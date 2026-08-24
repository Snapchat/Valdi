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
    bool enableScopedContextStackTraceCapture() const;
    bool enableRenderRequestContextFix() const;
    // Killswitch for the pre-raster quiescence fence in BridgedView::rasterInto.
    bool disablePreRasterFence() const;
    bool applyManagedChildFramePadding() const;
    bool disableHitTestSyncDeadline() const;
    // True when VALDI_MAX_VIEW_OPERATIONS_PROCESSING_TIME > 0 (throttling enabled). Gates top-down move order in TS.
    bool useTopDownMoveOrder() const;
    bool enableMmapModuleArchives() const;
    // True when the module matches a prefix in VALDI_MMAP_MODULE_ARCHIVES_DENYLIST
    // (comma-separated). Denylisted modules keep heap-backed archives even when
    // enableMmapModuleArchives() is on — on swapless iOS that de-facto pins them,
    // trading back their share of the memory win to avoid refault latency on
    // bursty surfaces.
    bool isMmapModuleArchiveDenylisted(const StringBox& modulePath) const;
    bool enableANRDiagnostics() const;
    bool enableFixFlexBasisFitContent() const;
    // Number of modules ModuleLoader.preloadBatch evaluates per JS-scheduler task before yielding.
    // 0 (default) keeps preload as a single uninterrupted task. > 0 bounds the max contiguous JS
    // occupancy during capture-start preload so the 5s Composer watchdog ack can run. Read via getInt.
    int32_t preloadYieldChunkSize() const;

private:
    Shared<ITweakValueProvider> _tweakValueProvider;

    bool getConfigKey(const char* key) const;
};

} // namespace Valdi
