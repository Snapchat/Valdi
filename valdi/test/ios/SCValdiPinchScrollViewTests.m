#import <UIKit/UIKit.h>
#import <XCTest/XCTest.h>
#import <objc/runtime.h>

#import "valdi/ios/Gestures/SCValdiGestureRecognizers.h"
#import "valdi_core/SCValdiConfiguration.h"
#import "valdi_core/SCValdiScrollView.h"

static const void *kStateOverrideKey = &kStateOverrideKey;

@interface UIGestureRecognizer (SCValdiPinchScrollTesting)
- (UIGestureRecognizerState)scValdiPinchScrollTesting_state;
@end

@implementation UIGestureRecognizer (SCValdiPinchScrollTesting)
- (UIGestureRecognizerState)scValdiPinchScrollTesting_state
{
    NSNumber *override = objc_getAssociatedObject(self, kStateOverrideKey);
    return override ? (UIGestureRecognizerState)override.integerValue : [self scValdiPinchScrollTesting_state];
}
@end

@interface SCValdiPinchGestureRecognizer (SCValdiPinchScrollTesting)
- (void)_updateHandoffWithEvent:(UIEvent *)event;
@end

@interface SCValdiFakeTouch : UITouch
@property (nonatomic, assign) UITouchPhase fakePhase;
@property (nonatomic, assign) CGPoint fakeLocation;
@end

@implementation SCValdiFakeTouch
- (UITouchPhase)phase
{
    return self.fakePhase;
}
- (CGPoint)locationInView:(UIView *)view
{
    return self.fakeLocation;
}
@end

@interface SCValdiFakeEvent : UIEvent
@property (nonatomic, copy) NSSet<UITouch *> *fakeTouches;
@end

@implementation SCValdiFakeEvent
- (NSSet<UITouch *> *)touchesForGestureRecognizer:(UIGestureRecognizer *)gesture
{
    return self.fakeTouches;
}
@end

@interface SCValdiDragSpyDelegate : NSObject <UIScrollViewDelegate>
@property (nonatomic, strong) NSMutableArray<NSString *> *log;
@end

@implementation SCValdiDragSpyDelegate
- (instancetype)init
{
    self = [super init];
    if (self) {
        _log = [NSMutableArray array];
    }
    return self;
}
- (void)scrollViewWillBeginDragging:(UIScrollView *)scrollView
{
    [self.log addObject:@"begin"];
}
- (void)scrollViewWillEndDragging:(UIScrollView *)scrollView
                     withVelocity:(CGPoint)velocity
              targetContentOffset:(inout CGPoint *)targetContentOffset
{
    [self.log addObject:@"willEnd"];
}
- (void)scrollViewDidEndDragging:(UIScrollView *)scrollView willDecelerate:(BOOL)decelerate
{
    [self.log addObject:@"didEnd"];
}
@end

@interface SCValdiPinchScrollViewTests : XCTestCase
@end

@implementation SCValdiPinchScrollViewTests {
    BOOL _originalEnabled;
}

- (void)setUp
{
    [super setUp];
    _originalEnabled = [SCValdiConfiguration scrollPinchWhileDraggingEnabled];
    method_exchangeImplementations(class_getInstanceMethod([UIGestureRecognizer class], @selector(state)),
                                   class_getInstanceMethod([UIGestureRecognizer class],
                                                           @selector(scValdiPinchScrollTesting_state)));
}

- (void)tearDown
{
    method_exchangeImplementations(class_getInstanceMethod([UIGestureRecognizer class], @selector(state)),
                                   class_getInstanceMethod([UIGestureRecognizer class],
                                                           @selector(scValdiPinchScrollTesting_state)));
    [SCValdiConfiguration setScrollPinchWhileDraggingEnabled:_originalEnabled];
    [super tearDown];
}

