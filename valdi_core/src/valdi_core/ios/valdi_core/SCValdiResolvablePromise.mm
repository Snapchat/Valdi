// Copyright © 2023 Snap, Inc. All rights reserved.

#import "valdi_core/SCValdiResolvablePromise.h"
#import "valdi_core/cpp/Utils/ResolvablePromise.hpp"
#import "valdi_core/cpp/Utils/Shared.hpp"
#import "valdi_core/SCValdiObjCValue.h"

@implementation SCValdiResolvablePromise {
    Valdi::Mutex _mutex;
    id _successValue;
    NSError *_errorValue;
    Valdi::SmallVector<ValdiIOS::ObjCValue, 1> _callbacks;
    dispatch_block_t _cancelBlock;
    bool _canceled;
    bool _completed;
    Valdi::Ref<Valdi::Promise> _peer;
}

- (instancetype)init
{
    self = [super init];

    if (self) {

    }

    return self;
}

- (void)fulfillWithSuccessValue:(id)value
{
    std::unique_lock<Valdi::Mutex> lock(_mutex);
    SC_ASSERT(!_completed);

    _successValue = value;
    _completed = YES;
    auto callbacks = std::move(_callbacks);
    dispatch_block_t cancelBlock = _cancelBlock;
    _cancelBlock = nil;
    auto peer = std::move(_peer);

    lock.unlock();

    for (const auto& callback : callbacks) {
        SCValdiPromiseCallbackForwardSuccess(callback.getObject(), value);
    }

    // Make sure we release the block and the peer with the mutex unlocked
    (void)cancelBlock;
    (void)peer;
}

- (void)fulfillWithError:(NSError *)error
{
    std::unique_lock<Valdi::Mutex> lock(_mutex);
    SC_ASSERT(!_completed);

    _errorValue = error;
    _completed = YES;
    auto callbacks = std::move(_callbacks);
    dispatch_block_t cancelBlock = _cancelBlock;
    _cancelBlock = nil;
    auto peer = std::move(_peer);

    lock.unlock();

    for (const auto& callback : callbacks) {
        SCValdiPromiseCallbackForwardFailure(callback.getObject(), error);
    }

    // Make sure we release the block and the peer with the mutex unlocked
    (void)cancelBlock;
    (void)peer;
}

- (void)_doOnCompleteWithCallbackUntyped:(__unsafe_unretained id)callbackUntyped
{
    std::unique_lock<Valdi::Mutex> lock(_mutex);
    if (_canceled) {
        return;
    }

    if (_completed) {
        if (_errorValue) {
            NSError *errorValue = _errorValue;
            lock.unlock();

            SCValdiPromiseCallbackForwardFailure(callbackUntyped, errorValue);
        } else {
            id successValue = _successValue;
            lock.unlock();

            SCValdiPromiseCallbackForwardSuccess(callbackUntyped, successValue);
        }

        return;
    }

    _callbacks.emplace_back(ValdiIOS::ObjCValue::makeObject(callbackUntyped));
}

- (void)onCompleteWithCallback:(SCValdiPromiseCallback<id> *)callback
{
    [self _doOnCompleteWithCallbackUntyped:callback];
}

- (void)onCompleteWithCallbackBlock:(SCValdiPromiseCallbackBlock)block
{
    [self _doOnCompleteWithCallbackUntyped:[block copy]];
}

- (void)cancel
{
    std::unique_lock<Valdi::Mutex> lock(_mutex);
    if (_canceled) {
        return;
    }
    _canceled = true;
    dispatch_block_t cancelBlock = _cancelBlock;
    _cancelBlock = nil;
    auto peer = std::move(_peer);

    lock.unlock();

    if (cancelBlock) {
        cancelBlock();
    }

    // Make sure we release the peer with the mutex unlocked
    (void)peer;
}

- (BOOL)isCancelable
{
    std::lock_guard<Valdi::Mutex> lock(_mutex);
    return _cancelBlock != nil;
}

- (void)setCancelCallback:(dispatch_block_t)cancelCallback
{
    std::unique_lock<Valdi::Mutex> lock(_mutex);
    if (_completed) {
        return;
    }

    if (_canceled) {
        lock.unlock();

        if (cancelCallback) {
            cancelCallback();
        }
        return;
    }

    dispatch_block_t oldCancelBlock = _cancelBlock;
    _cancelBlock = cancelCallback;
    lock.unlock();

    // Make sure we release the block with the mutex unlocked
    (void)oldCancelBlock;
}

- (void)setPeer:(void*)peer
{
    std::lock_guard<Valdi::Mutex> lock(_mutex);
    if (_completed || _canceled) {
        // Only fulfill*/cancel clear _peer, and they already ran. Storing the peer now
        // would create an unbreakable retain cycle with it (the peer strongly retains
        // this promise). Skipping is safe: a fresh transient peer forwards immediately
        // once the promise is resolved.
        return;
    }
    _peer = Valdi::unsafeBridge<Valdi::Promise>(peer);
}

- (void*)getPeer
{
    std::lock_guard<Valdi::Mutex> lock(_mutex);
    // Returned retained (+1): a concurrent fulfill*/cancel can drop the last
    // reference to _peer right after we unlock, so an unretained pointer could
    // dangle before the caller takes ownership.
    return Valdi::unsafeBridgeRetain(_peer.get());
}

@end
