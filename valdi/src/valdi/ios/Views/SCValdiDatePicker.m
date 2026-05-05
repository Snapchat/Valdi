//
//  SCValdiDatePicker.m
//  Valdi
//
//  Created by Brandon Francis on 3/9/19.
//

#import "valdi/ios/Views/SCValdiDatePicker.h"
#import "valdi/ios/Views/SCValdiDatePickerUtils.h"

#import "valdi/ios/Categories/UIView+Valdi.h"

@implementation SCValdiDatePicker {
    id<SCValdiFunction> _Nullable _onChange;
}

- (instancetype)initWithFrame:(CGRect)frame {
    self = [super initWithFrame:frame];
    if (self) {
        self.datePickerMode = UIDatePickerModeDate;
        SCValdiFixIOS14DatePicker(self);
        [self addTarget:self action:@selector(_handleOnChange) forControlEvents:UIControlEventValueChanged];
    }
    return self;
}

- (CGSize)sizeThatFits:(CGSize)size
{
    if (@available(iOS 13.4, *)) {
        if (self.preferredDatePickerStyle == UIDatePickerStyleCompact) {
            CGSize fittingSize = [self systemLayoutSizeFittingSize:UILayoutFittingCompressedSize
                                    withHorizontalFittingPriority:UILayoutPriorityFittingSizeLevel
                                          verticalFittingPriority:UILayoutPriorityFittingSizeLevel];
            if (fittingSize.width > 0 && fittingSize.height > 0) {
                return fittingSize;
            }
        }
    }
    return [super sizeThatFits:size];
}

- (CGPoint)convertPoint:(CGPoint)point fromView:(UIView *)view
{
    return [self valdi_convertPoint:point fromView:view];
}

- (CGPoint)convertPoint:(CGPoint)point toView:(UIView *)view
{
    return [self valdi_convertPoint:point toView:view];
}

- (UIView *)hitTest:(CGPoint)point withEvent:(UIEvent *)event
{
    if (self.valdiHitTest != nil) {
        return [self valdi_hitTest:point withEvent:event withCustomHitTest:self.valdiHitTest];
    }
    return [super hitTest:point withEvent:event];
}

#pragma mark - UIView+Valdi

- (BOOL)willEnqueueIntoValdiPool {
    return NO;
}

#pragma mark - Action handling methods

INTERNED_STRING_CONST("dateSeconds", SCValdiDatePickerDateInSecondsKey);

#pragma mark - Internal methods

// DO NOT USE - @mli6 - temporary workaround pending release of iOS dark mode
- (BOOL)valdi_setTextColor:(UIColor *)color
{
    [self setValue:color forKey:@"textColor"];
    [self setValue:@(NO) forKey:@"highlightsToday"];

    return YES;
}

- (void)valdi_setOnChange:(id<SCValdiFunction>)onChange
{
    _onChange = onChange;
}

- (void)_handleOnChange
{
    NSTimeInterval dateSeconds = self.date.timeIntervalSince1970;
    [self.valdiContext didChangeValue:@(dateSeconds)
            forInternedValdiAttribute:SCValdiDatePickerDateInSecondsKey()
                              inViewNode:self.valdiViewNode];
    
    if (!_onChange) {
        return;
    }
    
    // pseudocode: _onChange.call({ "dateSeconds": dateSeconds })
    SCValdiMarshallerScoped(marshaller, {
        NSInteger objectIndex = SCValdiMarshallerPushMap(marshaller, 1);
        SCValdiMarshallerPushDouble(marshaller, dateSeconds);
        SCValdiMarshallerPutMapProperty(marshaller, SCValdiDatePickerDateInSecondsKey(), objectIndex);
        
        [_onChange performWithMarshaller:marshaller];
    });
}

#pragma mark - Static methods

+ (void)bindAttributes:(id<SCValdiAttributesBinderProtocol>)attributesBinder {
    [attributesBinder bindAttribute:@"dateSeconds"
           invalidateLayoutOnChange:NO
                    withDoubleBlock:^BOOL(SCValdiDatePicker *view, CGFloat attributeValue, id<SCValdiAnimatorProtocol> animator) {
        NSDate *date = [NSDate dateWithTimeIntervalSince1970:attributeValue];
        BOOL animated = animator != nil;
        [view setDate:date animated:animated];
        return YES;
    } resetBlock:^(SCValdiDatePicker *view, id<SCValdiAnimatorProtocol> animator) {
        BOOL animated = animator != nil;
        [view setDate:[NSDate date] animated:animated];
    }];

    [attributesBinder bindAttribute:@"minimumDateSeconds"
           invalidateLayoutOnChange:NO
                    withDoubleBlock:^BOOL(SCValdiDatePicker *view, CGFloat attributeValue, id<SCValdiAnimatorProtocol> animator) {
        NSDate *minDate = [NSDate dateWithTimeIntervalSince1970:attributeValue];
        view.minimumDate = minDate;
        return YES;
    } resetBlock:^(SCValdiDatePicker *view, id<SCValdiAnimatorProtocol> animator) {
        view.minimumDate = nil;
    }];

    [attributesBinder bindAttribute:@"maximumDateSeconds"
           invalidateLayoutOnChange:NO
                    withDoubleBlock:^BOOL(SCValdiDatePicker *view, CGFloat attributeValue, id<SCValdiAnimatorProtocol> animator) {
        NSDate *maxDate = [NSDate dateWithTimeIntervalSince1970:attributeValue];
        view.maximumDate = maxDate;
        return YES;
    } resetBlock:^(SCValdiDatePicker *view, id<SCValdiAnimatorProtocol> animator) {
        view.maximumDate = nil;
    }];

    [attributesBinder bindAttribute:@"onChange"
                  withFunctionBlock:^(SCValdiDatePicker *view, id<SCValdiFunction> attributeValue) {
        [view valdi_setOnChange:attributeValue];
    }
                         resetBlock:^(SCValdiDatePicker *view) {
        [view valdi_setOnChange:nil];
    }];

    [attributesBinder bindAttribute:@"color"
           invalidateLayoutOnChange:NO
                     withColorBlock:^BOOL(SCValdiDatePicker *view, UIColor *attributeValue, id<SCValdiAnimatorProtocol> animator) {
        return [view valdi_setTextColor:attributeValue];
    }
                         resetBlock:^(SCValdiDatePicker *view, id<SCValdiAnimatorProtocol> animator) {
        [view valdi_setTextColor:nil];
    }];

    [attributesBinder bindAttribute:@"preferredStyle"
           invalidateLayoutOnChange:YES
                       withIntBlock:^BOOL(SCValdiDatePicker *view, NSInteger attributeValue, id<SCValdiAnimatorProtocol> animator) {
        SCValdiSetDatePickerStyle(view, attributeValue);
        return YES;
    } resetBlock:^(SCValdiDatePicker *view, id<SCValdiAnimatorProtocol> animator) {
        SCValdiSetDatePickerStyle(view, 1 /* wheels */);
    }];

    [attributesBinder setPlaceholderViewMeasureDelegate:^UIView *{
        return [SCValdiDatePicker new];
    }];
}

@end
