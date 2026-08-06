#import <XCTest/XCTest.h>

#import "valdi/ios/Views/SCValdiTextLayoutView.h"

@interface SCValdiTextViewRetainCycleTests : XCTestCase
@end

@implementation SCValdiTextViewRetainCycleTests

- (void)testTextLayoutViewDeallocatesAfterDisplayLinkCreated
{
    // If CADisplayLink holds a strong reference to the view (old bug), the view cannot
    // be deallocated and weakView remains non-nil after the autorelease pool drains.
    __weak SCValdiTextLayoutView *weakView = nil;
    @autoreleasepool {
        SCValdiTextLayoutView *view = [[SCValdiTextLayoutView alloc]
                                        initWithFrame:CGRectMake(0, 0, 100, 100)
                                        usesEffectsLayoutManager:NO];
        // Triggers _startAnimatedTextDisplayLinkIfNeeded, creating the CADisplayLink
        [view invalidateAnimatedTextProgress];
        weakView = view;
    }
    XCTAssertNil(weakView,
                 @"SCValdiTextLayoutView should deallocate after going out of scope; "
                 @"non-nil indicates a CADisplayLink retain cycle");
}

@end
