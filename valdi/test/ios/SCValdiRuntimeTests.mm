//
//  SCValdiRuntimeTests.m
//  ios_tests
//
//  Created by Simon Corsin on 4/28/21.
//

#import <Foundation/Foundation.h>
#import <XCTest/XCTest.h>
#import <OCMock/OCMock.h>

#import "valdi/ios/SCValdiRuntimeManager.h"
#import "valdi/ios/Views/SCValdiLabel.h"
#import "valdi/ios/Gestures/SCValdiGestureRecognizers.h"
#import "valdi/ios/Utils/SCValdiImageFilter.h"
#import "valdi/runtime/Utils/AsyncGroup.hpp"
#import "valdi_core/cpp/Threading/DispatchQueue.hpp"
#import "valdi_core/cpp/Threading/GCDDispatchQueue.hpp"
#import "valdi_core/SCValdiScrollView.h"
#import "valdi_core/SCValdiRootView.h"
#import "valdi_core/UIView+ValdiBase.h"

#import <SCCValdiTest/SCCValdiTest.h>
#import <SCCValdiTestTypes/SCCValdiTestTypes.h>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wgnu-zero-variadic-macro-arguments"
#pragma clang diagnostic ignored "-Wgnu-statement-expression"

@interface SCCValdiTestListenerImpl: NSObject<SCCValdiTestListener>

@property (copy, nonatomic) dispatch_block_t onRenderCallback;

@end

@implementation SCCValdiTestListenerImpl

- (void)onRender
{
    if (self.onRenderCallback) {
        self.onRenderCallback();
    }
}

@end

@interface SCValdiRuntimeTests: XCTestCase

@property (strong, nonatomic) SCValdiRuntimeManager *runtimeManager;

@end

@implementation SCValdiRuntimeTests

- (id<SCValdiRuntimeProtocol>)runtime
{
    return self.runtimeManager.mainRuntime;
}

- (void)getRuntimeUsingBatch:(BOOL)useMainThreadBatch withBlock:(void(^)(id<SCValdiRuntimeProtocol> runtime))block
{
    @autoreleasepool {
        id<SCValdiRuntimeProtocol> runtime = self.runtimeManager.mainRuntime;
        if (useMainThreadBatch) {
            [runtime executeMainThreadBatch:^{
                block(runtime);
            }];
        } else {
            block(runtime);
        }
    }
}

- (void)getRuntimeWithBlock:(void(^)(id<SCValdiRuntimeProtocol> runtime))block
{
    [self getRuntimeUsingBatch:YES withBlock:block];
}

- (void)setUp
{
    self.runtimeManager = [SCValdiRuntimeManager new];

    self.continueAfterFailure = NO;
}

- (void)tearDown
{
    self.runtimeManager = nil;
}

- (BOOL)_simulateTapOnView:(UIView *)view atLocation:(CGPoint)location
{
    UIView *hitView = [view hitTest:location withEvent:nil];
    if (!hitView) {
        return NO;
    }

    for (UIGestureRecognizer *gestureRecognizer in hitView.gestureRecognizers) {
        if ([gestureRecognizer isKindOfClass:[SCValdiTapGestureRecognizer class]]) {
            SCValdiTapGestureRecognizer *tapGesture = (SCValdiTapGestureRecognizer *)gestureRecognizer;

            @autoreleasepool {
                id<SCValdiRuntimeProtocol> runtime = self.runtimeManager.mainRuntime;
                [runtime executeMainThreadBatch:^{
                    [tapGesture triggerAtLocation:[hitView convertPoint:location fromView:view] forState:UIGestureRecognizerStateEnded];
                }];
            }

            return YES;
        }
    }

    return NO;
}

