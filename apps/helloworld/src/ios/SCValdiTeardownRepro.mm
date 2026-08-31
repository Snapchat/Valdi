//
//  SCValdiTeardownRepro.mm
//  helloworld
//

#import "SCValdiTeardownRepro.h"

// Generated bridge-function class SCCTeardownProbeMakeTeardownProbe for makeTeardownProbe (see
// teardown_probe/src/TeardownReproProbe.ts). The +functionWithJSRuntime: classes live in the
// module umbrella header (SCCTeardownProbe), not the *Types header.
#import <SCCTeardownProbe/SCCTeardownProbe.h>

#import <valdi/ios/SCValdiRuntime.h>
#import <valdi/ios/SCValdiRuntimeManager.h>
#import <valdi_core/SCValdiError.h>
#import <valdi_core/SCValdiJSRuntime.h>

#include <cstring>

#include "valdi/runtime/Interfaces/ITweakValueProvider.hpp"
#include "valdi/runtime/RuntimeManager.hpp"
#include "valdi_core/cpp/Utils/Shared.hpp"
#include "valdi_core/cpp/Utils/StringBox.hpp"
#include "valdi_core/cpp/Utils/Value.hpp"

namespace {

// Reports a fixed value for VALDI_ENABLE_RESOLUTION_TEARDOWN_DEGRADE so the harness can drive both
// kill-switch states; every other key falls through to its caller-supplied fallback. The value is
// cached on the JS runtime (Runtime::setRuntimeTweaks -> setResolutionTeardownDegradeEnabled) before
// teardown detaches the listener, so it stays reachable while the runtime is being disposed.
class TeardownDegradeProvider : public Valdi::SharedPtrRefCountable, public Valdi::ITweakValueProvider {
  public:
    explicit TeardownDegradeProvider(bool degradeEnabled) : _degradeEnabled(degradeEnabled) {}

    Valdi::StringBox getString(const Valdi::StringBox &, const Valdi::StringBox &fallback) override {
        return fallback;
    }
    bool getBool(const Valdi::StringBox &key, bool fallback) override {
        if (std::strcmp(key.getCStr(), "VALDI_ENABLE_RESOLUTION_TEARDOWN_DEGRADE") == 0) {
            return _degradeEnabled;
        }
        return fallback;
    }
    float getFloat(const Valdi::StringBox &, float fallback) override { return fallback; }
    int32_t getInt(const Valdi::StringBox &, int32_t fallback) override { return fallback; }
    Valdi::Value getBinary(const Valdi::StringBox &, const Valdi::Value &fallback) override { return fallback; }

  private:
    bool _degradeEnabled;
};

} // namespace

@implementation SCValdiTeardownRepro

+ (void)reproduceWithDegradeEnabled:(BOOL)degradeEnabled
{
    // Isolated runtime so teardown can be forced without touching the app's shared runtime.
    // __block so the async worker below can drop the last strong reference and trigger teardown.
    __block SCValdiRuntimeManager *manager = [SCValdiRuntimeManager new];

    // Force the main runtime into existence and capture its JS runtime. jsRuntime is captured
    // strongly; it outlives the manager and keeps the disposed runtime addressable after teardown.
    SCValdiRuntime *runtime = manager.mainRuntime;
    id<SCValdiJSRuntime> jsRuntime = [runtime jsRuntime];
    runtime = nil;

    // Pin the resolution-teardown degrade to the requested state on this isolated runtime.
    // setTweakValueProvider propagates through Runtime::setRuntimeTweaks to the JS runtime's cached
    // kill-switch value, so it is captured before teardown detaches the listener.
    auto *cppManager = static_cast<Valdi::RuntimeManager *>(manager.cppInstance);
    if (cppManager != nullptr) {
        cppManager->setTweakValueProvider(
            Valdi::makeShared<TeardownDegradeProvider>(degradeEnabled ? true : false).toShared());
    }

    // Everything runs off the main thread: async_strict_mode forbids bridge-function resolution on
    // the main thread. Resolution happens on a plain global queue (not the runtime's JS thread,
    // which is gone after teardown), matching how the crash is hit in production.
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
        // Sanity: on the still-live runtime the probe resolves normally. Logged, never fatal — a
        // failure here means the probe module isn't bundled, not a teardown misfire.
        @try {
            SCCTeardownProbeMakeTeardownProbe *live = [SCCTeardownProbeMakeTeardownProbe functionWithJSRuntime:jsRuntime];
            NSLog(@"[TeardownRepro] live resolve ok: %@", live);
        } @catch (SCValdiError *error) {
            NSLog(@"[TeardownRepro] live resolve raised (probe module may not be bundled): %@", error.reason);
        }

        // Tear the runtime down. Dropping the manager's last strong reference runs its dealloc, which
        // drives fullTeardown -> JavaScriptRuntime::teardown and disposes the runtime synchronously.
        manager = nil;

        if (degradeEnabled) {
            // Degrade ON (shipped default): the raising functionWithJSRuntime: path returns a no-op
            // function instead of raising, so a dying session unwinds quietly. No @try on purpose —
            // this must NOT crash.
            SCCTeardownProbeMakeTeardownProbe *afterTeardown = [SCCTeardownProbeMakeTeardownProbe functionWithJSRuntime:jsRuntime];
            NSLog(@"[TeardownRepro] degrade ON: resolved after teardown without crashing: %@", afterTeardown);
        } else {
            // Degrade OFF (kill switch disabled): resolution after teardown raises an uncatchable
            // SCValdiError below this ObjC frame and the process aborts (SIGABRT) — the original
            // teardown crash. No @try on purpose: the point is the crash.
            NSLog(@"[TeardownRepro] degrade OFF: resolving after teardown, expect SIGABRT...");
            SCCTeardownProbeMakeTeardownProbe *afterTeardown = [SCCTeardownProbeMakeTeardownProbe functionWithJSRuntime:jsRuntime];
            NSLog(@"[TeardownRepro] UNEXPECTED: resolution did not crash: %@", afterTeardown);
        }
    });
}

@end
