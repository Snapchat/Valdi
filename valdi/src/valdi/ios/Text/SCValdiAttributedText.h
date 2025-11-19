//
//  SCValdiAttributedText.h
//  valdi-ios
//
//  Created by Simon Corsin on 12/19/22.
//

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

NS_ASSUME_NONNULL_BEGIN

typedef NS_ENUM(NSUInteger, SCValdiTextDecoration) {
    SCValdiTextDecorationUnset,
    SCValdiTextDecorationNone,
    SCValdiTextDecorationUnderline,
    SCValdiTextDecorationStrikethrough
};

@protocol SCValdiFunction;
@class SCValdiWrappedValue;

/**
 SCValdiAttributedText is the Objective-C counter part of
 Valdi::TextAttributeValue. It contains a list of parts
 where each part has a string content and an associated style.
 */
@interface SCValdiAttributedText : NSObject

/**
 Returns the number of parts within the AttributedText
 */
@property (readonly, nonatomic) NSUInteger partsCount;

- (instancetype)initWithCppInstance:(void*)cppInstance;

- (instancetype)initWithWrappedValue:(SCValdiWrappedValue*)wrappedValue;

/**
 Return the string content for the part at the given index
 */
- (NSString*)contentAtIndex:(NSUInteger)index;

/**
 Return the font name for the part at the given index,
 or nil if unspecified.
 */
- (nullable NSString*)fontAtIndex:(NSUInteger)index;

/**
 Return the text decoration for the part at the given index
 */
- (SCValdiTextDecoration)textDecorationAtIndex:(NSUInteger)index;

/**
 Return the text color for the part at the given index,
 or nil if unspecified.
 */
- (nullable UIColor*)colorAtIndex:(NSUInteger)index;

/**
 Return the onTap callback for the part the given index,
 or nil if unspecified.
 */
- (nullable id<SCValdiFunction>)onTapAtIndex:(NSUInteger)index;

/**
 Return the onLayout callback for the part at the given index,
 or nil if unspecified.
 */
- (nullable id<SCValdiFunction>)onLayoutAtIndex:(NSUInteger)index;
/**
 Return the outline color for the part at the given index,
 or nil if unspecified.
 */
- (nullable UIColor*)outlineColorAtIndex:(NSUInteger)index;

/**
 Return the outline width for the part at the given index,
 or nil if unspecified.
 */
- (nullable NSNumber*)outlineWidthAtIndex:(NSUInteger)index;

/**
 Return the outer outline color for the part at the given index,
 or nil if unspecified.
 */
- (nullable UIColor*)outerOutlineColorAtIndex:(NSUInteger)index;

/**
 Return the outer outline width for the part at the given index,
 or nil if unspecified.
 */
- (nullable NSNumber*)outerOutlineWidthAtIndex:(NSUInteger)index;

@end

NS_ASSUME_NONNULL_END