- (void)testCanWaitUntilRenderCompleted
{
    __block id<SCValdiContextProtocol> context;
    [self getRuntimeUsingBatch:NO withBlock:^(id<SCValdiRuntimeProtocol> runtime) {
        SCCValdiTestViewModel *viewModel = [[SCCValdiTestViewModel alloc] initWithHeaderTitle:@"" scrollable:NO entries:@[]];
        SCCValdiTestContext *componentContext = [[SCCValdiTestContext alloc] initWithListener:[SCCValdiTestListenerImpl new] onTap:^(double index) {}];
        context = [runtime createContextWithViewClass:[SCCValdiTestIntegrationTests class] viewModel:viewModel componentContext:componentContext];
    }];

    XCTAssertNil(context.rootViewNode);

    [context waitUntilRenderCompletedSyncWithFlush:YES];

    XCTAssertNotNil(context.rootViewNode);
}

- (void)testCanExploreViewNodeTree
{
    __block id<SCValdiContextProtocol> context;
    [self getRuntimeWithBlock:^(id<SCValdiRuntimeProtocol> runtime) {
        SCCValdiTestViewModel *viewModel = [[SCCValdiTestViewModel alloc] initWithHeaderTitle:@"Hello World!" scrollable:NO entries:@[]];
        SCCValdiTestContext *componentContext = [[SCCValdiTestContext alloc] initWithListener:[SCCValdiTestListenerImpl new] onTap:^(double index) {}];
        context = [runtime createContextWithViewClass:[SCCValdiTestIntegrationTests class] viewModel:viewModel componentContext:componentContext];
    }];
    [context waitUntilRenderCompletedSyncWithFlush:YES];

    id<SCValdiViewNodeProtocol> rootViewNode = context.rootViewNode;
    XCTAssertNotNil(rootViewNode);

    NSArray<id<SCValdiViewNodeProtocol>> *children = [rootViewNode children];

    XCTAssertEqual(2, children.count);
    id<SCValdiViewNodeProtocol> headerLabel = [children firstObject];

    id value = [headerLabel valueForValdiAttribute:@"value"];
    XCTAssertEqualObjects(@"Hello World!", value);
}

- (void)testCanInflateView
{
    __block SCCValdiTestIntegrationTests *rootView;
    [self getRuntimeWithBlock:^(id<SCValdiRuntimeProtocol> runtime) {
        SCCValdiTestViewModel *viewModel = [[SCCValdiTestViewModel alloc] initWithHeaderTitle:@"Hello World!" scrollable:YES entries:@[]];
        SCCValdiTestContext *componentContext = [[SCCValdiTestContext alloc] initWithListener:[SCCValdiTestListenerImpl new] onTap:^(double index) {}];
        rootView = [[SCCValdiTestIntegrationTests alloc] initWithViewModel:viewModel componentContext:componentContext runtime:self.runtime];
    }];

    XCTAssertNotNil(rootView.valdiContext);
    [rootView.valdiContext waitUntilRenderCompletedSyncWithFlush:YES];

    XCTAssertNotNil(rootView.valdiViewNode);
    XCTAssertEqualObjects(@[], rootView.subviews);

    rootView.frame = CGRectMake(0, 0, 400, 800);
    [rootView layoutIfNeeded];

    XCTAssertNotEqualObjects(@[], rootView.subviews);
    XCTAssertEqual(2, rootView.subviews.count);

    SCValdiLabel *firstSubview = rootView.subviews[0];
    XCTAssertEqual([SCValdiLabel class], [firstSubview class]);

    SCValdiView *secondSubview = rootView.subviews[1];
    XCTAssertEqual([SCValdiView class], [secondSubview class]);

    SCValdiScrollView *scrollView = [secondSubview.subviews firstObject];
    XCTAssertEqual([SCValdiScrollView class], [scrollView class]);

    XCTAssertEqualObjects(@"Hello World!", firstSubview.text);
}

