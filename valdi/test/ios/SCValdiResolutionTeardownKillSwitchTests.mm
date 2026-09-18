//
//  SCValdiResolutionTeardownKillSwitchTests.mm
//  ios_tests
//
//  Pins the VALDI_ENABLE_RESOLUTION_TEARDOWN_DEGRADE kill switch end to end: resolving a bridge
//  function through the raising path after the JS runtime is torn down degrades to a no-op when the
//  switch is on (default), and raises the original SCValdiError when the switch is off. The switch is
//  read from a value the runtime holds independently of its listener, so it stays reachable during
//  teardown (Runtime::setRuntimeTweaks -> JavaScriptRuntime::setResolutionTeardownDegradeEnabled ->
//  pushModuleToMarshaller).
//

#import <Foundation/Foundation.h>
#import <XCTest/XCTest.h>

#import <SCCValdiTest/SCCValdiTest.h>

#import "valdi/ios/SCValdiRuntimeManager.h"
#import "valdi_core/SCValdiError.h"
#import "valdi_core/SCValdiJSRuntime.h"

#include <cstring>

#include "valdi/runtime/Interfaces/ITweakValueProvider.hpp"
#include "valdi/runtime/RuntimeManager.hpp"
#include "valdi_core/cpp/Utils/Shared.hpp"
#include "valdi_core/cpp/Utils/StringBox.hpp"
#include "valdi_core/cpp/Utils/Value.hpp"

namespace {

// Reports a fixed value for VALDI_ENABLE_RESOLUTION_TEARDOWN_DEGRADE so the test can drive both
// kill-switch states; every other key falls through to its caller-supplied fallback.
class FixedResolutionTeardownDegradeProvider : public Valdi::SharedPtrRefCountable, public Valdi::ITweakValueProvider {
  public:
    explicit FixedResolutionTeardownDegradeProvider(bool degradeEnabled) : _degradeEnabled(degradeEnabled) {}

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

@interface SCValdiResolutionTeardownKillSwitchTests : XCTestCase
@end

@implementation SCValdiResolutionTeardownKillSwitchTests

// Forces the kill switch to the given value on an isolated runtime, tears the runtime down, then
// resolves a bridge function off-main (async_strict_mode forbids resolution on the main thread).
// Returns YES if the resolution raised an SCValdiError.
- (BOOL)resolveAfterTeardownRaisesWithDegradeEnabled:(BOOL)degradeEnabled
{
    SCValdiRuntimeManager *manager = [SCValdiRuntimeManager new];
    id<SCValdiRuntimeProtocol> runtime = manager.mainRuntime;
    XCTAssertNotNil(runtime);
    id<SCValdiJSRuntime> jsRuntime = [runtime jsRuntime];
    XCTAssertNotNil(jsRuntime);
    runtime = nil;

    // setTweakValueProvider propagates through Runtime::setRuntimeTweaks to the JS runtime's cached
    // kill-switch value, so it is captured before teardown detaches the listener.
    auto *cppManager = static_cast<Valdi::RuntimeManager *>(manager.cppInstance);
    XCTAssertTrue(cppManager != nullptr);
    cppManager->setTweakValueProvider(
        Valdi::makeShared<FixedResolutionTeardownDegradeProvider>(degradeEnabled ? true : false).toShared());

    // Dropping the last strong reference drives fullTeardown and disposes the runtime; jsRuntime keeps
    // the disposed runtime addressable.
    manager = nil;

    __block BOOL raised = NO;
    XCTestExpectation *expectation = [self expectationWithDescription:@"resolve after teardown completed"];
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
        @try {
            (void)[SCCValdiTestMakeTestObject functionWithJSRuntime:jsRuntime];
        } @catch (SCValdiError *error) {
            raised = YES;
        }
        [expectation fulfill];
    });
    [self waitForExpectations:@[expectation] timeout:10.0];
    return raised;
}

// Kill switch on (default): resolution after teardown degrades to a no-op and does not raise.
- (void)testDegradeOnDoesNotRaiseAfterTeardown
{
    self.continueAfterFailure = NO;
    XCTAssertFalse([self resolveAfterTeardownRaisesWithDegradeEnabled:YES],
                   @"With the teardown-degrade kill switch on, resolution after teardown must not raise");
}

// Kill switch off (COF disabled): resolution after teardown raises the original SCValdiError. This is
// the assertion that the COF is a real lever, not a dead read.
- (void)testDegradeOffRaisesAfterTeardown
{
    self.continueAfterFailure = NO;
    XCTAssertTrue([self resolveAfterTeardownRaisesWithDegradeEnabled:NO],
                  @"With the teardown-degrade kill switch off, resolution after teardown must raise SCValdiError");
}

@end
