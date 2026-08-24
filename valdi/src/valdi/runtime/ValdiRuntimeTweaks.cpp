//
//  ValdiRuntimeTweaks.cpp
//  valdi
//
//  Created by Simon Corsin on 3/1/22.
//

#include "valdi/runtime/ValdiRuntimeTweaks.hpp"
#include "valdi/valdi.pb.h"
#include "valdi_core/cpp/Utils/Bytes.hpp"
#include "valdi_core/cpp/Utils/ConsoleLogger.hpp"
#include "valdi_core/cpp/Utils/Format.hpp"
#include "valdi_core/cpp/Utils/JSONReader.hpp"
#include "valdi_core/cpp/Utils/StringBox.hpp"
#include "valdi_core/cpp/Utils/StringCache.hpp"
#include "valdi_core/cpp/Utils/ValueTypedArray.hpp"
#include "valdi_core/cpp/Utils/ValueUtils.hpp"

namespace Valdi {

bool ValdiRuntimeTweaks::getConfigKey(const char* key) const {
    auto configKey = StringCache::getGlobal().makeStringFromLiteral(std::string_view(key));
    return _tweakValueProvider->getBool(configKey, false);
}

ValdiRuntimeTweaks::ValdiRuntimeTweaks(const Shared<ITweakValueProvider>& tweakValueProvider)
    : _tweakValueProvider(tweakValueProvider) {}
ValdiRuntimeTweaks::~ValdiRuntimeTweaks() = default;

bool ValdiRuntimeTweaks::enableAccessibility() const {
    return getConfigKey("VALDI_ENABLE_ACCESSIBILITY_TRAITS");
}

bool ValdiRuntimeTweaks::enableDeferredGC() const {
    return getConfigKey("VALDI_ENABLE_DEFERRED_GC");
}

bool ValdiRuntimeTweaks::enableCommonJsModuleLoader() const {
    return getConfigKey("VALDI_ENABLE_COMMONJS_MODULE_LOADER");
}

bool ValdiRuntimeTweaks::disableHotReloaderLazyDenylist() const {
    return getConfigKey("VALDI_DISABLE_HOTRELOADER_LAZY_DENYLIST");
}

bool ValdiRuntimeTweaks::disableSyncCallsInCallingThread() const {
    return getConfigKey("VALDI_DISABLE_SYNC_CALLS_IN_CALLING_THREAD");
}

bool ValdiRuntimeTweaks::enableTSN() const {
    return getConfigKey("VALDI_ENABLE_TSN");
}

bool ValdiRuntimeTweaks::shouldNudgeJSThread() const {
    return getConfigKey("VALDI_ENABLE_JSTHREAD_NUDGE");
}

bool ValdiRuntimeTweaks::enableTSNForModule(const StringBox& moduleName) const {
    auto const key = StringCache::getGlobal().makeStringFromLiteral(std::string_view("VALDI_TSN_ENABLED_MODULES"));
    auto const fallback = Value(makeShared<ValueTypedArray>(TypedArrayType::Uint8Array, Valdi::BytesView()));

    Value binary = _tweakValueProvider->getBinary(key, fallback);

    // If configured (has at least one per-module config, use per-module config)
    if (binary.isTypedArray()) {
        const ValueTypedArray* array = binary.getTypedArray();
        const BytesView& bytes = array->getBuffer();

        Valdi::TsnConfig tsnConfig;
        bool parsed = tsnConfig.ParseFromArray(bytes.data(), static_cast<int>(bytes.size()));

        if (parsed) {
            // trivial first pass because this is only used for a very small number of elements
            for (const auto& module : tsnConfig.enabled_modules()) {
                if (moduleName.hasPrefix(module.c_str())) {
                    Valdi::ConsoleLogger::getLogger().log(
                        Valdi::LogTypeDebug, fmt::format("TSN enabled for module: {} ", moduleName.slowToString()));
                    return true; // prefix match success
                }
            }
            // has configured modules, but not a match, return false
            // not configured (empty list), return true
            return tsnConfig.enabled_modules_size() == 0;
        }
    }
    // If not configured(null, failing to parse), return true
    // (fallback to global tsn config)
    return true;
}

bool ValdiRuntimeTweaks::shouldCrashOnANR() const {
    return getConfigKey("VALDI_ENABLE_CRASH_ON_ANR");
}

bool ValdiRuntimeTweaks::disableAnimationRemoveOnCompleteIos() const {
    return getConfigKey("VALDI_DISABLE_ANIMATION_REMOVE_ON_COMPLETE_IOS");
}

bool ValdiRuntimeTweaks::enableScopedContextStackTraceCapture() const {
    static const StringBox kKey =
        StringCache::getGlobal().makeStringFromLiteral("VALDI_ENABLE_SCOPED_CONTEXT_STACK_TRACE_CAPTURE");
    return _tweakValueProvider->getBool(kKey, false);
}

bool ValdiRuntimeTweaks::enableRenderRequestContextFix() const {
    auto configKey =
        StringCache::getGlobal().makeStringFromLiteral(std::string_view("VALDI_ENABLE_RENDER_REQUEST_CONTEXT_FIX"));
    return _tweakValueProvider->getBool(configKey, true);
}

bool ValdiRuntimeTweaks::disablePreRasterFence() const {
    return getConfigKey("VALDI_DISABLE_PRE_RASTER_FENCE");
}

bool ValdiRuntimeTweaks::applyManagedChildFramePadding() const {
    auto configKey =
        StringCache::getGlobal().makeStringFromLiteral(std::string_view("VALDI_MANAGES_CHILD_FRAME_PADDING_ENABLED"));
    return _tweakValueProvider->getBool(configKey, true);
}

bool ValdiRuntimeTweaks::disableHitTestSyncDeadline() const {
    return getConfigKey("VALDI_DISABLE_HIT_TEST_SYNC_DEADLINE");
}

bool ValdiRuntimeTweaks::useTopDownMoveOrder() const {
    auto configKey =
        StringCache::getGlobal().makeStringFromLiteral(std::string_view("VALDI_MAX_VIEW_OPERATIONS_PROCESSING_TIME"));
    return _tweakValueProvider->getInt(configKey, 0) > 0;
}

bool ValdiRuntimeTweaks::enableMmapModuleArchives() const {
    return getConfigKey("VALDI_ENABLE_MMAP_MODULE_ARCHIVES");
}

bool ValdiRuntimeTweaks::isMmapModuleArchiveDenylisted(const StringBox& modulePath) const {
    static const StringBox kKey = StringCache::getGlobal().makeStringFromLiteral("VALDI_MMAP_MODULE_ARCHIVES_DENYLIST");
    auto denylist = _tweakValueProvider->getString(kKey, StringBox());
    if (denylist.isEmpty()) {
        return false;
    }

    auto moduleView = modulePath.toStringView();
    auto listView = denylist.toStringView();
    size_t start = 0;
    while (start <= listView.size()) {
        auto end = listView.find(',', start);
        if (end == std::string_view::npos) {
            end = listView.size();
        }
        auto prefix = listView.substr(start, end - start);
        while (!prefix.empty() && prefix.front() == ' ') {
            prefix.remove_prefix(1);
        }
        while (!prefix.empty() && prefix.back() == ' ') {
            prefix.remove_suffix(1);
        }
        if (!prefix.empty() && moduleView.substr(0, prefix.size()) == prefix) {
            return true;
        }
        start = end + 1;
    }
    return false;
}

bool ValdiRuntimeTweaks::enableANRDiagnostics() const {
    // Key name kept from the earlier module-load diagnostics so the existing COF config carries over.
    return getConfigKey("VALDI_ENABLE_MODULE_LOAD_DIAGNOSTICS");
}

bool ValdiRuntimeTweaks::enableFixFlexBasisFitContent() const {
    return getConfigKey("VALDI_ENABLE_FIX_FLEX_BASIS_FIT_CONTENT");
}

int32_t ValdiRuntimeTweaks::preloadYieldChunkSize() const {
    auto configKey = StringCache::getGlobal().makeStringFromLiteral(std::string_view("VALDI_PRELOAD_YIELD_CHUNK_SIZE"));
    return _tweakValueProvider->getInt(configKey, 0);
}

} // namespace Valdi
