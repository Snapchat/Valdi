//
//  SCValdiCustomUnderlineStyle.h
//  Valdi
//

#import <UIKit/UIKit.h>

NS_ASSUME_NONNULL_BEGIN

@interface SCValdiCustomUnderlineStyle : NSObject

@property (readonly, nonatomic) CGFloat height;
@property (readonly, nonatomic) CGFloat onWidth;
@property (readonly, nonatomic) CGFloat offWidth;
@property (readonly, nonatomic) CGFloat offset;
@property (readonly, nonatomic, getter=isPatterned) BOOL patterned;

- (instancetype)initWithHeight:(CGFloat)height
                       onWidth:(CGFloat)onWidth
                      offWidth:(CGFloat)offWidth
                        offset:(CGFloat)offset NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

+ (nullable instancetype)styleWithString:(NSString *)styleString error:(NSError *_Nullable *_Nullable)error;

@end

FOUNDATION_EXPORT BOOL SCValdiCustomUnderlineShouldReplaceNativeUnderline(id _Nullable value);

FOUNDATION_EXPORT UIColor *SCValdiCustomUnderlineColorForRange(NSAttributedString *attributedString,
                                                               NSRange range,
                                                               UIColor *_Nullable fallbackColor);

FOUNDATION_EXPORT NSArray<NSValue *> *_Nullable SCValdiCustomUnderlineRemoveNativeUnderlines(
    NSMutableAttributedString *attributedString,
    BOOL removeUnderlineColor);

FOUNDATION_EXPORT void SCValdiCustomUnderlineApplyDashPattern(CGContextRef context,
                                                              SCValdiCustomUnderlineStyle *style);

FOUNDATION_EXPORT NSArray<NSValue *> *SCValdiCustomUnderlineRectsForRange(NSAttributedString *attributedString,
                                                                          NSLayoutManager *layoutManager,
                                                                          NSRange range,
                                                                          NSRange visibleGlyphRange,
                                                                          BOOL clipToVisibleGlyphRange,
                                                                          CGPoint origin,
                                                                          CGFloat lineWidth,
                                                                          CGFloat underlineOffset);

FOUNDATION_EXPORT void SCValdiCustomUnderlineDrawRects(CGContextRef context,
                                                       NSArray<NSValue *> *underlineRects);

NS_ASSUME_NONNULL_END
