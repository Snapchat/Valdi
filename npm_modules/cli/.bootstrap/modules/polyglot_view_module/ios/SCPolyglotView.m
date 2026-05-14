#import "SCPolyglotView.h"

@implementation SCPolyglotView

- (instancetype)initWithFrame:(CGRect)frame {
    self = [super initWithFrame:frame];
    if (self) {
        self.backgroundColor = [UIColor colorWithWhite:0.95 alpha:1];

        UILabel *label = [[UILabel alloc] init];
        label.text = @"Hello from iOS";
        label.translatesAutoresizingMaskIntoConstraints = NO;
        [self addSubview:label];

        [NSLayoutConstraint activateConstraints:@[
            [label.centerXAnchor constraintEqualToAnchor:self.centerXAnchor],
            [label.centerYAnchor constraintEqualToAnchor:self.centerYAnchor],
        ]];
    }
    return self;
}

@end