- (void)testDoesntTrackObjCReferencesWhenDisabled
{
    __block SCCValdiTestIntegrationTests *rootView;
    [self getRuntimeWithBlock:^(id<SCValdiRuntimeProtocol> runtime) {
        SCCValdiTestViewModel *viewModel = [[SCCValdiTestViewModel alloc] initWithHeaderTitle:@"Hello World!" scrollable:YES entries:@[]];
        SCCValdiTestContext *componentContext = [[SCCValdiTestContext alloc] initWithListener:[SCCValdiTestListenerImpl new] onTap:^(double index) {}];
        rootView = [[SCCValdiTestIntegrationTests alloc] initWithViewModel:viewModel componentContext:componentContext runtime:self.runtime];
    }];

    XCTAssertNotNil(rootView.valdiContext);
    [rootView.valdiContext waitUntilRenderCompletedSyncWithFlush:YES];

    NSArray<id> *objcReferences = rootView.valdiContext.trackedObjCReferences;
    XCTAssertNil(objcReferences);
}

- (void)testCanTrackObjCReferences
{
    [self.runtimeManager updateConfiguration:^(SCValdiConfiguration *configuration) {
        configuration.enableReferenceTracking = YES;
    }];

    SCCValdiTestListenerImpl *listenerImpl = [SCCValdiTestListenerImpl new];
    SCCValdiTestContextOnTapBlock onTap = [^(double index) {} copy];

    __block SCCValdiTestIntegrationTests *rootView;
    @autoreleasepool {
        [self getRuntimeWithBlock:^(id<SCValdiRuntimeProtocol> runtime) {
            SCCValdiTestViewModel *viewModel = [[SCCValdiTestViewModel alloc] initWithHeaderTitle:@"Hello World!" scrollable:YES entries:@[]];
            SCCValdiTestContext *componentContext = [[SCCValdiTestContext alloc] initWithListener:listenerImpl onTap:onTap];
            rootView = [[SCCValdiTestIntegrationTests alloc] initWithViewModel:viewModel componentContext:componentContext runtime:self.runtime];
        }];

        id<SCValdiContextProtocol> valdiContext = rootView.valdiContext;
        XCTAssertNotNil(valdiContext);
        [valdiContext waitUntilRenderCompletedSyncWithFlush:YES];

        NSArray<id> *objcReferences = valdiContext.trackedObjCReferences;
        XCTAssertNotNil(objcReferences);

        XCTAssertTrue([objcReferences containsObject:listenerImpl]);
        XCTAssertTrue([objcReferences containsObject:onTap]);

        [valdiContext destroy];

        [self.runtimeManager.mainRuntime dispatchOnJSQueueWithBlock:^{
            // Flush JS queue
        } sync:YES];

        // Get references again
        objcReferences = valdiContext.trackedObjCReferences;
        XCTAssertNotNil(objcReferences);
        XCTAssertEqual(0, objcReferences.count);
    }

    XCTAssertEqual(1, CFGetRetainCount((__bridge CFTypeRef)(listenerImpl)));
    XCTAssertEqual(1, CFGetRetainCount((__bridge CFTypeRef)(onTap)));
}

- (void)testCanHandleTap
{
    NSMutableArray<NSNumber *> *tappedCards = [NSMutableArray new];
    __block SCCValdiTestIntegrationTests *rootView;
    [self getRuntimeWithBlock:^(id<SCValdiRuntimeProtocol> runtime) {
        NSArray<SCCValdiTestEntry *> *entries = @[
            [[SCCValdiTestEntry alloc] initWithColor:@"black" text:@"First"],
            [[SCCValdiTestEntry alloc] initWithColor:@"black" text:@"Second"],
            [[SCCValdiTestEntry alloc] initWithColor:@"black" text:@"Last"],
        ];
        SCCValdiTestViewModel *viewModel = [[SCCValdiTestViewModel alloc] initWithHeaderTitle:@"Hello World!" scrollable:NO entries:entries];
        SCCValdiTestContext *componentContext = [[SCCValdiTestContext alloc] initWithListener:[SCCValdiTestListenerImpl new] onTap:^(double index) {
            @synchronized (tappedCards) {
                [tappedCards addObject:@(index)];
            }
        }];
        rootView = [[SCCValdiTestIntegrationTests alloc] initWithViewModel:viewModel componentContext:componentContext runtime:self.runtime];
    }];

    XCTAssertNotNil(rootView.valdiContext);
    [rootView.valdiContext waitUntilRenderCompletedSyncWithFlush:YES];

    rootView.frame = CGRectMake(0, 0, 400, 800);
    [rootView layoutSubviews];

    [self _simulateTapOnView:rootView atLocation:CGPointMake(196, 68)];

    @synchronized (tappedCards) {
        XCTAssertEqual(1, tappedCards.count);
        XCTAssertEqualObjects(@(0), tappedCards[0]);
    }

    [self _simulateTapOnView:rootView atLocation:CGPointMake(196, 88)];

    @synchronized (tappedCards) {
        XCTAssertEqual(2, tappedCards.count);
        XCTAssertEqualObjects(@(1), tappedCards[1]);
    }

    [self _simulateTapOnView:rootView atLocation:CGPointMake(196, 107)];

    @synchronized (tappedCards) {
        XCTAssertEqual(3, tappedCards.count);
        XCTAssertEqualObjects(@(2), tappedCards[2]);
    }
}

