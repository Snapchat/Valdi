//
//  SCValdiLabelSelection.h
//  Valdi
//

#import <UIKit/UIKit.h>

NS_ASSUME_NONNULL_BEGIN

@protocol SCValdiFunction;

FOUNDATION_EXPORT NSString *const SCValdiTextSelectionMenuActionIDKey;
FOUNDATION_EXPORT NSString *const SCValdiTextSelectionMenuActionTitleKey;

@interface SCValdiLabelTextPosition : UITextPosition

@property (nonatomic, assign, readonly) NSInteger offset;

+ (instancetype)positionWithOffset:(NSInteger)offset;

@end

@interface SCValdiLabelTextRange : UITextRange

@property (nonatomic, strong, readonly) SCValdiLabelTextPosition *startPosition;
@property (nonatomic, strong, readonly) SCValdiLabelTextPosition *endPosition;

+ (instancetype)rangeWithStartOffset:(NSInteger)startOffset endOffset:(NSInteger)endOffset;
+ (instancetype)rangeWithNSRange:(NSRange)range;

@end

@interface SCValdiLabelSelectionRect : UITextSelectionRect

@property (nonatomic, assign) CGRect valdiRect;
@property (nonatomic, assign) NSWritingDirection valdiWritingDirection;
@property (nonatomic, assign) BOOL valdiContainsStart;
@property (nonatomic, assign) BOOL valdiContainsEnd;
@property (nonatomic, assign) BOOL valdiIsVertical;

@end

@interface SCValdiLabelSelectionState : NSObject

@property (nonatomic, assign) BOOL selectable;
@property (nonatomic, assign) NSRange selectedRange;
@property (nonatomic, strong, nullable) UITextInteraction *selectionInteraction API_AVAILABLE(ios(13.0));
@property (nonatomic, copy, nullable) NSArray<id<UIInteraction>> *selectionInstalledInteractions;
@property (nonatomic, strong, nullable) UIView *selectionInteractionOverlayView;
@property (nonatomic, strong, nullable) id<UITextInputTokenizer> tokenizer;
@property (nonatomic, copy, nullable) NSDictionary<NSAttributedStringKey, id> *markedTextStyle;
@property (nonatomic, weak, nullable) id<UITextInputDelegate> inputDelegate;
@property (nonatomic, assign) UITextStorageDirection selectionAffinity;
@property (nonatomic, strong, nullable) id<SCValdiFunction> onSelectionChange;
@property (nonatomic, strong, nullable) id<SCValdiFunction> onTextSelectionMenu;
@property (nonatomic, strong, nullable) id<SCValdiFunction> onTextSelectionMenuAction;

@end

NSDictionary<NSString *, id> *SCValdiTextSelectionMenuEventForText(NSString *_Nullable text, NSRange selectedRange);
NSArray<NSDictionary<NSString *, NSString *> *> *SCValdiTextSelectionMenuActionsForProvider(
    id<SCValdiFunction> _Nullable provider,
    NSDictionary<NSString *, id> *event);
void SCValdiPerformTextSelectionMenuAction(
    id<SCValdiFunction> _Nullable actionHandler,
    NSString *actionID,
    NSDictionary<NSString *, id> *event);

NS_ASSUME_NONNULL_END
