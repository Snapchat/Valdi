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

extern NSString* const kSCValdiAttributedStringKeyOnTap;
extern NSString* const kSCValdiAttributedStringKeyOnLayout;
extern NSString* const kSCValdiOuterOutlineColorAttribute;
extern NSString* const kSCValdiOuterOutlineWidthAttribute;
extern NSString* const kSCValdiAttributedStringKeyAnimationTransform;

@interface NSAttributedString (Valdi)

+ (NSAttributedString*)attributedStringWithValdiText:(id)text
                                          attributes:(NSDictionary<NSAttributedStringKey, id>*)attributes
                                       isRightToLeft:(BOOL)isRightToLeft
                                         fontManager:(id<SCValdiFontManagerProtocol>)fontManager
                                     traitCollection:(UITraitCollection*)traitCollection;

+ (SCValdiFontAttributes*)fontAttributesWithCompositeValue:(NSArray<id>*)compositeValue;
+ (SCValdiFontAttributes*)fontAttributesWithCompositeValueGrowable:(NSArray<id>*)compositeValue;
+ (SCValdiFontAttributes*)fontAttributesWithFont:(SCValdiFont*)font
                                           color:(NSNumber*)color
                                       textAlign:(NSString*)textAlign
                                      lineHeight:(NSNumber*)lineHeight
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

@end

typedef NS_ENUM(NSUInteger, SCValdiTextMode) {
    SCValdiTextModeText,
    SCValdiTextModeAttributedText,
    SCValdiTextModeValdiTextLayout,
};