- (void)testCanMeasureContext
{
    __block id<SCValdiContextProtocol> context;
    [self getRuntimeWithBlock:^(id<SCValdiRuntimeProtocol> runtime) {
        NSArray<SCCValdiTestEntry *> *entries = @[
            [[SCCValdiTestEntry alloc] initWithColor:@"black" text:@"First"],
            [[SCCValdiTestEntry alloc] initWithColor:@"black" text:@"Second"],
            [[SCCValdiTestEntry alloc] initWithColor:@"black" text:@"Last"],

        ];
        SCCValdiTestViewModel *viewModel = [[SCCValdiTestViewModel alloc] initWithHeaderTitle:@"Hello World!" scrollable:NO entries:entries];
        SCCValdiTestContext *componentContext = [[SCCValdiTestContext alloc] initWithListener:[SCCValdiTestListenerImpl new] onTap:^(double index) {
        }];
        context = [runtime createContextWithViewClass:[SCCValdiTestIntegrationTests class] viewModel:viewModel componentContext:componentContext];
    }];

    [context waitUntilRenderCompletedSyncWithFlush:YES];

    CGSize size;

    {
        context.useLegacyMeasureBehavior = NO;

        size = [context measureLayoutWithMaxSize:CGSizeMake(CGFLOAT_MAX, CGFLOAT_MAX) direction:SCValdiLayoutDirectionLTR];

        XCTAssertEqualWithAccuracy(118.67, size.width, 0.5);
        XCTAssertEqualWithAccuracy(132.0, size.height, 0.5);

        size = [context measureLayoutWithMaxSize:CGSizeMake(500, CGFLOAT_MAX) direction:SCValdiLayoutDirectionLTR];

        XCTAssertEqualWithAccuracy(118.67, size.width, 0.5);
        XCTAssertEqualWithAccuracy(132.0, size.height, 0.5);

        size = [context measureLayoutWithMaxSize:CGSizeMake(80, CGFLOAT_MAX) direction:SCValdiLayoutDirectionLTR];

        XCTAssertEqualWithAccuracy(77.67, size.width, 0.5);
        XCTAssertEqualWithAccuracy(151.0, size.height, 0.5);

        size = [context measureLayoutWithMaxSize:CGSizeMake(80, 100) direction:SCValdiLayoutDirectionLTR];

        XCTAssertEqualWithAccuracy(77.67, size.width, 0.5);
        XCTAssertEqualWithAccuracy(100.0, size.height, 0.5);
    }

    {
        context.useLegacyMeasureBehavior = YES;

        size = [context measureLayoutWithMaxSize:CGSizeMake(CGFLOAT_MAX, CGFLOAT_MAX) direction:SCValdiLayoutDirectionLTR];

        XCTAssertEqualWithAccuracy(118.67, size.width, 0.5);
        XCTAssertEqualWithAccuracy(132.0, size.height, 0.5);

        size = [context measureLayoutWithMaxSize:CGSizeMake(500, CGFLOAT_MAX) direction:SCValdiLayoutDirectionLTR];

        XCTAssertEqualWithAccuracy(500.0, size.width, 0.5);
        XCTAssertEqualWithAccuracy(132.0, size.height, 0.5);

        size = [context measureLayoutWithMaxSize:CGSizeMake(80, CGFLOAT_MAX) direction:SCValdiLayoutDirectionLTR];

        XCTAssertEqualWithAccuracy(80.0, size.width, 0.5);
        XCTAssertEqualWithAccuracy(151.0, size.height, 0.5);

        size = [context measureLayoutWithMaxSize:CGSizeMake(80, 100) direction:SCValdiLayoutDirectionLTR];

        XCTAssertEqualWithAccuracy(80.0, size.width, 0.5);
        XCTAssertEqualWithAccuracy(100.0, size.height, 0.5);
    }
}