- (BOOL)pinch:(SCValdiPinchGestureRecognizer *)pinch recognizesSimultaneouslyWith:(UIGestureRecognizer *)other
{
    return [pinch.delegate gestureRecognizer:pinch shouldRecognizeSimultaneouslyWithGestureRecognizer:other];
}

- (void)testScrollPinchWhileDraggingIsDisabledByDefault
{
    XCTAssertFalse([SCValdiConfiguration scrollPinchWhileDraggingEnabled]);
}

- (void)testPinchRecognizesSimultaneouslyWithOwnScrollViewPan
{
    [SCValdiConfiguration setScrollPinchWhileDraggingEnabled:YES];
    SCValdiScrollView *scrollView = [[SCValdiScrollView alloc] initWithFrame:CGRectMake(0, 0, 320, 480)];
    SCValdiPinchGestureRecognizer *pinch = [[SCValdiPinchGestureRecognizer alloc] init];
    [scrollView addGestureRecognizer:pinch];

    XCTAssertTrue([self pinch:pinch recognizesSimultaneouslyWith:scrollView.innerScrollView.panGestureRecognizer]);
}

- (void)testPinchDoesNotRecognizeSimultaneouslyWithUnrelatedPan
{
    [SCValdiConfiguration setScrollPinchWhileDraggingEnabled:YES];
    SCValdiScrollView *scrollView = [[SCValdiScrollView alloc] initWithFrame:CGRectMake(0, 0, 320, 480)];
    SCValdiPinchGestureRecognizer *pinch = [[SCValdiPinchGestureRecognizer alloc] init];
    [scrollView addGestureRecognizer:pinch];
    UIScrollView *otherScrollView = [[UIScrollView alloc] initWithFrame:CGRectMake(0, 0, 320, 480)];

    XCTAssertFalse([self pinch:pinch recognizesSimultaneouslyWith:otherScrollView.panGestureRecognizer]);
}

- (void)testPinchDoesNotRecognizeSimultaneouslyWithOwnScrollViewPanWhenDisabled
{
    [SCValdiConfiguration setScrollPinchWhileDraggingEnabled:NO];
    SCValdiScrollView *scrollView = [[SCValdiScrollView alloc] initWithFrame:CGRectMake(0, 0, 320, 480)];
    SCValdiPinchGestureRecognizer *pinch = [[SCValdiPinchGestureRecognizer alloc] init];
    [scrollView addGestureRecognizer:pinch];

    XCTAssertFalse([self pinch:pinch recognizesSimultaneouslyWith:scrollView.innerScrollView.panGestureRecognizer]);
}

- (void)testBoundPinchLimitsOwnScrollPanToOneTouchAndRestoresIt
{
    [SCValdiConfiguration setScrollPinchWhileDraggingEnabled:YES];
    SCValdiScrollView *scrollView = [[SCValdiScrollView alloc] initWithFrame:CGRectMake(0, 0, 320, 480)];
    NSUInteger original = scrollView.innerScrollView.panGestureRecognizer.maximumNumberOfTouches;

    SCValdiSetScrollPanLimitedForPinch(scrollView, YES);
    XCTAssertEqual(scrollView.innerScrollView.panGestureRecognizer.maximumNumberOfTouches, 1u);

    SCValdiSetScrollPanLimitedForPinch(scrollView, NO);
    XCTAssertEqual(scrollView.innerScrollView.panGestureRecognizer.maximumNumberOfTouches, original);
}

- (void)testScrollPanKeepsItsTouchLimitWhenDisabled
{
    [SCValdiConfiguration setScrollPinchWhileDraggingEnabled:NO];
    SCValdiScrollView *scrollView = [[SCValdiScrollView alloc] initWithFrame:CGRectMake(0, 0, 320, 480)];
    NSUInteger original = scrollView.innerScrollView.panGestureRecognizer.maximumNumberOfTouches;

    SCValdiSetScrollPanLimitedForPinch(scrollView, YES);

    XCTAssertEqual(scrollView.innerScrollView.panGestureRecognizer.maximumNumberOfTouches, original);
}

