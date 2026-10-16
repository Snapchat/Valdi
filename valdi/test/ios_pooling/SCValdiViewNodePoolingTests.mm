//
//  SCValdiViewNodePoolingTests.mm
//  Valdi
//
//  The layout callback of SCValdiViewNode used to read the UIView through the public -view accessor,
//  which disables pooling. Every view with a didFinishLayout block (borderRadius, boxShadow, gradients,
//  masks, shape paths) was therefore never recycled.
//

#import <XCTest/XCTest.h>

#import "valdi/ios/CPPBindings/UIViewHolder.h"
#import "valdi/ios/SCValdiViewNode+CPP.h"

#include "ViewNodeTestsUtils.hpp"

@interface SCValdiViewNodePoolingTests : XCTestCase
@end

@implementation SCValdiViewNodePoolingTests {
    std::unique_ptr<ValdiTest::ViewNodeTestsDependencies> _deps;
    Valdi::Ref<Valdi::ViewNode> _node;
    Valdi::Ref<ValdiIOS::UIViewHolder> _holder;
    UIView* _uiView;
}

- (void)setUp {
    [super setUp];
    _deps = std::make_unique<ValdiTest::ViewNodeTestsDependencies>();
    _node = _deps->createView();
    _uiView = [UIView new];
    _holder = ValdiIOS::UIViewHolder::makeWithUIView(_uiView);
    _node->setView(_deps->getViewTransactionScope(), _holder, nullptr);
}

- (void)tearDown {
    // The standalone test transaction can only pool StandaloneViews; keep it from trying with ours.
    _holder->setCanBeReused(false);
    _node->setView(_deps->getViewTransactionScope(), nullptr, nullptr);
    _node = nullptr;
    _holder = nullptr;
    _deps.reset();
    [super tearDown];
}

- (void)testLayoutCallbackRunsBlocksWithoutDisablingPooling {
    SCValdiViewNode* wrapper = [[SCValdiViewNode alloc] initWithViewNode:_node.get()];
    __block NSInteger calls = 0;
    __block UIView* receivedView = nil;
    [wrapper
        setDidFinishLayoutBlock:^(UIView* view, id<SCValdiAnimatorProtocol> animator) {
            calls++;
            receivedView = view;
        }
                         forKey:@"uiview-borderradius"];

    XCTAssertTrue(_holder->canBeReused());
    for (int i = 0; i < 3; i++) {
        [wrapper didApplyLayoutWithAnimator:nil];
    }

    XCTAssertEqual(calls, 3);
    XCTAssertEqual(receivedView, _uiView);
    XCTAssertTrue(_holder->canBeReused(), @"A layout callback must not exclude the view from the view pool");
}

- (void)testAccessibilityFallbacksDoNotDisablePooling {
    SCValdiViewNode* wrapper = [[SCValdiViewNode alloc] initWithViewNode:_node.get()];
    _uiView.accessibilityIdentifier = @"native-id";

    // Declared by SCValdiViewNode's implementation, not its public header.
    id node = wrapper;
    XCTAssertEqualObjects([node accessibilityIdentifier], @"native-id");
    (void)[node accessibilityTraits];
    (void)[node accessibilityValue];

    XCTAssertTrue(_holder->canBeReused());
}

- (void)testKillswitchMakesLayoutCallbackAndAccessibilityDisablePooling {
    _deps->getTree().setKeepsViewPoolingOnLayoutCallbacks(false);
    SCValdiViewNode* wrapper = [[SCValdiViewNode alloc] initWithViewNode:_node.get()];
    [wrapper
        setDidFinishLayoutBlock:^(UIView* view, id<SCValdiAnimatorProtocol> animator) {
        }
                         forKey:@"uiview-borderradius"];

    [wrapper didApplyLayoutWithAnimator:nil];
    XCTAssertFalse(_holder->canBeReused(), @"With the killswitch on, layout callbacks restore the old behavior");

    _holder->setCanBeReused(true);
    id node = wrapper;
    (void)[node accessibilityIdentifier];
    XCTAssertFalse(_holder->canBeReused(), @"With the killswitch on, accessibility fallbacks restore the old behavior");
}

- (void)testDetachedNodeLayoutCallbackDisablesPooling {
    _node->setViewNodeTree(nullptr);
    SCValdiViewNode* wrapper = [[SCValdiViewNode alloc] initWithViewNode:_node.get()];
    [wrapper
        setDidFinishLayoutBlock:^(UIView* view, id<SCValdiAnimatorProtocol> animator) {
        }
                         forKey:@"uiview-borderradius"];

    [wrapper didApplyLayoutWithAnimator:nil];
    XCTAssertFalse(_holder->canBeReused(), @"Without a tree the killswitch cannot be read, so keep the old behavior");
}

- (void)testPublicViewAccessorStillDisablesPooling {
    SCValdiViewNode* wrapper = [[SCValdiViewNode alloc] initWithViewNode:_node.get()];

    XCTAssertEqual(wrapper.view, _uiView);
    XCTAssertFalse(_holder->canBeReused(), @"External callers may retain the UIView, so it must leave the pool");
}

@end
