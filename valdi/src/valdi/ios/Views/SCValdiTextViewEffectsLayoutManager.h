
#import <UIKit/UIKit.h>

NS_ASSUME_NONNULL_BEGIN

@class SCValdiCustomUnderlineStyle;
@class SCValdiProcessedText;
@class SCValdiTextAnimationCoordinator;
@class SCValdiTextAnimationPresentation;
@protocol SCValdiViewNodeProtocol;

FOUNDATION_EXPORT NSAttributedStringKey const kSCValdiTextViewCustomUnderlineColorAttribute;

@interface SCValdiTextViewBackgroundEffects : NSObject

@property (nonatomic, strong, nullable) UIColor* color;
@property (nonatomic, assign) CGFloat borderRadius;
@property (nonatomic, assign) CGFloat padding;

@end

/// Layout manager for applying text effects to a text view like:
///  - Drawing a background behind each line fragment of text,
///   wrapping each line together making a coheasive background, following the shape of the text.
///  - Drawing an outline stroke around each glyph for processed text outline ranges.
@interface SCValdiTextViewEffectsLayoutManager : NSLayoutManager

@property (nonatomic, strong, nullable) SCValdiTextViewBackgroundEffects* effects;
@property (nonatomic, strong, nullable) SCValdiCustomUnderlineStyle* customUnderlineStyle;
@property (nonatomic, strong, nullable) NSAttributedString *customUnderlineSourceAttributedString;
@property (nonatomic, copy, nullable) NSArray<NSValue *> *customUnderlineCharacterRanges;
@property (nonatomic, strong, nullable) UIColor *customUnderlineFallbackColor;
@property (nonatomic, strong, nullable) SCValdiProcessedText* processedText;

@property (nonatomic, strong, readonly) UIColor* backgroundColor;
@property (nonatomic, assign, readonly) CGFloat backgroundBorderRadius;
@property (nonatomic, assign, readonly) CGFloat backgroundPadding;
@property (nonatomic, assign, readonly) BOOL hasActiveAnimationRanges;
@property (nonatomic, weak, nullable) SCValdiTextAnimationCoordinator* textAnimationCoordinator;
@property (nonatomic, assign) NSUInteger textAnimationBasePartIndex;
@property (nonatomic, weak, nullable) id<SCValdiViewNodeProtocol> valdiViewNode;

- (void)prepareGroupedAnimatedTextProgress;
- (BOOL)invalidateAnimatedTextProgress;
- (void)saveAnimatedTextProgress;
- (void)clearAnimatedTextProgress;
- (CGFloat)opacityForAnimationRange:(NSRange)range;
- (nullable SCValdiTextAnimationPresentation *)presentationForAnimationRange:(NSRange)range;

@end

NS_ASSUME_NONNULL_END
