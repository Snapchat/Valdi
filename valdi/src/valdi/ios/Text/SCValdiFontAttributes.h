//
//  SCValdiFontAttributes.h
//  valdi-ios
//
//  Created by Simon Corsin on 5/18/20.
//

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#import "valdi/ios/Text/SCValdiFont.h"

NSTextAlignment SCValdiFontAttributesResolveTextAlignment(NSTextAlignment textAlignment, BOOL isRightToLeft);
FOUNDATION_EXPORT NSAttributedStringKey const SCValdiLineHeightAttributeName;
FOUNDATION_EXPORT NSAttributedStringKey const SCValdiLineHeightAbsoluteAttributeName;

@class SCNValdiCoreCompositeAttributePart;

@interface SCValdiFontAttributes : NSObject

@property (readonly, nonatomic, strong) SCValdiFont* font;
@property (readonly, nonatomic, strong) UIColor* color;
@property (readonly, nonatomic, assign) NSInteger numberOfLines;
@property (readonly, nonatomic, assign) NSLineBreakMode lineBreakMode;
@property (readonly, nonatomic) BOOL needAttributedString;

- (instancetype)initWithAttributes:(NSDictionary<NSAttributedStringKey, id>*)attributes
                              font:(SCValdiFont*)font
                             color:(UIColor*)color
                      textAligment:(NSTextAlignment)textAlignment
                     numberOfLines:(NSInteger)numberOfLines
                     lineBreakMode:(NSLineBreakMode)lineBreakMode
              needAttributedString:(BOOL)needAttributedString;

- (NSDictionary<NSAttributedStringKey, id>*)resolveAttributesWithIsRightToLeft:(BOOL)isRightToLeft
                                                               traitCollection:(UITraitCollection*)traitCollection;

- (NSTextAlignment)resolveTextAlignmentWithIsRightToLeft:(BOOL)isRightToLeft;

+ (void)applyLineHeightInAttributes:(NSMutableDictionary<NSAttributedStringKey, id>*)attributes
                                font:(UIFont*)font;

@end
