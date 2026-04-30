//
//  SCValdiAction.h
//  Valdi
//
//  Created by Simon Corsin on 4/27/18.
//

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

extern NSString* const SCValdiActionSenderKey;

@protocol SCValdiAction <NSObject>

- (void)performWithSender:(nullable id)sender;

// This can be called from any thread, so this needs to be thread safe.
- (nullable id)performWithParameters:(NSArray<id>*)parameters;

@end

NS_ASSUME_NONNULL_END