- (void)testCanUpdateAttributeThroughViewNode
{
    __block SCCValdiTestIntegrationTests *rootView;
    [self getRuntimeWithBlock:^(id<SCValdiRuntimeProtocol> runtime) {
        SCCValdiTestViewModel *viewModel = [[SCCValdiTestViewModel alloc] initWithHeaderTitle:@"Hello World!" scrollable:NO entries:@[]];
        SCCValdiTestContext *componentContext = [[SCCValdiTestContext alloc] initWithListener:[SCCValdiTestListenerImpl new] onTap:^(double index) {}];
        rootView = [[SCCValdiTestIntegrationTests alloc] initWithViewModel:viewModel componentContext:componentContext runtime:self.runtime];
    }];

    [rootView.valdiContext waitUntilRenderCompletedSyncWithFlush:YES];

    rootView.frame = CGRectMake(0, 0, 400, 800);
    [rootView layoutIfNeeded];

    SCValdiLabel *titleView = [rootView.subviews firstObject];
    XCTAssertNotNil(titleView);
    XCTAssertEqual([SCValdiLabel class], [titleView class]);

    XCTAssertEqualObjects(@"Hello World!", titleView.text);

    XCTAssertNotNil(titleView.valdiViewNode);

    [titleView.valdiViewNode setValue:@"Welcome to Unit Test!" forValdiAttribute:@"value"];
    [titleView layoutIfNeeded];

    XCTAssertEqualObjects(@"Welcome to Unit Test!", titleView.text);
}

- (void)testDestroysViewOnDealloc
{
    __block SCCValdiTestIntegrationTests *rootView;

    @autoreleasepool {
        [self getRuntimeWithBlock:^(id<SCValdiRuntimeProtocol> runtime) {
            SCCValdiTestViewModel *viewModel = [[SCCValdiTestViewModel alloc] initWithHeaderTitle:@"Hello World!" scrollable:NO entries:@[]];
            SCCValdiTestContext *componentContext = [[SCCValdiTestContext alloc] initWithListener:[SCCValdiTestListenerImpl new] onTap:^(double index) {}];
            rootView = [[SCCValdiTestIntegrationTests alloc] initWithViewModel:viewModel componentContext:componentContext runtime:self.runtime];
        }];
    }

    id<SCValdiContextProtocol> valdiContext = rootView.valdiContext;

    XCTAssertNotNil(valdiContext);
    @autoreleasepool {
        [valdiContext waitUntilRenderCompletedSyncWithFlush:YES];
    }

    XCTAssertFalse(valdiContext.destroyed);

    rootView = nil;

    XCTAssertTrue(valdiContext.destroyed);
}

