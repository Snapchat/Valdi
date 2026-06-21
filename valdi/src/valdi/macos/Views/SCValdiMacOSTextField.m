//
//  SCValdiMacOSTextField.m
//  valdi-macos
//
//  Created by Simon Corsin on 10/13/20.
//

#import "SCValdiMacOSTextField.h"
#import "valdi/macos/SCValdiMacOSFunction.h"

static NSFont *SCValdiResolveFont(NSString *str) {
    if (![str isKindOfClass:[NSString class]]) {
        return nil;
    }

    NSArray *components = [str componentsSeparatedByString:@" "];
    if (components.count < 2) {
        return nil;
    }

    NSString *fontName = components[0];
    CGFloat fontSize = [components[1] doubleValue];
    BOOL isItalic = [fontName hasSuffix:@"-italic"];
    NSFont *font = nil;

    if ([fontName isEqualToString:@"system"]) {
        font = [NSFont systemFontOfSize:fontSize];
    } else if ([fontName isEqualToString:@"system-medium"] || [fontName isEqualToString:@"system-medium-italic"]) {
        font = [NSFont systemFontOfSize:fontSize weight:NSFontWeightMedium];
    } else if ([fontName isEqualToString:@"system-semibold"] ||
               [fontName isEqualToString:@"system-demi-bold"] ||
               [fontName isEqualToString:@"system-semibold-italic"] ||
               [fontName isEqualToString:@"system-demi-bold-italic"]) {
        font = [NSFont systemFontOfSize:fontSize weight:NSFontWeightSemibold];
    } else if ([fontName isEqualToString:@"system-bold"] || [fontName isEqualToString:@"system-bold-italic"]) {
        font = [NSFont boldSystemFontOfSize:fontSize];
    } else if ([fontName isEqualToString:@"system-italic"]) {
        font = [NSFont systemFontOfSize:fontSize];
    } else {
        font = [NSFont fontWithName:fontName size:fontSize];
    }

    if (isItalic && font != nil) {
        font = [[NSFontManager sharedFontManager] convertFont:font toHaveTrait:NSItalicFontMask] ?: font;
    }
    return font;
}

static NSColor *SCValdiResolveColor(NSNumber *color) {
    if (![color isKindOfClass:[NSNumber class]]) {
        return nil;
    }

    long value = [color longValue];
    CGFloat r = (CGFloat)((value >> 24) & 0xFF) / 255.0;
    CGFloat g = (CGFloat)((value >> 16) & 0xFF) / 255.0;
    CGFloat b = (CGFloat)((value >> 8) & 0xFF) / 255.0;
    CGFloat a = (CGFloat)(value & 0xFF) / 255.0;

    return [NSColor colorWithRed:r green:g blue:b alpha:a];
}

@interface SCValdiMacOSTextField() <NSTextFieldDelegate>
@end

@implementation SCValdiMacOSTextField {
    NSColor *_placeholderColor;
    BOOL _placeholderDirty;
    NSString *_placeholderText;
    SCValdiMacOSFunction *_onChange;
    SCValdiMacOSFunction *_onWillChange;
    SCValdiMacOSFunction *_onEditBegin;
    SCValdiMacOSFunction *_onEditEnd;
}

- (instancetype)initWithFrame:(NSRect)frameRect
{
    self = [super initWithFrame:frameRect];

    if (self) {
        self.delegate = self;
        self.bezeled = NO;
        self.drawsBackground = NO;
    }

    return self;
}

- (void)layout
{
    [self _updatePlaceholderIfNeeded];

    [super layout];
}

- (NSSize)fittingSize
{
    [self _updatePlaceholderIfNeeded];
    return [super fittingSize];
}

- (void)_updatePlaceholderIfNeeded
{
    if (_placeholderDirty) {
        _placeholderDirty = NO;
        if (!_placeholderText.length) {
            self.placeholderAttributedString = nil;
        } else {
            NSColor *placeholderColor = _placeholderColor;
            if (!placeholderColor) {
                placeholderColor = [NSColor grayColor];
            }
            id font = self.font;
            if (!font) {
                font = [NSNull null];
            }

            self.placeholderAttributedString = [[NSAttributedString alloc] initWithString:_placeholderText attributes:@{
                NSForegroundColorAttributeName: placeholderColor,
                NSFontAttributeName: font,
            }];
        }
    }
}

