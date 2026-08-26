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

typedef NS_ENUM(NSInteger, SCValdiMacOSTextInputUnfocusReason) {
    SCValdiMacOSTextInputUnfocusReasonUnknown = 0,
    SCValdiMacOSTextInputUnfocusReasonReturnKeyPress = 1,
    SCValdiMacOSTextInputUnfocusReasonDismissKeyPress = 2,
};

@interface SCValdiMacOSTextField() <NSTextFieldDelegate>
@end

@interface NSView (SCValdiMacOSNativeAttributeState)
- (void)valdi_didChangeValue:(id)value forAttribute:(NSString *)attributeName;
@end

@implementation SCValdiMacOSTextField {
    NSColor *_placeholderColor;
    BOOL _placeholderDirty;
    NSString *_placeholderText;
    SCValdiMacOSFunction *_onChange;
    SCValdiMacOSFunction *_onWillChange;
    SCValdiMacOSFunction *_onEditBegin;
    SCValdiMacOSFunction *_onEditEnd;
    SCValdiMacOSFunction *_onReturn;
    SCValdiMacOSFunction *_onWillDelete;
    NSRange _pendingSelection;
    BOOL _hasPendingSelection;
    BOOL _selectTextOnFocus;
    BOOL _closesWhenReturnKeyPressed;
    BOOL _shouldBecomeFocusedWhenAttached;
    __weak NSText *_observedEditor;
    SCValdiMacOSTextInputUnfocusReason _lastUnfocusReason;
}