- (void)testCanSetDeferredRootView
{
    __block id<SCValdiContextProtocol> valdiContext;

    @autoreleasepool {
        [self getRuntimeWithBlock:^(id<SCValdiRuntimeProtocol> runtime) {
            SCCValdiTestViewModel *viewModel = [[SCCValdiTestViewModel alloc] initWithHeaderTitle:@"Hello World!" scrollable:NO entries:@[]];
            SCCValdiTestContext *componentContext = [[SCCValdiTestContext alloc] initWithListener:[SCCValdiTestListenerImpl new] onTap:^(double index) {}];
            valdiContext = [runtime createContextWithViewClass:[SCCValdiTestIntegrationTests class] viewModel:viewModel componentContext:componentContext];
        }];
    }

    CGSize layoutSize = CGSizeMake(400, 800);

    [valdiContext waitUntilRenderCompletedSyncWithFlush:YES];
    [valdiContext setLayoutSize:layoutSize direction:SCValdiLayoutDirectionLTR];

    id<SCValdiViewNodeProtocol> rootViewNode = valdiContext.rootViewNode;
    id<SCValdiViewNodeProtocol> scrollViewNode = [[rootViewNode children] firstObject];

    XCTAssertNotNil(rootViewNode);
    XCTAssertNotNil(scrollViewNode);

    XCTAssertNil(rootViewNode.view);
    XCTAssertNil(scrollViewNode.view);

    SCValdiRootView *rootView = [[SCValdiRootView alloc] initWithoutValdiContext];
    rootView.enableViewInflationWhenInvisible = YES;
    rootView.frame = CGRectMake(0, 0, layoutSize.width, layoutSize.height);

    valdiContext.rootValdiView = rootView;

    XCTAssertEqual(rootView, rootViewNode.view);
    XCTAssertNotNil(scrollViewNode.view);

    valdiContext.rootValdiView = nil;

    XCTAssertNil(rootViewNode.view);
    XCTAssertNil(scrollViewNode.view);

    valdiContext.rootValdiView = rootView;

    XCTAssertEqual(rootView, rootViewNode.view);

    [valdiContext destroy];
}

- (void)testDeferredRootViewDoesntDestroyContext
{
    __block id<SCValdiContextProtocol> valdiContext;

    @autoreleasepool {
        [self getRuntimeWithBlock:^(id<SCValdiRuntimeProtocol> runtime) {
            SCCValdiTestViewModel *viewModel = [[SCCValdiTestViewModel alloc] initWithHeaderTitle:@"Hello World!" scrollable:NO entries:@[]];
            SCCValdiTestContext *componentContext = [[SCCValdiTestContext alloc] initWithListener:[SCCValdiTestListenerImpl new] onTap:^(double index) {}];
            valdiContext = [runtime createContextWithViewClass:[SCCValdiTestIntegrationTests class] viewModel:viewModel componentContext:componentContext];
        }];
    }

    [valdiContext waitUntilRenderCompletedSyncWithFlush:YES];

    CGSize layoutSize = CGSizeMake(400, 800);

    [valdiContext setLayoutSize:layoutSize direction:SCValdiLayoutDirectionLTR];

    id<SCValdiViewNodeProtocol> rootViewNode = valdiContext.rootViewNode;
    id<SCValdiViewNodeProtocol> scrollViewNode = [[rootViewNode children] firstObject];

    XCTAssertNotNil(rootViewNode);
    XCTAssertNotNil(scrollViewNode);

    XCTAssertNil(rootViewNode.view);
    XCTAssertNil(scrollViewNode.view);

    @autoreleasepool {
        SCValdiRootView *rootView = [[SCValdiRootView alloc] initWithoutValdiContext];
        rootView.enableViewInflationWhenInvisible = YES;
        rootView.frame = CGRectMake(0, 0, layoutSize.width, layoutSize.height);

        valdiContext.rootValdiView = rootView;

        XCTAssertEqual(rootView, rootViewNode.view);
        XCTAssertNotNil(scrollViewNode.view);
    }

    XCTAssertFalse(valdiContext.destroyed);
}

