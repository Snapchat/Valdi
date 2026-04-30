//
//  UIView+ValdiObjects.h
//  ValdiIOS
//
//  Created by Simon Corsin on 5/25/18.
//  Copyright © 2018 Snap Inc. All rights reserved.
//

#import "valdi_core/SCValdiContextProtocol.h"
#import "valdi_core/SCValdiViewNodeProtocol.h"

#import <UIKit/UIKit.h>

NS_ASSUME_NONNULL_BEGIN

@interface UIView (ValdiObjects)

@property (strong, nonatomic, nullable) id<SCValdiContextProtocol> valdiContext;

@property (strong, nonatomic, nullable) id<SCValdiViewNodeProtocol> valdiViewNode;

@end

NS_ASSUME_NONNULL_END
