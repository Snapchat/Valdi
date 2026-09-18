#import <UIKit/UIKit.h>
#import <XCTest/XCTest.h>

#import "valdi/ios/NativeModules/SCValdiBridgeModuleUtils.h"
#import "valdi/ios/NativeModules/SCValdiDeviceModule.h"
#import "valdi_core/SCValdiRootView.h"

// _currentDisplaySize is the module's key-window-anchored size source; it is intentionally
// not part of the public header, so redeclare it here for direct comparison.
@interface SCValdiDeviceModule (SCValdiDeviceModuleTests)
- (CGSize)_currentDisplaySize;
- (void)_updateDisplaySize:(CGSize)size scale:(CGFloat)scale;
- (void)_observeGeometryOfWindowSceneIfNeeded:(UIWindowScene *)windowScene API_AVAILABLE(ios(16.0));
@end

// Counts notifications without forwarding: super's nil-marshaller path allocates a scoped
// marshaller, which the module never needs for these observers and tests shouldn't depend on.
@interface SCValdiCountingBridgeObserver : SCValdiBridgeObserver
@property (nonatomic) NSInteger notifyCount;
@end

@implementation SCValdiCountingBridgeObserver
- (void)notifyWithMarshaller:(SCValdiMarshaller *)marshaller
{
    self.notifyCount++;
}
@end

// SCValdiDeviceModule is a process-wide size tracker and it subscribes to
// SCValdiRootViewDidMoveToWindowNotificationKey with object:nil, so EVERY root view that
// attaches to ANY window reaches it. A root view hosted in a non-key window (an overlay,
// a tooltip window, or a fixed-size test stub) must not overwrite the display size that
// Device.getDisplayWidth()/getWindowWidth() report for the whole runtime.
@interface SCValdiDeviceModuleTests : XCTestCase
@end

@implementation SCValdiDeviceModuleTests {
    UIWindow *_stubWindow;
}

- (void)tearDown
{
    _stubWindow.hidden = YES;
    _stubWindow = nil;
    [super tearDown];
}

// No zero-arg getter exists (values are exposed through marshaller-based bridge methods),
// so read the ivars through KVC.
- (CGFloat)displayWidthOf:(SCValdiDeviceModule *)module
{
    return [[module valueForKey:@"displayWidth"] doubleValue];
}

- (CGFloat)displayHeightOf:(SCValdiDeviceModule *)module
{
    return [[module valueForKey:@"displayHeight"] doubleValue];
}

- (void)testRootViewInNonKeyWindowDoesNotChangeDisplaySize
{
    SCValdiDeviceModule *module = [[SCValdiDeviceModule alloc] initWithJSQueueDispatcher:nil];

    CGSize expected = [module _currentDisplaySize];
    XCTAssertEqualWithAccuracy([self displayWidthOf:module], expected.width, 0.001);
    XCTAssertEqualWithAccuracy([self displayHeightOf:module], expected.height, 0.001);

    // Deliberately absurd bounds so a false adoption cannot hide behind a size that
    // happens to match the host's screen.
    _stubWindow = [[UIWindow alloc] initWithFrame:CGRectMake(0, 0, 123, 457)];
    _stubWindow.hidden = NO;
    XCTAssertFalse(_stubWindow.isKeyWindow);

    // Attaching posts SCValdiRootViewDidMoveToWindowNotificationKey from didMoveToWindow,
    // exercising the real end-to-end path into the module.
    SCValdiRootView *rootView = [[SCValdiRootView alloc] initWithoutValdiContext];
    [_stubWindow addSubview:rootView];
    XCTAssertNotNil(rootView.window);

    XCTAssertEqualWithAccuracy([self displayWidthOf:module], expected.width, 0.001);
    XCTAssertEqualWithAccuracy([self displayHeightOf:module], expected.height, 0.001);
}