- (instancetype)initWithFrame:(NSRect)frameRect
{
    self = [super initWithFrame:frameRect];

    if (self) {
        self.delegate = self;
        self.bezeled = NO;
        self.drawsBackground = NO;
        _closesWhenReturnKeyPressed = YES;
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

- (NSDictionary *)_editTextEvent
{
    NSText *editor = self.currentEditor;
    NSString *text = editor.string ?: self.stringValue ?: @"";
    NSRange selection = editor != nil ? editor.selectedRange : NSMakeRange(text.length, 0);
    NSUInteger selectionStart = MIN(selection.location, text.length);
    NSUInteger selectionEnd = MIN(NSMaxRange(selection), text.length);
    return @{
        @"text": text,
        @"selectionStart": @(selectionStart),
        @"selectionEnd": @(selectionEnd),
    };
}

- (void)_notifyValueAndSelectionChangedWithEvent:(NSDictionary *)event
{
    NSString *text = event[@"text"];
    NSUInteger textLength = [text isKindOfClass:NSString.class] ? text.length : 0;
    NSUInteger selectionStart = MIN([event[@"selectionStart"] unsignedIntegerValue], textLength);
    NSUInteger selectionEnd = MIN([event[@"selectionEnd"] unsignedIntegerValue], textLength);
    if (selectionEnd < selectionStart) {
        selectionEnd = selectionStart;
    }
    _pendingSelection = NSMakeRange(selectionStart, selectionEnd - selectionStart);
    _hasPendingSelection = YES;
    [self valdi_didChangeValue:event[@"text"] forAttribute:@"value"];
    [self valdi_didChangeValue:@[event[@"selectionStart"], event[@"selectionEnd"]] forAttribute:@"selection"];
}

- (void)_notifyValueAndSelectionChanged
{
    [self _notifyValueAndSelectionChangedWithEvent:[self _editTextEvent]];
}

- (void)_notifySelectionChanged:(NSNotification *)notification
{
    NSText *editor = self.currentEditor;
    if (editor == nil || notification.object != editor) {
        return;
    }
    NSDictionary *event = [self _editTextEvent];
    NSUInteger selectionStart = [event[@"selectionStart"] unsignedIntegerValue];
    NSUInteger selectionEnd = [event[@"selectionEnd"] unsignedIntegerValue];
    _pendingSelection = NSMakeRange(selectionStart, selectionEnd - selectionStart);
    _hasPendingSelection = YES;
    [self valdi_didChangeValue:@[event[@"selectionStart"], event[@"selectionEnd"]] forAttribute:@"selection"];
}

- (NSDictionary *)_editTextEndEvent
{
    NSMutableDictionary *event = [[self _editTextEvent] mutableCopy];
    event[@"reason"] = @(_lastUnfocusReason);
    return event;
}

- (void)_submitEventToFunction:(SCValdiMacOSFunction *)func
{
    if (!func) {
        return;
    }

    [func performWithParameters:@[[self _editTextEvent]]];
}

- (void)_submitEditEndEventWithBaseEvent:(NSDictionary *)baseEvent
{
    if (!_onEditEnd) {
        return;
    }

    NSMutableDictionary *event = [baseEvent mutableCopy];
    event[@"reason"] = @(_lastUnfocusReason);
    [_onEditEnd performWithParameters:@[event]];
}

- (void)textDidChange:(NSNotification *)notification
{
    [super textDidChange:notification];

    if (_onWillChange) {
        id replacement = [_onWillChange performWithParametersAndReturnValue:@[[self _editTextEvent]]];
        if ([replacement isKindOfClass:NSDictionary.class]) {
            NSString *replacementText = replacement[@"text"];
            NSNumber *selectionStartValue = replacement[@"selectionStart"];
            NSNumber *selectionEndValue = replacement[@"selectionEnd"];
            if ([replacementText isKindOfClass:NSString.class] &&
                [selectionStartValue isKindOfClass:NSNumber.class] &&
                [selectionEndValue isKindOfClass:NSNumber.class]) {
                NSText *editor = self.currentEditor;
                self.stringValue = replacementText;
                editor.string = replacementText;
                NSUInteger selectionStart = (NSUInteger)MAX(
                    0, MIN((NSInteger)replacementText.length, selectionStartValue.integerValue));
                NSUInteger selectionEnd = (NSUInteger)MAX(
                    0, MIN((NSInteger)replacementText.length, selectionEndValue.integerValue));
                if (selectionEnd < selectionStart) {
                    selectionEnd = selectionStart;
                }
                editor.selectedRange = NSMakeRange(selectionStart, selectionEnd - selectionStart);
            }
        }
    }
    [self _notifyValueAndSelectionChanged];
    [self _submitEventToFunction:_onChange];
}

- (void)textDidBeginEditing:(NSNotification *)notification
{
    [super textDidBeginEditing:notification];

    _lastUnfocusReason = SCValdiMacOSTextInputUnfocusReasonUnknown;
    NSText *editor = self.currentEditor;
    if (editor != nil) {
        _observedEditor = editor;
        [[NSNotificationCenter defaultCenter] addObserver:self
                                                 selector:@selector(_notifySelectionChanged:)
                                                     name:NSTextViewDidChangeSelectionNotification
                                                   object:editor];
        if (_selectTextOnFocus) {
            editor.selectedRange = NSMakeRange(0, self.stringValue.length);
        } else if (_hasPendingSelection) {
            editor.selectedRange = _pendingSelection;
        }
    }
    [self valdi_didChangeValue:@YES forAttribute:@"focused"];
    [self _notifyValueAndSelectionChanged];
    [self _submitEventToFunction:_onEditBegin];
}

- (void)textDidEndEditing:(NSNotification *)notification
{
    NSDictionary *finalEvent = [self _editTextEvent];
    [super textDidEndEditing:notification];

    NSInteger textMovement = [notification.userInfo[NSTextMovementUserInfoKey] integerValue];
    if (textMovement == NSReturnTextMovement) {
        _lastUnfocusReason = SCValdiMacOSTextInputUnfocusReasonReturnKeyPress;
    } else if (textMovement == NSCancelTextMovement) {
        _lastUnfocusReason = SCValdiMacOSTextInputUnfocusReasonDismissKeyPress;
    }
    [[NSNotificationCenter defaultCenter] removeObserver:self
                                                    name:NSTextViewDidChangeSelectionNotification
                                                  object:_observedEditor];
    _observedEditor = nil;
    [self _notifyValueAndSelectionChangedWithEvent:finalEvent];
    [self valdi_didChangeValue:@NO forAttribute:@"focused"];
    [self _submitEditEndEventWithBaseEvent:finalEvent];
    _lastUnfocusReason = SCValdiMacOSTextInputUnfocusReasonUnknown;
}

- (void)dealloc
{
    [[NSNotificationCenter defaultCenter] removeObserver:self];
}

- (void)viewDidMoveToWindow
{
    [super viewDidMoveToWindow];
    if (_shouldBecomeFocusedWhenAttached && self.window != nil) {
        _shouldBecomeFocusedWhenAttached = NO;
        [self.window makeFirstResponder:self];
    }
}

- (BOOL)control:(NSControl *)control textView:(NSTextView *)textView doCommandBySelector:(SEL)commandSelector
{
    if (commandSelector == @selector(insertNewline:)) {
        if (_closesWhenReturnKeyPressed) {
            _lastUnfocusReason = SCValdiMacOSTextInputUnfocusReasonReturnKeyPress;
            [self.window makeFirstResponder:nil];
        }
        [self _submitEventToFunction:_onReturn];
        return YES;
    }
    if (commandSelector == @selector(cancelOperation:)) {
        _lastUnfocusReason = SCValdiMacOSTextInputUnfocusReasonDismissKeyPress;
        return NO;
    }
    if (commandSelector == @selector(deleteBackward:) || commandSelector == @selector(deleteForward:)) {
        [self _submitEventToFunction:_onWillDelete];
    }
    return NO;
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

- (void)valdi_setEditable:(id)editable
{
    self.editable = editable == nil || [editable boolValue];
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
    NSString *text = [value isKindOfClass:NSString.class] ? value : @"";
    self.stringValue = text;
    NSText *editor = self.currentEditor;
    if (editor != nil) {
        NSRange previousSelection = editor.selectedRange;
        editor.string = text;
        NSUInteger selectionStart = MIN(previousSelection.location, text.length);
        NSUInteger selectionEnd = MIN(NSMaxRange(previousSelection), text.length);
        if (selectionEnd < selectionStart) {
            selectionEnd = selectionStart;
        }
        editor.selectedRange = NSMakeRange(selectionStart, selectionEnd - selectionStart);
    }
    if (_hasPendingSelection) {
        NSUInteger selectionStart = MIN(_pendingSelection.location, text.length);
        NSUInteger selectionEnd = MIN(NSMaxRange(_pendingSelection), text.length);
        if (selectionEnd < selectionStart) {
            selectionEnd = selectionStart;
        }
        _pendingSelection = NSMakeRange(selectionStart, selectionEnd - selectionStart);
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

- (void)valdi_setOnWillChange:(id)value
{
    _onWillChange = value;
}

- (void)valdi_setOnReturn:(id)value
{
    _onReturn = value;
}

- (void)valdi_setOnWillDelete:(id)value
{
    _onWillDelete = value;
}

- (void)valdi_setFocused:(id)value
{
    BOOL focused = value != nil && [value boolValue];
    if (focused) {
        if (self.window != nil) {
            [self.window makeFirstResponder:self];
        } else {
            _shouldBecomeFocusedWhenAttached = YES;
        }
    } else {
        _shouldBecomeFocusedWhenAttached = NO;
        if (self.currentEditor != nil) {
            [self.window makeFirstResponder:nil];
        }
    }
}

- (void)valdi_setSelection:(id)value
{
    _hasPendingSelection = NO;
    if (![value isKindOfClass:[NSArray class]] || [value count] < 2) {
        return;
    }
    NSUInteger textLength = self.stringValue.length;
    NSUInteger selectionStart = (NSUInteger)MAX(0, MIN((NSInteger)textLength, [value[0] integerValue]));
    NSUInteger selectionEnd = (NSUInteger)MAX(0, MIN((NSInteger)textLength, [value[1] integerValue]));
    if (selectionEnd < selectionStart) {
        selectionEnd = selectionStart;
    }
    _pendingSelection = NSMakeRange(selectionStart, selectionEnd - selectionStart);
    _hasPendingSelection = YES;
    NSText *editor = self.currentEditor;
    if (editor != nil) {
        editor.selectedRange = _pendingSelection;
    }
}

- (void)valdi_setSelectTextOnFocus:(id)value
{
    _selectTextOnFocus = value != nil && [value boolValue];
}

- (void)valdi_setClosesWhenReturnKeyPressed:(id)value
{
    _closesWhenReturnKeyPressed = value == nil || [value boolValue];
}

- (void)valdi_setPlaceholder:(id)placeholder
{
    _placeholderText = placeholder;
    [self _invalidatePlaceholder];
}

+ (void)bindAttributes:(SCValdiMacOSAttributesBinder *)attributesBinder
{
    [attributesBinder bindUntypedAttribute:@"enabled" invalidateLayoutOnChange:NO selector:@selector(valdi_setEnabled:)];
    [attributesBinder bindUntypedAttribute:@"editable" invalidateLayoutOnChange:NO selector:@selector(valdi_setEditable:)];
    [attributesBinder bindUntypedAttribute:@"font" invalidateLayoutOnChange:YES selector:@selector(valdi_setFont:)];
    [attributesBinder bindUntypedAttribute:@"value" invalidateLayoutOnChange:NO selector:@selector(valdi_setValue:)];
    [attributesBinder bindUntypedAttribute:@"placeholder" invalidateLayoutOnChange:NO selector:@selector(valdi_setPlaceholder:)];
    [attributesBinder bindUntypedAttribute:@"onEditBegin" invalidateLayoutOnChange:NO selector:@selector(valdi_setEditBegin:)];
    [attributesBinder bindUntypedAttribute:@"onEditEnd" invalidateLayoutOnChange:NO selector:@selector(valdi_setEditEnd:)];
    [attributesBinder bindUntypedAttribute:@"onChange" invalidateLayoutOnChange:NO selector:@selector(valdi_setOnChange:)];
    [attributesBinder bindUntypedAttribute:@"onWillChange" invalidateLayoutOnChange:NO selector:@selector(valdi_setOnWillChange:)];
    [attributesBinder bindUntypedAttribute:@"onReturn" invalidateLayoutOnChange:NO selector:@selector(valdi_setOnReturn:)];
    [attributesBinder bindUntypedAttribute:@"onWillDelete" invalidateLayoutOnChange:NO selector:@selector(valdi_setOnWillDelete:)];
    [attributesBinder bindUntypedAttribute:@"focused" invalidateLayoutOnChange:NO selector:@selector(valdi_setFocused:)];
    [attributesBinder bindUntypedAttribute:@"selection" invalidateLayoutOnChange:NO selector:@selector(valdi_setSelection:)];
    [attributesBinder bindUntypedAttribute:@"selectTextOnFocus" invalidateLayoutOnChange:NO selector:@selector(valdi_setSelectTextOnFocus:)];
    [attributesBinder bindUntypedAttribute:@"closesWhenReturnKeyPressed" invalidateLayoutOnChange:NO selector:@selector(valdi_setClosesWhenReturnKeyPressed:)];

    [attributesBinder bindColorAttribute:@"color" invalidateLayoutOnChange:NO selector:@selector(valdi_setColor:)];
    [attributesBinder bindColorAttribute:@"placeholderColor" invalidateLayoutOnChange:NO selector:@selector(valdi_setPlaceholderColor:)];
    [attributesBinder bindColorAttribute:@"tintColor" invalidateLayoutOnChange:NO selector:@selector(valdi_setTintColor:)];
}

@end
