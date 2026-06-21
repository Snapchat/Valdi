
#import <UIKit/UIKit.h>

@class SCValdiCustomUnderlineStyle;

FOUNDATION_EXPORT NSAttributedStringKey const kSCValdiTextViewCustomUnderlineColorAttribute;

@interface SCValdiTextViewBackgroundEffects : NSObject

@property (nonatomic, strong) UIColor* color;
@property (nonatomic, assign) CGFloat borderRadius;
@property (nonatomic, assign) CGFloat padding;

@end

/// Layout manager for applying text effects to a text view like:
///  - Drawing a background behind each line fragment of text,
///   wrapping each line together making a coheasive background, following the shape of the text.
///  - Drawing an outline stroke around each glyph for a given range for text attribute key
///  `kSCValdiOuterOutlineWidthAttribute`.
@interface SCValdiTextViewEffectsLayoutManager : NSLayoutManager

@property (nonatomic, strong) SCValdiTextViewBackgroundEffects* effects;
@property (nonatomic, strong) SCValdiCustomUnderlineStyle* customUnderlineStyle;

@property (nonatomic, strong, readonly) UIColor* backgroundColor;
@property (nonatomic, assign, readonly) CGFloat backgroundBorderRadius;
@property (nonatomic, assign, readonly) CGFloat backgroundPadding;

@end
