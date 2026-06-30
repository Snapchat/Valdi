//
//  NSAttributedString+Valdi.h
//  Valdi
//
//  Created by Nathaniel Parrott on 8/10/18.
//

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

@class SCNValdiCoreCompositeAttributePart;
@class SCValdiFontAttributes;
@class SCValdiFont;
@protocol SCValdiFontManagerProtocol;

@interface NSAttributedString (Valdi)

+ (SCValdiFontAttributes*)fontAttributesWithCompositeValue:(NSArray<id>*)compositeValue;
+ (SCValdiFontAttributes*)fontAttributesWithCompositeValueGrowable:(NSArray<id>*)compositeValue;
+ (SCValdiFontAttributes*)fontAttributesWithFont:(SCValdiFont*)font
                                           color:(NSNumber*)color
                                       textAlign:(NSString*)textAlign
                                      lineHeight:(NSNumber*)lineHeight
                             lineHeightAbsolute:(NSNumber*)lineHeightAbsolute
                                  textDecoration:(NSString*)textDecoration
                                   letterSpacing:(NSNumber*)letterSpacing
                                   numberOfLines:(NSNumber*)numberOfLines
                                    textOverflow:(NSString*)textOverflow;

+ (SCValdiFontAttributes*)defaultFontAttributes;
/*
 * Initializes a UILabel under the hood (see implementation),
 * so first call MUST BE ON THE MAIN THREAD.
 */
+ (NSParagraphStyle*)defaultParagraphStyle;

+ (NSArray<SCNValdiCoreCompositeAttributePart*>*)valdiFontAttributes;
+ (NSArray<SCNValdiCoreCompositeAttributePart*>*)valdiFontAttributesGrowable;

/*
 * Parses and trims an attributed string to match character limit, keeping styling info
 */
+ (NSAttributedString*)trimAttributedString:(NSAttributedString*)attributedString
                             characterLimit:(NSInteger)characterLimit;

+ (NSAttributedString *)valdi_attributedStringWithAttachment:(NSTextAttachment *)attachment
                                                  attributes:(NSMutableDictionary<NSAttributedStringKey, id> *)attributes;

@end

typedef NS_ENUM(NSUInteger, SCValdiTextMode) {
    SCValdiTextModeText,
    SCValdiTextModeAttributedText,
    SCValdiTextModeValdiTextLayout,
};
