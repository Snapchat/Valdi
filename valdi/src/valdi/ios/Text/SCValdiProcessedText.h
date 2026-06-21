//
//  SCValdiProcessedText.h
//  valdi-ios
//

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

NS_ASSUME_NONNULL_BEGIN

@protocol SCValdiFontManagerProtocol;
@protocol SCValdiFunction;
@class SCValdiCustomUnderlineStyle;
@class SCValdiInlineViewAttachmentInfo;
@class SCValdiTextAnimationTransform;

typedef void (^SCValdiProcessedTextCallbackRangeBlock)(id<SCValdiFunction> callback, NSRange range, BOOL *stop);
typedef void (^SCValdiProcessedTextInlineViewAttachmentBlock)(SCValdiInlineViewAttachmentInfo *attachment,
                                                              NSRange range,
                                                              BOOL *stop);
typedef void (^SCValdiProcessedTextAnimationTransformBlock)(SCValdiTextAnimationTransform *animationTransform,
                                                            NSRange range,
                                                            BOOL *stop);
typedef void (^SCValdiProcessedTextOuterOutlineBlock)(UIColor *color, CGFloat width, NSRange range, BOOL *stop);

typedef NS_ENUM(NSInteger, SCValdiProcessedTextCustomUnderlineMode) {
    SCValdiProcessedTextCustomUnderlineModeNone,
    SCValdiProcessedTextCustomUnderlineModeRemoveNativeUnderline,
    SCValdiProcessedTextCustomUnderlineModeReplaceNativeUnderlineWithColorAttribute,
};

/**
 * Options that customize the final attributed string produced by
 * SCValdiProcessedText.
 *
 * Callers use this when the same parsed Valdi text needs platform-specific
 * postprocessing, such as replacing native underline rendering with custom
 * underline metadata or forcing a foreground color.
 */
@interface SCValdiProcessedTextConfiguration : NSObject

@property (nullable, nonatomic, strong) UIColor *foregroundColorOverride;
@property (nullable, nonatomic, strong) SCValdiCustomUnderlineStyle *customUnderlineStyle;
@property (nonatomic, assign) SCValdiProcessedTextCustomUnderlineMode customUnderlineMode;
@property (nullable, nonatomic, copy) NSAttributedStringKey customUnderlineColorAttributeName;
@property (nullable, nonatomic, strong) UIColor *customUnderlineFallbackColor;

@end

/**
 * Parsed, platform-ready representation of Valdi attributed text on iOS.
 *
 * The object owns the final attributed string installed in UILabel/TextKit and
 * side tables for Valdi-only behavior: tap callbacks, layout callbacks, inline
 * child attachments, animation transforms, custom underline ranges, and outline
 * drawing. Consumers query this object directly instead of scanning private
 * attributes out of NSAttributedString.
 */
@interface SCValdiProcessedText : NSObject

@property (readonly, nonatomic) NSAttributedString *attributedString;
@property (readonly, nonatomic) BOOL hasAnimationTransform;
@property (readonly, nonatomic) BOOL hasInlineViewAttachment;
@property (readonly, nonatomic) BOOL hasOnTap;
@property (readonly, nonatomic) BOOL hasOnLayout;
@property (readonly, nonatomic) BOOL hasOuterOutline;
@property (readonly, nonatomic) BOOL hasCustomUnderline;
@property (readonly, nonatomic) NSUInteger animationTransformsCount;
@property (readonly, nullable, nonatomic) NSAttributedString *customUnderlineSourceString;
@property (readonly, nullable, nonatomic) NSArray<NSValue *> *customUnderlineCharacterRanges;

+ (SCValdiProcessedText *)processedTextWithAttributeValue:(nullable id)attributeValue
                                               attributes:(nullable NSDictionary<NSAttributedStringKey, id> *)attributes
                                            isRightToLeft:(BOOL)isRightToLeft
                                              fontManager:(nullable id<SCValdiFontManagerProtocol>)fontManager
                                          traitCollection:(nullable UITraitCollection *)traitCollection
                                            configuration:(nullable SCValdiProcessedTextConfiguration *)configuration;

- (instancetype)init NS_UNAVAILABLE;

- (nullable id<SCValdiFunction>)onTapAtIndex:(NSUInteger)index effectiveRange:(nullable NSRangePointer)range;
- (nullable id<SCValdiFunction>)onLayoutAtIndex:(NSUInteger)index effectiveRange:(nullable NSRangePointer)range;
- (nullable SCValdiInlineViewAttachmentInfo *)inlineViewAttachmentAtIndex:(NSUInteger)index
                                                           effectiveRange:(nullable NSRangePointer)range;
- (nullable SCValdiInlineViewAttachmentInfo *)inlineViewAttachmentForViewIndex:(NSUInteger)childIndex;
- (BOOL)hasInlineViewAttachmentForIndex:(NSUInteger)childIndex;

- (CGRect)rectForInlineViewAttachment:(SCValdiInlineViewAttachmentInfo *)inlineViewAttachment
                         layoutManager:(NSLayoutManager *)layoutManager
                         textContainer:(NSTextContainer *)textContainer;

- (void)clampToCharacterLimit:(NSInteger)characterLimit
               ignoreNewlines:(BOOL)ignoreNewlines
                    didChange:(nullable BOOL *)didChange;

- (void)enumerateOnLayoutCallbacksUsingBlock:(SCValdiProcessedTextCallbackRangeBlock)block;
- (void)enumerateInlineViewAttachmentsUsingBlock:(SCValdiProcessedTextInlineViewAttachmentBlock)block;
- (void)enumerateAnimationTransformsUsingBlock:(SCValdiProcessedTextAnimationTransformBlock)block;
- (void)enumerateOuterOutlinesUsingBlock:(SCValdiProcessedTextOuterOutlineBlock)block;

// Returns YES when any inline attachment bounds changed.
- (BOOL)updateInlineAttachments;

@end

NS_ASSUME_NONNULL_END
