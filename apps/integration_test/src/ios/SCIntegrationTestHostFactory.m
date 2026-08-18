#import "valdi/ios/Gestures/SCValdiGestureRecognizers.h"
#import "valdi/ios/SCValdiRuntimeManager.h"
#import "valdi_core/SCValdiAttributesBinderBase.h"
#import "valdi_core/SCValdiModuleFactoryRegistry.h"
#import "valdi_core/SCValdiRuntimeProtocol.h"
#import "valdi_core/SCValdiViewFactory.h"
#import "valdi_core/SCValdiViewNodeProtocol.h"
#import <SCCIntegrationTestAppTypes/SCCIntegrationTestAppTypes.h>
#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

@interface SCIntegrationTestHost : NSObject<SCCIntegrationTestAppIntegrationTestHostModule>
@end

@implementation SCIntegrationTestHost

- (NSString *)resolvePath:(NSString *)path
{
    if (path.isAbsolutePath) {
        return path;
    }

    NSArray<NSString *> *paths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES);
    NSString *documents = paths.firstObject ?: NSTemporaryDirectory();
    return [[documents stringByAppendingPathComponent:@"Valdi"] stringByAppendingPathComponent:path];
}

- (NSString *)getPlatform
{
    return @"ios";
}

- (NSString *)getOutputPath
{
    return @"valdi-integration-test/results.json";
}

- (void)markFinishedWithPath:(NSString *)path
{
    NSString *resolvedPath = [self resolvePath:path];
    NSString *directory = resolvedPath.stringByDeletingLastPathComponent;
    [NSFileManager.defaultManager createDirectoryAtPath:directory withIntermediateDirectories:YES attributes:nil error:nil];
    [@"done" writeToFile:resolvedPath atomically:YES encoding:NSUTF8StringEncoding error:nil];
}

- (void)writeTextFileWithPath:(NSString *)path contents:(NSString *)contents
{
    NSString *resolvedPath = [self resolvePath:path];
    NSString *directory = resolvedPath.stringByDeletingLastPathComponent;
    [NSFileManager.defaultManager createDirectoryAtPath:directory withIntermediateDirectories:YES attributes:nil error:nil];
    [contents writeToFile:resolvedPath atomically:YES encoding:NSUTF8StringEncoding error:nil];
}

- (NSString *)submitTouchSequenceWithNode:(id<SCValdiViewNodeProtocol>)node sequenceJson:(NSString *)sequenceJson
{
    UIView *view = node.view;
    if (!view) {
        return @"no backing UIView";
    }

    NSData *data = [sequenceJson dataUsingEncoding:NSUTF8StringEncoding];
    NSDictionary *request = data ? [NSJSONSerialization JSONObjectWithData:data options:0 error:nil] : nil;
    NSString *kind = [request[@"kind"] isKindOfClass:NSString.class] ? request[@"kind"] : @"tap";
    CGPoint location = CGPointMake(CGRectGetMidX(view.bounds), CGRectGetMidY(view.bounds));
    NSUInteger triggered = 0;

    for (UIGestureRecognizer *recognizer in view.gestureRecognizers) {
        if ([kind isEqualToString:@"doubleTap"] && [recognizer isKindOfClass:SCValdiFastDoubleTapGestureRecognizer.class]) {
            [(SCValdiFastDoubleTapGestureRecognizer *)recognizer triggerAtLocation:location forState:UIGestureRecognizerStateEnded];
            triggered++;
        } else if ([kind isEqualToString:@"longPress"] && [recognizer isKindOfClass:SCValdiLongPressGestureRecognizer.class]) {
            [(SCValdiLongPressGestureRecognizer *)recognizer triggerAtLocation:location forState:UIGestureRecognizerStateBegan];
            triggered++;
        } else if ([recognizer isKindOfClass:SCValdiTapGestureRecognizer.class]) {
            [(SCValdiTapGestureRecognizer *)recognizer triggerAtLocation:location forState:UIGestureRecognizerStateEnded];
            triggered++;
        }
    }

    if (triggered == 0) {
        return [NSString stringWithFormat:@"no triggerable recognizer for %@ on %@", kind, NSStringFromClass(view.class)];
    }
    return [NSString stringWithFormat:@"triggered %lu recognizer(s) for %@ on %@", (unsigned long)triggered, kind, NSStringFromClass(view.class)];
}

- (NSString *)focusTextInputWithNode:(id<SCValdiViewNodeProtocol>)node
{
    UIView *view = node.view;
    if ([view respondsToSelector:@selector(becomeFirstResponder)]) {
        [view becomeFirstResponder];
        return [NSString stringWithFormat:@"focused %@", NSStringFromClass(view.class)];
    }
    return @"target cannot become first responder";
}

- (NSString *)replaceTextWithNode:(id<SCValdiViewNodeProtocol>)node value:(NSString *)value
{
    UIView *view = node.view;
    if ([view isKindOfClass:UITextField.class]) {
        UITextField *textField = (UITextField *)view;
        textField.text = value;
        [textField sendActionsForControlEvents:UIControlEventEditingChanged];
        return [NSString stringWithFormat:@"set UITextField text length=%lu", (unsigned long)value.length];
    }
    if ([view isKindOfClass:UITextView.class]) {
        ((UITextView *)view).text = value;
        return [NSString stringWithFormat:@"set UITextView text length=%lu", (unsigned long)value.length];
    }
    return [NSString stringWithFormat:@"target is not editable text: %@", NSStringFromClass(view.class)];
}

- (NSString *)pressReturnWithNode:(id<SCValdiViewNodeProtocol>)node
{
    UIView *view = node.view;
    if ([view isKindOfClass:UITextField.class]) {
        UITextField *textField = (UITextField *)view;
        [textField sendActionsForControlEvents:UIControlEventEditingDidEndOnExit];
        return @"sent UITextField return";
    }
    return [NSString stringWithFormat:@"return key unsupported for %@", NSStringFromClass(view.class)];
}

