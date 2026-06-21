//
//  SCValdiTextLayout.h
//  valdi-ios
//
//  Created by Simon Corsin on 12/21/22.
//

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

NS_ASSUME_NONNULL_BEGIN

@class SCValdiFontAttributes;
@protocol SCValdiFontManagerProtocol;

/**
 A TextLayout implementation that leverages TextKit.
 Used for layouts that requires introspection at runtime
 on how the characters are actually laid out.
 */
@interface SCValdiTextLayout : NSObject

@property (copy, nonatomic, nullable) NSAttributedString* attributedString;

@property (assign, nonatomic) CGSize size;
@property (assign, nonatomic) NSUInteger maxNumberOfLines;
@property (readonly, nonatomic) CGRect usedRect;

- (instancetype)init;

/**
 Draw the entire text layout inside the given rect
 */
- (void)drawInRect:(CGRect)rect;

/**
 Return the closest character index at the given point.
 Returns NSNotFound if there are no characters close to the given point.
 */
- (NSInteger)characterIndexAtPoint:(CGPoint)point;

/**
 Return the bounding rect for the given range of characters.
 */
- (CGRect)boundingRectForRange:(NSRange)range;

/**
 Return the per-line underline rects for the given range of characters in the
 same coordinate space used by drawInRect:.
 */
- (NSArray<NSValue *> *)underlineRectsForRange:(NSRange)range
                                 inDrawingRect:(CGRect)rect
                                     lineWidth:(CGFloat)lineWidth
                               underlineOffset:(CGFloat)underlineOffset;

/**
 Return the bounding rect for the given attributed string, constrained to the
 given maxSize and max number of lines.
 */
+ (CGRect)boundingRectWithAttributedString:(NSAttributedString*)attributedString
                                   maxSize:(CGSize)maxSize
                          maxNumberOfLines:(NSUInteger)maxNumberOfLines;

+ (CGSize)measureSizeWithMaxSize:(CGSize)maxSize
                   fontAttributes:(nullable SCValdiFontAttributes *)fontAttributes
                      fontManager:(id<SCValdiFontManagerProtocol>)fontManager
                             text:(nullable id)text
                  traitCollection:(nullable UITraitCollection *)traitCollection;

@end

NS_ASSUME_NONNULL_END