- (void)testGetAllModuleHashes
{
    // There should be no hashes before loading any view
    NSDictionary<NSString*, NSString*>* hashesBeforeLoadingView = [self.runtimeManager.mainRuntime getAllModuleHashes];
    XCTAssertEqual(hashesBeforeLoadingView.count, 0);

    // Load a view
    __block SCCValdiTestIntegrationTests *rootView;
    [self getRuntimeWithBlock:^(id<SCValdiRuntimeProtocol> runtime) {
        SCCValdiTestViewModel *viewModel = [[SCCValdiTestViewModel alloc] initWithHeaderTitle:@"Hello World!" scrollable:YES entries:@[]];
        SCCValdiTestContext *componentContext = [[SCCValdiTestContext alloc] initWithListener:[SCCValdiTestListenerImpl new] onTap:^(double index) {}];
        rootView = [[SCCValdiTestIntegrationTests alloc] initWithViewModel:viewModel componentContext:componentContext runtime:self.runtime];
    }];
    [rootView.valdiContext waitUntilRenderCompletedSyncWithFlush:YES];
    rootView.frame = CGRectMake(0, 0, 400, 800);
    [rootView layoutIfNeeded];

    // Check that the valdi_test module is loaded
    NSDictionary<NSString*, NSString*>* hashes = [self.runtimeManager.mainRuntime getAllModuleHashes];
    XCTAssertGreaterThan(hashes.count, 0);
    XCTAssertNotNil(hashes[@"valdi_test"]);
}

