//
//  SCValdiLabel.h
//  Valdi
//
//  Created by Simon Corsin on 5/18/20.
//

#import "valdi/ios/Text/SCValdiAttributedTextHelper.h"
#import <UIKit/UIKit.h>

NS_ASSUME_NONNULL_BEGIN

@class SCValdiFontAttributes;
@protocol SCValdiFontManagerProtocol;

@interface SCValdiLabel : UILabel <SCValdiTextHolder>

- (void)valdi_setSelectable:(BOOL)selectable;

+ (CGSize)measureSizeWithMaxSize:(CGSize)maxSize
                  fontAttributes:(SCValdiFontAttributes*)fontAttributes
                     fontManager:(id<SCValdiFontManagerProtocol>)fontManager
                            text:(id)text
                 traitCollection:(UITraitCollection*)traitCollection;

@end

NS_ASSUME_NONNULL_END
