//
//  UIImage+Valdi.h
//  Valdi
//
//  Created by Simon Corsin on 12/12/17.
//  Copyright © 2017 Snap Inc. All rights reserved.
//

#import <UIKit/UIKit.h>

#import "valdi_core/SCValdiContextProtocol.h"

NS_ASSUME_NONNULL_BEGIN

@interface UIImage (Valdi)

+ (nullable UIImage*)imageFromValdiAttributeValue:(nullable id)attributeValue
                                          context:(id<SCValdiContextProtocol>)context;
+ (nullable UIImage*)imageNamed:(NSString*)imageName inValdiBundle:(NSString*)bundleName;

@end

NS_ASSUME_NONNULL_END