// Test clearCaches behavior in rasterizeImage of SCValdiImageFilter.mm
// clearCaches should be called less often than rasterizeImage, such that
// clearCaches should only trigger if there were no recent calls to postprocessImage
//
// Make multiple calls to rasterizeImage (through postprocessImage) and check:
// 1) clearCaches was called at least once
// 2) clearCaches call count is less than rasterizeImage call count
- (void)testClearCIContextCache
{
    // Programmatically create a dummy image for postprocessImage
    Valdi::Ref<Valdi::ImageFilter> filter = Valdi::makeShared<Valdi::ImageFilter>();
    UIImage *testUIImage = [self _generateImage];
    SCValdiImage *testValdiImage = [SCValdiImage imageWithUIImage:testUIImage];

    // postprocessImage needs to run on the same DispatchQueue thread
    // as caller or Valdi::DispatchQueue::getCurrent() will be invalid.
    // Create a local DispatchQueue for all calls to run on.
    Valdi::Ref<Valdi::DispatchQueue> queueRef = Valdi::makeShared<Valdi::GCDDispatchQueue>(
        STRING_LITERAL("testClearCIContextCache Queue"), Valdi::ThreadQoSClassNormal);

    // Use an AsyncGroup to wait for rasterizeImage's async calls to finish
    auto group = Valdi::makeShared<Valdi::AsyncGroup>();

    __block int clearCachesCallCount = 0;
    CIContext* mockCIContext = OCMClassMock([CIContext class]);
    OCMStub([mockCIContext clearCaches]).andDo(^(NSInvocation *invocation) {
            ++clearCachesCallCount;
            group->leave();
        });

    Valdi::DispatchFunction dispatchFn = [&testValdiImage, &filter, &mockCIContext, &group]() {
        ValdiIOS::postprocessImage(testValdiImage, filter, mockCIContext);
        group->enter();
    };

    static constexpr int arbitraryProcessCount = 5;
    for (int processQueued = 0; processQueued < arbitraryProcessCount; ++processQueued)
    {
        queueRef->sync(dispatchFn);
        // Simulate a delay between continuous rasterizeImage calls
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    // Wait for arbitrary time period longer than rasterizeImage's clear threshold
    // It is not necessary (nor should it be possible) for enter/leave count to match
    group->blockingWaitWithTimeout(std::chrono::seconds(5));

    XCTAssertGreaterThan(clearCachesCallCount, 0);
    XCTAssertLessThan(clearCachesCallCount, arbitraryProcessCount);
}

- (UIImage *)_generateImage
{
    UIColor *fillColor = [UIColor whiteColor];
    CGSize size = CGSizeMake(1, 1);
    UIGraphicsBeginImageContextWithOptions(size, YES, 0);
    CGContextRef context = UIGraphicsGetCurrentContext();
    [fillColor setFill];
    CGContextFillRect(context, CGRectMake(0, 0, 1, 1));
    UIImage *image = UIGraphicsGetImageFromCurrentImageContext();
    UIGraphicsEndImageContext();
    return image;
}

- (void)testInvokeWithJSRuntimeProvider
{
    XCTestExpectation *expectation = [self expectationWithDescription:@"invokeWithJSRuntimeProvider completes"];
    
    // Use getRuntimeUsingBatch:NO so we're not inside executeMainThreadBatch. Otherwise the runtime
    // may run the dispatched block synchronously on the main thread (ScheduleTypeDefault), and with
    // async_strict_mode the resolution (functionWithJSRuntime:) would assert for being on main thread.
    [self getRuntimeUsingBatch:NO withBlock:^(id<SCValdiRuntimeProtocol> runtime) {
        // Test the new invokeWithJSRuntimeProvider method
        [SCCValdiTestMakeTestObject invokeWithJSRuntimeProvider:^id<SCValdiJSRuntime> {
            return [runtime jsRuntime];
        } completionHandler:^(id<SCCValdiTestITestObject> testObject) {
            XCTAssertNotNil(testObject, @"Test object should not be nil");
            
            // Test that the object works and returns correct values
            double result1 = [testObject addWithValue:10.0];
            XCTAssertEqual(result1, 10.0, @"First add should return 10");
            
            double result2 = [testObject addWithValue:32.0];
            XCTAssertEqual(result2, 42.0, @"Second add should return 42 (10 + 32)");
            
            [expectation fulfill];
        }];
    }];
    
    [self waitForExpectations:@[expectation] timeout:5.0];
}

- (void)testResolvingExportedFunctionOnMainThreadTriggersAssertionFailure
{
    // valdi_test has async_strict_mode enabled. Resolving (functionWithJSRuntime:) from the main thread
    // must trigger an assertion (to avoid ANRs). NSAssert raises NSInternalInconsistencyException.
    XCTestExpectation *expectation = [self expectationWithDescription:@"resolution from main thread triggers assertion"];
    __block id<SCValdiRuntimeProtocol> capturedRuntime = nil;

    [self getRuntimeWithBlock:^(id<SCValdiRuntimeProtocol> runtime) {
        capturedRuntime = runtime;
    }];

    XCTAssertNotNil(capturedRuntime);
    dispatch_async(dispatch_get_main_queue(), ^{
        @try {
            (void)[SCCValdiTestMakeTestObject functionWithJSRuntime:[capturedRuntime jsRuntime]];
            XCTFail(@"Expected resolution from main thread to trigger an assertion (NSInternalInconsistencyException)");
        } @catch (NSException *exception) {
            XCTAssertTrue([exception.name isEqualToString:NSInternalInconsistencyException],
                         @"Expected NSInternalInconsistencyException, got %@", exception.name);
            [expectation fulfill];
        }
    });

    [self waitForExpectations:@[expectation] timeout:5.0];
}

- (void)testPrepareForPoolReuseResetsTransform
{
    // valdi_prepareForPoolReuse is called unconditionally by the pool infrastructure
    // on every recycled view. Verify it resets a stale CALayer transform — the
    // scenario that occurs when a transform animation is cancelled mid-flight:
    // the Valdi animation system sets the model value to the target before the
    // CAAnimation starts, so cancellation snaps the layer to that model value.
    UIView *view = [[UIView alloc] initWithFrame:CGRectMake(0, 0, 200, 44)];
    view.layer.transform = CATransform3DMakeTranslation(-216, 0, 0);
    XCTAssertFalse(CATransform3DIsIdentity(view.layer.transform));

    [view valdi_prepareForPoolReuse];

    XCTAssertTrue(CATransform3DIsIdentity(view.layer.transform));
}

- (void)testLabelAllowsPoolReentry
{
    SCValdiLabel *label = [[SCValdiLabel alloc] initWithFrame:CGRectMake(0, 0, 200, 44)];
    XCTAssertTrue([label willEnqueueIntoValdiPool]);
}

@end

#pragma clang diagnostic pop
