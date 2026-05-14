//
//  SCValdiViewAssetHandlingProtocol.h
//  valdi_core
//
//  Created by Simon Corsin on 8/11/21.
//

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@protocol SCValdiViewAssetHandlingProtocol <NSObject>

- (void)onValdiAssetDidChange:(nullable id)asset shouldFlip:(BOOL)shouldFlip;

@end

NS_ASSUME_NONNULL_END
