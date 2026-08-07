// Copyright © 2023 Snap, Inc. All rights reserved.

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

/**
 A promise callback represents an object that can receive the result from
 a Promise.
 */
@protocol SCValdiPromiseCallback <NSObject>

/**
 Called whenever the subscribed promise succeeded.
 */
- (void)onSuccessWithValue:(id)value;

/**
 Called whenever the subscribed promise failed.
 */
- (void)onFailureWithError:(NSError*)error;

@end

typedef void (^SCValdiPromiseCallbackBlock)(id _Nullable value, NSError* _Nullable error);

/**
 A Promise represents a future result associated with a value that will
 be emitted at some point. The underlying result can be observed using
 onCompleteWithCallback: . If the promise is cancelable, cancel can
 be called to cancel the pending operation.
 */
@protocol SCValdiPromise <NSObject>

/**
 Observe the result of the promise with the callback given as SCValdiPromiseCallback instance.
 */
- (void)onCompleteWithCallback:(id<SCValdiPromiseCallback>)callback;

/**
 Observe the result of the promise with the callback given as a block.
 */
- (void)onCompleteWithCallbackBlock:(SCValdiPromiseCallbackBlock)block;

/**
 Cancel the ongoing promise.
 */
- (void)cancel;

/**
 Returns whether the promise is cancelable, meaning that the cancel implementation
 is going to actually cancel the underlying operation.
 */
- (BOOL)isCancelable;

/**
 Attach a bridge promise
 */
- (void)setPeer:(void*)peer;

/**
 Acquire a previously attached bridge promise.
 Returns a retained (+1) reference, or nil; the caller takes ownership
 (e.g. by adopting it with Valdi::unsafeBridgeTransfer).
 */
- (void*)getPeer;

@end

/**
 A promise callback represents an object that can receive the result from
 a Promise.
 */
@interface SCValdiPromiseCallback<ValueType> : NSObject <SCValdiPromiseCallback>

/**
 Called whenever the subscribed promise succeeded.
 */
- (void)onSuccessWithValue:(ValueType)value;

/**
 Called whenever the subscribed promise failed.
 */
- (void)onFailureWithError:(NSError*)error;

@end

/**
 A Promise represents a future result associated with a value that will
 be emitted at some point. The underlying result can be observed using
 onCompleteWithCallback: . If the promise is cancelable, cancel can
 be called to cancel the pending operation.
 */
@interface SCValdiPromise<ValueType> : NSObject <SCValdiPromise>

/**
 Observe the result of the promise with the callback given as SCValdiPromiseCallback instance.
 */
- (void)onCompleteWithCallback:(SCValdiPromiseCallback<ValueType>*)callback;

/**
 Observe the result of the promise with the callback given as a block.
 */
- (void)onCompleteWithCallbackBlock:(void (^)(ValueType _Nullable value, NSError* _Nullable error))block;

/**
 Cancel the ongoing promise.
 */
- (void)cancel;

/**
 Returns whether the promise is cancelable, meaning that the cancel implementation
 is going to actually cancel the underlying operation.
 */
- (BOOL)isCancelable;

/**
 Returns a resolved promise with the given value.
 */
+ (SCValdiPromise<ValueType>*)resolvedPromiseWithValue:(ValueType)value;

/**
 Returns a rejected promise with the given error.
 */
+ (SCValdiPromise<ValueType>*)rejectedPromiseWithError:(NSError*)error NS_SWIFT_NAME(rejectedPromise(withError:));

/**
 Attach a bridge promise
 */
- (void)setPeer:(void*)peer;

/**
 Acquire a previously attached bridge promise.
 Returns a retained (+1) reference, or nil; the caller takes ownership
 (e.g. by adopting it with Valdi::unsafeBridgeTransfer).
 */
- (void*)getPeer;

@end

#ifdef __cplusplus
extern "C" {
#endif

/**
 Forward a success value to the given promise callback, which can be either an implementation of
 the SCValdiPromiseCallback protocol, or a SCValdiPromiseCallbackBlock block
 */
void SCValdiPromiseCallbackForwardSuccess(__unsafe_unretained id promiseCallback,
                                          __unsafe_unretained id _Nullable value);

/**
 Forward a failure error to the given promise callback, which can be either an implementation of
 the SCValdiPromiseCallback protocol, or a SCValdiPromiseCallbackBlock block
 */
void SCValdiPromiseCallbackForwardFailure(__unsafe_unretained id promiseCallback, __unsafe_unretained NSError* error);

#ifdef __cplusplus
}
#endif

NS_ASSUME_NONNULL_END