- (void)testScrollPanLimitIsRestoredWhenTheSwitchTurnsOffAfterBinding
{
    [SCValdiConfiguration setScrollPinchWhileDraggingEnabled:YES];
    SCValdiScrollView *scrollView = [[SCValdiScrollView alloc] initWithFrame:CGRectMake(0, 0, 320, 480)];
    NSUInteger original = scrollView.innerScrollView.panGestureRecognizer.maximumNumberOfTouches;
    SCValdiSetScrollPanLimitedForPinch(scrollView, YES);

    [SCValdiConfiguration setScrollPinchWhileDraggingEnabled:NO];
    SCValdiSetScrollPanLimitedForPinch(scrollView, YES);

    XCTAssertEqual(scrollView.innerScrollView.panGestureRecognizer.maximumNumberOfTouches, original);
}

#pragma mark - Handoff

- (SCValdiFakeTouch *)touchAt:(CGPoint)location phase:(UITouchPhase)phase
{
    SCValdiFakeTouch *touch = [[SCValdiFakeTouch alloc] init];
    touch.fakeLocation = location;
    touch.fakePhase = phase;
    return touch;
}

- (SCValdiFakeEvent *)eventWithTouches:(NSArray<UITouch *> *)touches
{
    SCValdiFakeEvent *event = [[SCValdiFakeEvent alloc] init];
    event.fakeTouches = [NSSet setWithArray:touches];
    return event;
}

- (void)setPanState:(UIGestureRecognizerState)state onScrollView:(SCValdiScrollView *)scrollView
{
    objc_setAssociatedObject(scrollView.innerScrollView.panGestureRecognizer, kStateOverrideKey, @(state),
                             OBJC_ASSOCIATION_RETAIN_NONATOMIC);
}

- (void)drainMainQueue
{
    [[NSRunLoop mainRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.1]];
}

// Drives the scroll pan from dragging with two fingers down to one finger left, and returns the finger left.
- (SCValdiFakeTouch *)startHandoffOn:(SCValdiPinchGestureRecognizer *)pinch
                          scrollView:(SCValdiScrollView *)scrollView
{
    SCValdiFakeTouch *dragging = [self touchAt:CGPointMake(100, 100) phase:UITouchPhaseMoved];
    SCValdiFakeTouch *remaining = [self touchAt:CGPointMake(200, 100) phase:UITouchPhaseMoved];
    [self setPanState:UIGestureRecognizerStateChanged onScrollView:scrollView];
    [pinch _updateHandoffWithEvent:[self eventWithTouches:@[ dragging, remaining ]]];

    [self setPanState:UIGestureRecognizerStatePossible onScrollView:scrollView];
    dragging.fakePhase = UITouchPhaseEnded;
    [pinch _updateHandoffWithEvent:[self eventWithTouches:@[ dragging, remaining ]]];
    [self drainMainQueue];
    return remaining;
}

- (SCValdiScrollView *)handoffScrollViewWithSpy:(SCValdiDragSpyDelegate *)spy
{
    SCValdiScrollView *scrollView = [[SCValdiScrollView alloc] initWithFrame:CGRectMake(0, 0, 320, 480)];
    scrollView.innerScrollView.contentSize = CGSizeMake(2000, 480);
    scrollView.innerScrollView.delegate = spy;
    return scrollView;
}

- (void)testHandoffPairsDragCallbacksWhenTheRemainingFingerLifts
{
    [SCValdiConfiguration setScrollPinchWhileDraggingEnabled:YES];
    SCValdiDragSpyDelegate *spy = [[SCValdiDragSpyDelegate alloc] init];
    SCValdiScrollView *scrollView = [self handoffScrollViewWithSpy:spy];
    SCValdiPinchGestureRecognizer *pinch = [[SCValdiPinchGestureRecognizer alloc] init];
    [scrollView addGestureRecognizer:pinch];

    SCValdiFakeTouch *remaining = [self startHandoffOn:pinch scrollView:scrollView];
    XCTAssertEqualObjects(spy.log, (@[ @"begin" ]));

    remaining.fakePhase = UITouchPhaseEnded;
    [pinch _updateHandoffWithEvent:[self eventWithTouches:@[ remaining ]]];

    XCTAssertEqualObjects(spy.log, (@[ @"begin", @"willEnd", @"didEnd" ]));
}

