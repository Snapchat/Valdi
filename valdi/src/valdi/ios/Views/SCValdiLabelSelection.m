//
//  SCValdiLabelSelection.m
//  Valdi
//

#import "valdi/ios/Views/SCValdiLabelSelection.h"

#import "valdi_core/SCMacros.h"
#import "valdi_core/SCValdiError.h"
#import "valdi_core/SCValdiFunction.h"
#import "valdi_core/SCValdiLogger.h"
#import "valdi_core/SCValdiMarshaller.h"

static NSString *const SCValdiTextSelectionMenuTextKey = @"text";
static NSString *const SCValdiTextSelectionMenuSelectedTextKey = @"selectedText";
static NSString *const SCValdiTextSelectionMenuSelectionStartKey = @"selectionStart";
static NSString *const SCValdiTextSelectionMenuSelectionEndKey = @"selectionEnd";
NSString *const SCValdiTextSelectionMenuActionIDKey = @"id";
NSString *const SCValdiTextSelectionMenuActionTitleKey = @"title";

NSDictionary<NSString *, id> *SCValdiTextSelectionMenuEventForText(NSString *text, NSRange selectedRange)
{
    NSString *resolvedText = text ?: @"";
    NSUInteger textLength = resolvedText.length;
    NSUInteger selectionStart = selectedRange.location == NSNotFound ? 0 : MIN(selectedRange.location, textLength);
    NSUInteger selectionEnd = selectedRange.location == NSNotFound ? 0 : MIN(NSMaxRange(selectedRange), textLength);
    selectionEnd = MAX(selectionStart, selectionEnd);
    NSString *selectedText = selectionEnd > selectionStart
        ? [resolvedText substringWithRange:NSMakeRange(selectionStart, selectionEnd - selectionStart)]
        : @"";

    return @{
        SCValdiTextSelectionMenuTextKey: resolvedText,
        SCValdiTextSelectionMenuSelectedTextKey: selectedText,
        SCValdiTextSelectionMenuSelectionStartKey: @(selectionStart),
        SCValdiTextSelectionMenuSelectionEndKey: @(selectionEnd),
    };
}

static NSArray<NSDictionary<NSString *, NSString *> *> *SCValdiTextSelectionMenuActionsFromResult(id result)
{
    if (SCValdiIsNull(result)) {
        return @[];
    }

    NSArray *rawActions = ObjectAs(result, NSArray);
    if (!rawActions) {
        SCLogValdiError(@"Text selection menu provider must return an array");
        return @[];
    }

    NSMutableArray<NSDictionary<NSString *, NSString *> *> *actions = [NSMutableArray arrayWithCapacity:rawActions.count];
    for (id rawAction in rawActions) {
        NSDictionary *action = ObjectAs(rawAction, NSDictionary);
        NSString *actionID = ObjectAs(action[SCValdiTextSelectionMenuActionIDKey], NSString);
        NSString *title = ObjectAs(action[SCValdiTextSelectionMenuActionTitleKey], NSString);
        if (actionID.length == 0 || title.length == 0) {
            SCLogValdiError(@"Text selection menu actions require non-empty id and title strings");
            continue;
        }
        [actions addObject:@{
            SCValdiTextSelectionMenuActionIDKey: actionID,
            SCValdiTextSelectionMenuActionTitleKey: title,
        }];
    }
    return actions;
}

NSArray<NSDictionary<NSString *, NSString *> *> *SCValdiTextSelectionMenuActionsForProvider(
    id<SCValdiFunction> provider,
    NSDictionary<NSString *, id> *event)
{
    if (!provider) {
        return @[];
    }
    if (![provider respondsToSelector:@selector(performSyncWithMarshaller:propagatesError:)]) {
        SCLogValdiError(@"Text selection menu provider must be callable synchronously");
        return @[];
    }

    __block NSArray<NSDictionary<NSString *, NSString *> *> *menuActions = @[];
    SCValdiMarshallerScoped(marshaller, {
        SCValdiMarshallerPushUntyped(marshaller, event);
        if ([provider performSyncWithMarshaller:marshaller propagatesError:NO]) {
            @try {
                id result = SCValdiMarshallerGetUntyped(marshaller, -1);
                menuActions = SCValdiTextSelectionMenuActionsFromResult(result);
            } @catch (SCValdiError *error) {
                SCLogValdiError(@"Failed to unmarshall text selection menu actions: %@", error.reason);
            }
        }
    });
    return menuActions;
}

void SCValdiPerformTextSelectionMenuAction(
    id<SCValdiFunction> actionHandler,
    NSString *actionID,
    NSDictionary<NSString *, id> *event)
{
    if (!actionHandler) {
        return;
    }

    NSMutableDictionary<NSString *, id> *actionEvent = [event mutableCopy];
    actionEvent[SCValdiTextSelectionMenuActionIDKey] = actionID;
    SCValdiMarshallerScoped(marshaller, {
        SCValdiMarshallerPushUntyped(marshaller, actionEvent);
        [actionHandler performWithMarshaller:marshaller];
    });
}

@implementation SCValdiLabelTextPosition

+ (instancetype)positionWithOffset:(NSInteger)offset
{
    SCValdiLabelTextPosition *position = [SCValdiLabelTextPosition new];
    position->_offset = offset;
    return position;
}

@end

@implementation SCValdiLabelTextRange

+ (instancetype)rangeWithStartOffset:(NSInteger)startOffset endOffset:(NSInteger)endOffset
{
    SCValdiLabelTextRange *range = [SCValdiLabelTextRange new];
    NSInteger resolvedStartOffset = MIN(startOffset, endOffset);
    NSInteger resolvedEndOffset = MAX(startOffset, endOffset);
    range->_startPosition = [SCValdiLabelTextPosition positionWithOffset:resolvedStartOffset];
    range->_endPosition = [SCValdiLabelTextPosition positionWithOffset:resolvedEndOffset];
    return range;
}

+ (instancetype)rangeWithNSRange:(NSRange)range
{
    return [self rangeWithStartOffset:(NSInteger)range.location endOffset:(NSInteger)NSMaxRange(range)];
}

- (UITextPosition *)start
{
    return _startPosition;
}

- (UITextPosition *)end
{
    return _endPosition;
}

- (BOOL)isEmpty
{
    return _startPosition.offset == _endPosition.offset;
}

@end

@implementation SCValdiLabelSelectionRect

- (CGRect)rect
{
    return _valdiRect;
}

- (NSWritingDirection)writingDirection
{
    return _valdiWritingDirection;
}

- (BOOL)containsStart
{
    return _valdiContainsStart;
}

- (BOOL)containsEnd
{
    return _valdiContainsEnd;
}

- (BOOL)isVertical
{
    return _valdiIsVertical;
}

@end

@implementation SCValdiLabelSelectionState

- (instancetype)init
{
    self = [super init];
    if (self) {
        _selectedRange = NSMakeRange(0, 0);
        _selectionAffinity = UITextStorageDirectionForward;
    }
    return self;
}

@end
