#import "SCPolyglotView.h"

@implementation SCPolyglotView

- (instancetype)initWithFrame:(NSRect)frameRect {
    self = [super initWithFrame:frameRect];
    if (self) {
        self.wantsLayer = YES;
        self.layer.backgroundColor = [[NSColor colorWithWhite:0.93 alpha:1] CGColor];

        NSTextField *label = [NSTextField labelWithString:@"Hello from macOS"];
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