- (void)_submitEventToFuntion:(SCValdiMacOSFunction *)func
{
    if (!func) {
        return;
    }

    [func performWithParameters:@[@{@"text": self.stringValue}]];
}

- (void)textDidChange:(NSNotification *)notification
{
    [super textDidChange:notification];

    [self _submitEventToFuntion:_onChange];
}

- (void)textDidBeginEditing:(NSNotification *)notification
{
    [super textDidBeginEditing:notification];

    [self _submitEventToFuntion:_onEditBegin];
}

- (void)textDidEndEditing:(NSNotification *)notification
{
    [super textDidEndEditing:notification];

    [self _submitEventToFuntion:_onEditEnd];
}

- (void)_invalidatePlaceholder
{
    _placeholderDirty = YES;
    [self setNeedsLayout:YES];
}

- (void)valdi_setEnabled:(id)enabled
{
    self.enabled = [enabled boolValue];
}

- (void)valdi_setFont:(id)font
{
    NSFont *nsFont = SCValdiResolveFont(font);
    if (!nsFont) {
        nsFont = [NSFont systemFontOfSize:[NSFont systemFontSize]];
    }
    self.font = nsFont;
    [self _invalidatePlaceholder];
}

- (void)valdi_setValue:(id)value
{
    if (value) {
        [self setStringValue:value];
    } else {
        [self setStringValue:@""];
    }
}

- (void)valdi_setColor:(id)color
{
    self.textColor = SCValdiResolveColor(color);
}

- (void)valdi_setPlaceholderColor:(id)color
{
    _placeholderColor = SCValdiResolveColor(color);
    [self _invalidatePlaceholder];
}

- (void)valdi_setTintColor:(id)color
{
}

- (void)valdi_setEditBegin:(id)value
{
    _onEditBegin = value;
}

- (void)valdi_setEditEnd:(id)value
{
    _onEditEnd = value;
}

- (void)valdi_setOnChange:(id)value
{
    _onChange = value;
}

// TODO implement the actual behaviour of onWillChange
// ios example:
// ../../ios/Views/SCValdiTextField.m#L130
- (void)valdi_setOnWillChange:(id)value
{
    _onWillChange = value;
}

- (void)valdi_setPlaceholder:(id)placeholder
{
    _placeholderText = placeholder;
    [self _invalidatePlaceholder];
}

+ (void)bindAttributes:(SCValdiMacOSAttributesBinder *)attributesBinder
{
    [attributesBinder bindUntypedAttribute:@"enabled" invalidateLayoutOnChange:NO selector:@selector(valdi_setEnabled:)];
    [attributesBinder bindUntypedAttribute:@"font" invalidateLayoutOnChange:YES selector:@selector(valdi_setFont:)];
    [attributesBinder bindUntypedAttribute:@"value" invalidateLayoutOnChange:NO selector:@selector(valdi_setValue:)];
    [attributesBinder bindUntypedAttribute:@"placeholder" invalidateLayoutOnChange:NO selector:@selector(valdi_setPlaceholder:)];
    [attributesBinder bindUntypedAttribute:@"onEditBegin" invalidateLayoutOnChange:NO selector:@selector(valdi_setEditBegin:)];
    [attributesBinder bindUntypedAttribute:@"onEditEnd" invalidateLayoutOnChange:NO selector:@selector(valdi_setEditEnd:)];
    [attributesBinder bindUntypedAttribute:@"onChange" invalidateLayoutOnChange:NO selector:@selector(valdi_setOnChange:)];
    [attributesBinder bindUntypedAttribute:@"onWillChange" invalidateLayoutOnChange:NO selector:@selector(valdi_setOnWillChange:)];

    [attributesBinder bindColorAttribute:@"color" invalidateLayoutOnChange:NO selector:@selector(valdi_setColor:)];
    [attributesBinder bindColorAttribute:@"placeholderColor" invalidateLayoutOnChange:NO selector:@selector(valdi_setPlaceholderColor:)];
    [attributesBinder bindColorAttribute:@"tintColor" invalidateLayoutOnChange:NO selector:@selector(valdi_setTintColor:)];
}

@end
