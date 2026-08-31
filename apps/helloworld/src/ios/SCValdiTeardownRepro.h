//
//  SCValdiTeardownRepro.h
//  helloworld
//
//  Demonstrates the bridge-function resolution teardown behavior, both with and without the
//  resolution-teardown degrade.
//

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

/**
 Debug-only harness that demonstrates what happens when a bridge function is resolved through the
 raising path (+[SCValdiBridgeFunction functionWithJSRuntime:]) while its JS runtime is being torn
 down — the situation a Valdi session hits on logout / account switch.

 It stands up an isolated SCValdiRuntimeManager, captures the JS runtime, forces the
 VALDI_ENABLE_RESOLUTION_TEARDOWN_DEGRADE kill switch to a known state on that runtime, tears the
 runtime down, then resolves a probe bridge function off the main thread.

 The isolated runtime is used so teardown can be triggered deterministically without touching the
 app's shared runtime (which would kill the on-screen UI).

 Two modes make the fix visible side by side:
   - degrade ON (the shipped default): resolution after teardown returns a non-nil no-op function
     and unwinds quietly — no crash.
   - degrade OFF (kill switch disabled): resolution after teardown raises an uncatchable SCValdiError
     below the calling frame and the process aborts (SIGABRT) — the original crash.
 */
@interface SCValdiTeardownRepro : NSObject

/**
 Resolves the probe bridge function after tearing its runtime down, with the resolution-teardown
 degrade forced to @c degradeEnabled on the isolated runtime. Runs the resolution on a background
 queue (async_strict_mode forbids resolution on the main thread).

 With @c degradeEnabled == NO the resolution raises an uncaught SCValdiError and aborts the process
 (SIGABRT). With @c degradeEnabled == YES it returns a no-op function and logs without crashing.
 */
+ (void)reproduceWithDegradeEnabled:(BOOL)degradeEnabled;

@end

NS_ASSUME_NONNULL_END