- (NSString *)pressBackspaceWithNode:(id<SCValdiViewNodeProtocol>)node
{
    UIView *view = node.view;
    if ([view isKindOfClass:UITextField.class]) {
        UITextField *textField = (UITextField *)view;
        NSString *text = textField.text ?: @"";
        textField.text = text.length > 0 ? [text substringToIndex:text.length - 1] : text;
        [textField sendActionsForControlEvents:UIControlEventEditingChanged];
        return @"sent UITextField backspace";
    }
    if ([view isKindOfClass:UITextView.class]) {
        UITextView *textView = (UITextView *)view;
        NSString *text = textView.text ?: @"";
        textView.text = text.length > 0 ? [text substringToIndex:text.length - 1] : text;
        return @"sent UITextView backspace";
    }
    return [NSString stringWithFormat:@"backspace unsupported for %@", NSStringFromClass(view.class)];
}

@end

@interface SCIntegrationTestHostFactory : SCCIntegrationTestAppIntegrationTestHostModuleFactory
@end

@implementation SCIntegrationTestHostFactory

VALDI_REGISTER_MODULE()

- (id<SCCIntegrationTestAppIntegrationTestHostModule>)onLoadModule
{
    return [SCIntegrationTestHost new];
}

@end

@interface SCIntegrationTestFactoryView : UIView

@property (nonatomic, copy, nullable) NSString *factoryText;

@end

@implementation SCIntegrationTestFactoryView

- (void)setFactoryText:(NSString *)factoryText
{
    _factoryText = [factoryText copy];
    [self setNeedsDisplay];
}

- (void)drawRect:(CGRect)rect
{
    CGFloat centerY = CGRectGetMidY(rect);

    UIBezierPath *hexagon = [UIBezierPath bezierPath];
    [hexagon moveToPoint:CGPointMake(44, centerY - 26)];
    [hexagon addLineToPoint:CGPointMake(66, centerY - 13)];
    [hexagon addLineToPoint:CGPointMake(66, centerY + 13)];
    [hexagon addLineToPoint:CGPointMake(44, centerY + 26)];
    [hexagon addLineToPoint:CGPointMake(22, centerY + 13)];
    [hexagon addLineToPoint:CGPointMake(22, centerY - 13)];
    [hexagon closePath];
    [[UIColor colorWithRed:79.0 / 255.0 green:70.0 / 255.0 blue:229.0 / 255.0 alpha:1] setFill];
    [hexagon fill];

    UIBezierPath *sparkle = [UIBezierPath bezierPath];
    [sparkle moveToPoint:CGPointMake(44, centerY - 16)];
    [sparkle addLineToPoint:CGPointMake(49, centerY - 5)];
    [sparkle addLineToPoint:CGPointMake(60, centerY)];
    [sparkle addLineToPoint:CGPointMake(49, centerY + 5)];
    [sparkle addLineToPoint:CGPointMake(44, centerY + 16)];
    [sparkle addLineToPoint:CGPointMake(39, centerY + 5)];
    [sparkle addLineToPoint:CGPointMake(28, centerY)];
    [sparkle addLineToPoint:CGPointMake(39, centerY - 5)];
    [sparkle closePath];
    [UIColor.whiteColor setFill];
    [sparkle fill];

    UIFont *font = [UIFont systemFontOfSize:16 weight:UIFontWeightSemibold];
    CGRect textRect = CGRectMake(82, floor(centerY - font.lineHeight / 2), CGRectGetWidth(rect) - 82, font.lineHeight);
    [self.factoryText drawInRect:textRect withAttributes:@{
        NSFontAttributeName: font,
        NSForegroundColorAttributeName: [UIColor colorWithRed:23.0 / 255.0 green:37.0 / 255.0 blue:84.0 / 255.0 alpha:1],
    }];
}

@end

@interface SCFactoryIntegrationHost : NSObject<SCCIntegrationTestAppFactoryIntegrationHostModule>
@end

@implementation SCFactoryIntegrationHost

- (id<SCValdiViewFactory>)createIntegrationViewFactory
{
    id<SCValdiRuntimeProtocol> runtime = SCValdiRuntimeManager.allRuntimeManagers.firstObject.mainRuntime;
    NSAssert(runtime != nil, @"The integration app must have an active Valdi runtime");

    return [runtime makeViewFactoryWithBlock:^UIView *{
        SCIntegrationTestFactoryView *view = [SCIntegrationTestFactoryView new];
        view.opaque = NO;
        view.accessibilityIdentifier = @"integration-factory-view";
        return view;
    } attributesBinder:^(id<SCValdiAttributesBinderProtocol> binder) {
        [binder bindAttribute:@"factoryText"
            invalidateLayoutOnChange:NO
                     withStringBlock:^BOOL(SCIntegrationTestFactoryView *view, NSString *value, id<SCValdiAnimatorProtocol> animator) {
                         view.factoryText = value;
                         return YES;
                     }
                          resetBlock:^(SCIntegrationTestFactoryView *view, id<SCValdiAnimatorProtocol> animator) {
                              view.factoryText = nil;
                          }];
    } forClass:SCIntegrationTestFactoryView.class];
}

@end

@interface SCFactoryIntegrationHostFactory : SCCIntegrationTestAppFactoryIntegrationHostModuleFactory
@end

@implementation SCFactoryIntegrationHostFactory

VALDI_REGISTER_MODULE()

- (id<SCCIntegrationTestAppFactoryIntegrationHostModule>)onLoadModule
{
    return [SCFactoryIntegrationHost new];
}

@end
