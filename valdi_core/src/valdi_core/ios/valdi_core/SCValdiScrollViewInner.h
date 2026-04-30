
#import <UIKit/UIKit.h>

@interface SCValdiScrollViewInner : UIScrollView

@property (assign, nonatomic) BOOL horizontalScroll;
@property (assign, nonatomic) BOOL bouncesFromDragAtStart;
@property (assign, nonatomic) BOOL bouncesFromDragAtEnd;
@property (assign, nonatomic) BOOL panGestureRecognizerEnabled;
@property (assign, nonatomic) BOOL stopScrollingOnTouch;

@end