- (void)testHandoffEndsWhenASecondFingerComesDown
{
    [SCValdiConfiguration setScrollPinchWhileDraggingEnabled:YES];
    SCValdiDragSpyDelegate *spy = [[SCValdiDragSpyDelegate alloc] init];
    SCValdiScrollView *scrollView = [self handoffScrollViewWithSpy:spy];
    SCValdiPinchGestureRecognizer *pinch = [[SCValdiPinchGestureRecognizer alloc] init];
    [scrollView addGestureRecognizer:pinch];
    SCValdiFakeTouch *remaining = [self startHandoffOn:pinch scrollView:scrollView];

    SCValdiFakeTouch *second = [self touchAt:CGPointMake(50, 100) phase:UITouchPhaseBegan];
    [pinch _updateHandoffWithEvent:[self eventWithTouches:@[ remaining, second ]]];
    CGPoint offset = scrollView.innerScrollView.contentOffset;
    remaining.fakeLocation = CGPointMake(260, 100);
    [pinch _updateHandoffWithEvent:[self eventWithTouches:@[ remaining, second ]]];

    XCTAssertEqualObjects(spy.log, (@[ @"begin", @"willEnd", @"didEnd" ]));
    XCTAssertTrue(CGPointEqualToPoint(scrollView.innerScrollView.contentOffset, offset));
}

- (void)testCancelledScrollPanDoesNotStartAHandoff
{
    [SCValdiConfiguration setScrollPinchWhileDraggingEnabled:YES];
    SCValdiDragSpyDelegate *spy = [[SCValdiDragSpyDelegate alloc] init];
    SCValdiScrollView *scrollView = [self handoffScrollViewWithSpy:spy];
    SCValdiPinchGestureRecognizer *pinch = [[SCValdiPinchGestureRecognizer alloc] init];
    [scrollView addGestureRecognizer:pinch];
    SCValdiFakeTouch *dragging = [self touchAt:CGPointMake(100, 100) phase:UITouchPhaseMoved];
    SCValdiFakeTouch *remaining = [self touchAt:CGPointMake(200, 100) phase:UITouchPhaseMoved];
    [self setPanState:UIGestureRecognizerStateChanged onScrollView:scrollView];
    [pinch _updateHandoffWithEvent:[self eventWithTouches:@[ dragging, remaining ]]];

    [self setPanState:UIGestureRecognizerStateCancelled onScrollView:scrollView];
    dragging.fakePhase = UITouchPhaseCancelled;
    [pinch _updateHandoffWithEvent:[self eventWithTouches:@[ dragging, remaining ]]];
    [self drainMainQueue];

    XCTAssertEqualObjects(spy.log, (@[]));
}

- (void)testPagingScrollViewDoesNotStartAHandoff
{
    [SCValdiConfiguration setScrollPinchWhileDraggingEnabled:YES];
    SCValdiDragSpyDelegate *spy = [[SCValdiDragSpyDelegate alloc] init];
    SCValdiScrollView *scrollView = [self handoffScrollViewWithSpy:spy];
    scrollView.innerScrollView.pagingEnabled = YES;
    SCValdiPinchGestureRecognizer *pinch = [[SCValdiPinchGestureRecognizer alloc] init];
    [scrollView addGestureRecognizer:pinch];

    [self startHandoffOn:pinch scrollView:scrollView];

    XCTAssertEqualObjects(spy.log, (@[]));
}

@end
