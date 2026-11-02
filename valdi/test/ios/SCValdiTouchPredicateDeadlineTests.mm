//
//  SCValdiTouchPredicateDeadlineTests.mm
//  ios_tests
//
//  Hit tests and gesture shouldBegin predicates are answered by JS synchronously on the main thread.
//  These drive every such call site against a function whose JS thread is stalled and assert each
//  one gives up within the input deadline and answers NO, rather than freezing the main thread until
//  JS drains.
//

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>
#import <XCTest/XCTest.h>

#import "valdi/ios/Gestures/SCValdiGestureRecognizers.h"
#import "valdi_core/SCValdiFunctionWithCPPFunction+CPP.h"
#import "valdi_core/SCValdiTouches.h"

#include "valdi_core/cpp/Utils/ValueFunction.hpp"

#include <chrono>
#include <future>

namespace {

// Long enough that an unbounded wait is unmistakable, short enough that a regressed call site fails
// the elapsed-time assertion instead of hanging the bundle.
constexpr auto kStall = std::chrono::seconds(3);
constexpr NSTimeInterval kCallSiteBoundSeconds = 1.5;

// Stands in for a JS-backed predicate whose JS thread is busy: an unbounded sync call parks until the
// stall ends, while a deadline-bounded call gives up at its deadline, as JS functions do.
class StalledJsPredicate : public Valdi::ValueFunction {
public:
    StalledJsPredicate() : _released(_release.get_future().share()) {}

    void release() {
        _release.set_value();
    }

    Valdi::Value operator()(const Valdi::ValueFunctionCallContext& /*callContext*/) noexcept override {
        _released.wait_for(kStall);
        return Valdi::Value(true);
    }

    std::string_view getFunctionType() const override {
        return "stalled JS predicate";
    }

protected:
    Valdi::Result<Valdi::Value> callSyncWithDeadline(const std::chrono::steady_clock::time_point& deadline,
                                                     Valdi::Value* /*parameters*/,
                                                     size_t /*size*/,
                                                     Valdi::SyncCallTimeoutPolicy /*timeoutPolicy*/) noexcept override {
        if (_released.wait_until(deadline) != std::future_status::ready) {
            return Valdi::Error("JS function timeout");
        }
        return Valdi::Value(true);
    }

private:
    std::promise<void> _release;
    std::shared_future<void> _released;
};

} // namespace

@interface SCValdiTouchPredicateDeadlineTests : XCTestCase
@end

@implementation SCValdiTouchPredicateDeadlineTests {
    Valdi::Ref<StalledJsPredicate> _stalledFunction;
    SCValdiFunctionWithCPPFunction* _predicate;
    UIView* _view;
}

- (void)setUp
{
    [super setUp];
    _stalledFunction = Valdi::makeShared<StalledJsPredicate>();
    _predicate = [[SCValdiFunctionWithCPPFunction alloc] initWithCPPFunction:_stalledFunction];
    _view = [[UIView alloc] initWithFrame:CGRectMake(0, 0, 100, 100)];
}

- (void)tearDown
{
    _stalledFunction->release();
    [super tearDown];
}

- (void)assertShouldBeginReturnsNoWithinDeadline:(UIGestureRecognizer<SCValdiGestureRecognizer>*)recognizer
{
    [_view addGestureRecognizer:recognizer];
    [recognizer setPredicate:_predicate];

    NSDate* start = [NSDate date];
    BOOL shouldBegin = [recognizer gestureRecognizerShouldBegin:recognizer];
    NSTimeInterval elapsed = -[start timeIntervalSinceNow];

    XCTAssertLessThan(elapsed, kCallSiteBoundSeconds, @"%@ shouldBegin blocked the main thread", [recognizer class]);
    XCTAssertFalse(shouldBegin, @"%@ shouldBegin should drop the gesture when JS misses the deadline",
                   [recognizer class]);
}

- (void)testHitTestReturnsNoWithinDeadlineWhileJsThreadIsStalled
{
    NSDate* start = [NSDate date];
    BOOL hit = SCValdiCallHitTestActionWithUIEventAndView(_predicate, CGPointMake(10, 10), nil, _view);
    NSTimeInterval elapsed = -[start timeIntervalSinceNow];

    XCTAssertLessThan(elapsed, kCallSiteBoundSeconds);
    XCTAssertFalse(hit);
}

- (void)testTapShouldBeginReturnsNoWithinDeadlineWhileJsThreadIsStalled
{
    [self assertShouldBeginReturnsNoWithinDeadline:[[SCValdiTapGestureRecognizer alloc] init]];
}

- (void)testDoubleTapShouldBeginReturnsNoWithinDeadlineWhileJsThreadIsStalled
{
    [self assertShouldBeginReturnsNoWithinDeadline:[[SCValdiFastDoubleTapGestureRecognizer alloc] init]];
}

- (void)testLongPressShouldBeginReturnsNoWithinDeadlineWhileJsThreadIsStalled
{
    [self assertShouldBeginReturnsNoWithinDeadline:[[SCValdiLongPressGestureRecognizer alloc] init]];
}

- (void)testDragShouldBeginReturnsNoWithinDeadlineWhileJsThreadIsStalled
{
    [self assertShouldBeginReturnsNoWithinDeadline:[[SCValdiDragGestureRecognizer alloc] init]];
}

- (void)testPinchShouldBeginReturnsNoWithinDeadlineWhileJsThreadIsStalled
{
    [self assertShouldBeginReturnsNoWithinDeadline:[[SCValdiPinchGestureRecognizer alloc] init]];
}

- (void)testRotateShouldBeginReturnsNoWithinDeadlineWhileJsThreadIsStalled
{
    [self assertShouldBeginReturnsNoWithinDeadline:[[SCValdiRotationGestureRecognizer alloc] init]];
}

@end
