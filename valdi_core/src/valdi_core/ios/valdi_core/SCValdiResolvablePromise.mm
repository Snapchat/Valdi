// Copyright © 2023 Snap, Inc. All rights reserved.

#import "valdi_core/SCValdiResolvablePromise.h"
#import "valdi_core/cpp/Utils/ResolvablePromise.hpp"
#import "valdi_core/cpp/Utils/Shared.hpp"
#import "valdi_core/cpp/Utils/StringCache.hpp"
#import "valdi_core/SCValdiObjCConversionUtils.h"
#import "valdi_core/SCValdiObjCValue.h"

namespace {

// Interned once: STRING_LITERAL hashes and takes the global StringCache mutex on every call, and
// cancel runs on teardown paths.
STRING_CONST(promiseCanceledMessage, "Promise canceled")

} // namespace

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
    if (_completed) {
        // A producer may legitimately settle asynchronously after cancel already recorded the
        // synthetic canceled error; drop its result. Settling twice without a cancel is still a bug.
        SC_ASSERT(_canceled);
        return;
    }

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
    if (_completed) {
        SC_ASSERT(_canceled);
        return;
    }

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
    // A registrant that lands between cancel's two phases (canceled, not completed yet) is appended
    // and drained by cancel's second phase or by a synchronous producer settle.
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
    if (_canceled || _completed) {
        return;
    }
    _canceled = true;
    dispatch_block_t cancelBlock = _cancelBlock;
    _cancelBlock = nil;

    // _callbacks and _peer intentionally stay populated across the cancel block: the producer may
    // settle synchronously from it, and fulfillWith* then drains the callbacks with the real result.
    lock.unlock();

    if (cancelBlock) {
        cancelBlock();
    }

    lock.lock();
    if (_completed) {
        // The producer settled during cancellation; the callbacks already got the real result.
        auto peer = std::move(_peer);
        lock.unlock();
        (void)peer;
        return;
    }
    NSError *canceledError = ValdiIOS::NSErrorFromError(
        Valdi::Error(promiseCanceledMessage(), Valdi::kPromiseCanceledErrorCode));
    _errorValue = canceledError;
    _completed = YES;
    auto callbacks = std::move(_callbacks);
    auto peer = std::move(_peer);
    lock.unlock();

    for (const auto& callback : callbacks) {
        SCValdiPromiseCallbackForwardFailure(callback.getObject(), canceledError);
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
    // Check _canceled before _completed: cancel records a completion, and a producer attaching to
    // an already-canceled promise must still be told to cancel rather than silently dropped.
    if (_canceled) {
        lock.unlock();

        if (cancelCallback) {
            cancelCallback();
        }
        return;
    }

    if (_completed) {
        lock.unlock();
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
