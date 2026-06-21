//
//  SCValdiTextLayoutView.h
//  Valdi
//

#import "valdi/ios/Gestures/SCValdiGestureRecognizers.h"

#import <UIKit/UIKit.h>

NS_ASSUME_NONNULL_BEGIN

@class SCValdiCustomUnderlineStyle;
@class SCValdiTextAnimationCoordinator;
@class SCValdiTextLayout;
@class SCValdiTextLayoutView;
@protocol SCValdiContextProtocol;
@protocol SCValdiFunction;
@protocol SCValdiViewNodeProtocol;

@protocol SCValdiTextLayoutViewDelegate <NSObject>

- (BOOL)textLayoutViewIsRightToLeft:(SCValdiTextLayoutView *)textLayoutView;
- (nullable id<SCValdiContextProtocol>)valdiContextForTextLayoutView:(SCValdiTextLayoutView *)textLayoutView;
- (nullable id<SCValdiViewNodeProtocol>)valdiViewNodeForTextLayoutView:(SCValdiTextLayoutView *)textLayoutView;

@end

@interface SCValdiTextLayoutView : UIView <SCValdiAttributedTextOnTapGestureRecognizerFunctionProvider>

@property (nonatomic, weak, nullable) id<SCValdiTextLayoutViewDelegate> delegate;
@property (nonatomic, strong, readonly) SCValdiTextLayout *textLayout;
@property (nonatomic, assign) NSUInteger maxNumberOfLines;
@property (nonatomic, strong, nullable) UIColor *defaultTextColor;
@property (nonatomic, assign, readonly) BOOL selectable;
@property (nonatomic, assign, readonly) BOOL usesEffectsLayoutManager;

- (instancetype)initWithFrame:(CGRect)frame usesEffectsLayoutManager:(BOOL)usesEffectsLayoutManager NS_DESIGNATED_INITIALIZER;
- (nullable instancetype)initWithCoder:(NSCoder *)coder NS_DESIGNATED_INITIALIZER;
- (void)configureWithUsesEffectsLayoutManager:(BOOL)usesEffectsLayoutManager;
- (void)setAttributedString:(nullable NSAttributedString *)attributedString;
- (void)setCustomUnderlineStyle:(nullable SCValdiCustomUnderlineStyle *)customUnderlineStyle
         sourceAttributedString:(nullable NSAttributedString *)sourceAttributedString
                characterRanges:(nullable NSArray<NSValue *> *)characterRanges;
- (void)performOnLayoutCallbacks;
- (void)setTextAnimationCoordinator:(nullable SCValdiTextAnimationCoordinator *)coordinator basePartIndex:(NSUInteger)basePartIndex;
- (void)prepareGroupedTextAnimationFrame;
- (BOOL)invalidateGroupedTextAnimationFrame;
- (void)invalidateAnimatedTextProgress;
- (void)stopAnimations;

- (void)setSelectable:(BOOL)selectable;
- (BOOL)setSelection:(NSArray *)selection;
- (void)setOnSelectionChange:(nullable id<SCValdiFunction>)onSelectionChange;
- (void)setOnTextSelectionMenu:(nullable id<SCValdiFunction>)onTextSelectionMenu;
- (void)setOnTextSelectionMenuAction:(nullable id<SCValdiFunction>)onTextSelectionMenuAction;
- (BOOL)pointInsideActiveSelectionHandleBounds:(CGPoint)point;

+ (NSAttributedString *)displayAttributedStringForAttributedString:(NSAttributedString *)attributedString
                                             customUnderlineStyle:(nullable SCValdiCustomUnderlineStyle *)customUnderlineStyle
                                          sourceAttributedString:(NSAttributedString *_Nullable *_Nullable)sourceAttributedString
                                                 characterRanges:(NSArray<NSValue *> *_Nullable *_Nullable)characterRanges;

@end

NS_ASSUME_NONNULL_END