// The size and insets observers are separate contracts: TypeScript invalidates its Device
// caches directly on the size observer (registered at Device module load, ahead of any
// component callback), and real inset changes arrive via safeAreaInsetsDidChange with their
// own diffing. Broadcasting a fake insets event on every resize frame forces every insets
// subscriber to re-render throughout a continuous drag.
- (void)testDisplaySizeChangeNotifiesSizeObserverOnly
{
    SCValdiDeviceModule *module = [[SCValdiDeviceModule alloc] initWithJSQueueDispatcher:nil];
    SCValdiCountingBridgeObserver *sizeObserver = [SCValdiCountingBridgeObserver new];
    SCValdiCountingBridgeObserver *insetsObserver = [SCValdiCountingBridgeObserver new];
    [module setValue:sizeObserver forKey:@"displaySizeObserver"];
    [module setValue:insetsObserver forKey:@"displayInsetsObserver"];

    [module _updateDisplaySize:CGSizeMake(999, 888) scale:2];

    XCTAssertEqual(sizeObserver.notifyCount, 1);
    XCTAssertEqual(insetsObserver.notifyCount, 0);

    // Unchanged size must not re-notify.
    [module _updateDisplaySize:CGSizeMake(999, 888) scale:2];
    XCTAssertEqual(sizeObserver.notifyCount, 1);
}

// Longstanding contract predating the size observer: rotation always pings the insets
// observer, even when neither the size nor the insets end up changing.
- (void)testOrientationChangeStillNotifiesInsetsObserver
{
    SCValdiDeviceModule *module = [[SCValdiDeviceModule alloc] initWithJSQueueDispatcher:nil];
    SCValdiCountingBridgeObserver *sizeObserver = [SCValdiCountingBridgeObserver new];
    SCValdiCountingBridgeObserver *insetsObserver = [SCValdiCountingBridgeObserver new];
    [module setValue:sizeObserver forKey:@"displaySizeObserver"];
    [module setValue:insetsObserver forKey:@"displayInsetsObserver"];

    [[NSNotificationCenter defaultCenter] postNotificationName:UIDeviceOrientationDidChangeNotification
                                                        object:nil];
    // The handler re-reads the screen on the next main-queue turn.
    [[NSRunLoop mainRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.05]];

    XCTAssertGreaterThanOrEqual(insetsObserver.notifyCount, 1);
    XCTAssertEqual(sizeObserver.notifyCount, 0);
}

// Scenes outlive runtimes (each of which owns its own device module), and Apple's KVO
// contract requires observers to deregister before deallocating. Modern KVO stores observers
// weakly, so a missing dealloc doesn't crash outright, but every dead runtime would leave a
// stale zeroed observance accumulating on the scene, and delivery safety would rest on that
// undocumented weak-observer behavior. A plain NSObject stands in for the scene: the KVO
// add/remove mechanics are identical and the bazel test host runs without any connected
// UIWindowScene.
- (void)testDeallocRemovesSceneGeometryObservation
{
    if (@available(iOS 16.0, *)) {
        NSObject *scene = [NSObject new];

        @autoreleasepool {
            SCValdiDeviceModule *module = [[SCValdiDeviceModule alloc] initWithJSQueueDispatcher:nil];
            [module _observeGeometryOfWindowSceneIfNeeded:(UIWindowScene *)scene];
            module = nil;
        }

        // Nothing observed this fresh object except the module, so after the module deallocs
        // its observation info must carry no effectiveGeometry observance at all.
        NSString *keyPath = NSStringFromSelector(@selector(effectiveGeometry));
        NSString *observationInfo = [NSString stringWithFormat:@"%@", scene.observationInfo
                                                                          ? (__bridge id)scene.observationInfo
                                                                          : nil];
        XCTAssertFalse([observationInfo containsString:keyPath],
                       @"scene still holds an observance from the deallocated module: %@", observationInfo);

        // Delivering a geometry change must be a no-op rather than a message to freed memory.
        [scene willChangeValueForKey:keyPath];
        [scene didChangeValueForKey:keyPath];
    }
}

@end
