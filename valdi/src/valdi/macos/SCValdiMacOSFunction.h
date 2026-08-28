//
//  SCValdiMacOSFunction.h
//  valdi-macos
//
//  Created by Simon Corsin on 10/13/20.
//

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

typedef id _Nullable (^SCValdiMacOSFunctionBlock)(NSArray<id>* parameters);

@interface SCValdiMacOSFunction : NSObject

@property (readonly, nonatomic) void* cppInstance;

- (instancetype)initWithCppInstance:(void*)cppInstance;
- (instancetype)initWithBlock:(SCValdiMacOSFunctionBlock)block;

- (void)performWithParameters:(NSArray<id>*)parameters;
- (nullable id)performWithParametersAndReturnValue:(NSArray<id>*)parameters;

@end

NS_ASSUME_NONNULL_END
