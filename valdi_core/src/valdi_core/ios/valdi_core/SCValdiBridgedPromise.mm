// Copyright © 2023 Snap, Inc. All rights reserved.

#import "valdi_core/SCValdiBridgedPromise+CPP.h"
#import "valdi_core/SCValdiObjCConversionUtils.h"
#import "valdi_core/SCValdiUndefinedValue.h"
#import "valdi_core/cpp/Utils/Shared.hpp"

namespace ValdiIOS {

class PromiseCallback: public Valdi::PromiseCallback {
public:
    PromiseCallback(__unsafe_unretained id promiseCallback,
                    const Valdi::Ref<Valdi::ValueMarshaller<ValdiIOS::ObjCValue>> &valueMarshaller): _promiseCallback(promiseCallback), _valueMarshaller(valueMarshaller) {}

    ~PromiseCallback() final = default;

    void onSuccess(const Valdi::Value& value) final {
        Valdi::SimpleExceptionTracker exceptionTracker;
        auto objcValue = _valueMarshaller->unmarshall(value, Valdi::ReferenceInfoBuilder(), exceptionTracker);
        if (!exceptionTracker) {
            SCValdiPromiseCallbackForwardFailure(_promiseCallback, NSErrorFromError(exceptionTracker.extractError()));
        } else {
            if (objcValue.isVoid()) {
                if (_valueMarshaller->isOptional()) {
                    SCValdiPromiseCallbackForwardSuccess(_promiseCallback, nil);
                } else {
                    SCValdiPromiseCallbackForwardSuccess(_promiseCallback, [SCValdiUndefinedValue undefined]);
                }
            } else {
                SCValdiPromiseCallbackForwardSuccess(_promiseCallback, objcValue.getObject());
            }
        }
    }

    void onFailure(const Valdi::Error& error) final {
        SCValdiPromiseCallbackForwardFailure(_promiseCallback, NSErrorFromError(error));
    }
private:
    id _promiseCallback;
    Valdi::Ref<Valdi::ValueMarshaller<ValdiIOS::ObjCValue>> _valueMarshaller;
};

class IOSPromise final: public Valdi::Promise {
public:
    IOSPromise(__unsafe_unretained id<SCValdiPromise> promise, const Valdi::Ref<Valdi::ValueMarshaller<ValdiIOS::ObjCValue>> &valueMarshaller): _promise(promise), _valueMarshaller(valueMarshaller) {}
    ~IOSPromise() final = default;

    void onComplete(const Valdi::Ref<Valdi::PromiseCallback>& callback) final {
        [_promise onCompleteWithCallback:[[SCValdiBridgedPromiseCallback alloc] initWithPromiseCallback:callback valueMarshaller:_valueMarshaller]];
    }

    void cancel() final {
        if ([_promise respondsToSelector:@selector(cancel)]) {
            [_promise cancel];
        }
    }

    bool isCancelable() const final {
        return [_promise isCancelable];
    }

private:
    id<SCValdiPromise> _promise;
    Valdi::Ref<Valdi::ValueMarshaller<ValdiIOS::ObjCValue>> _valueMarshaller;
};

Valdi::Ref<Valdi::Promise> PromiseFromSCValdiPromise(id<SCValdiPromise> promise, const Valdi::Ref<Valdi::ValueMarshaller<ValdiIOS::ObjCValue>> &valueMarshaller) {
    auto peer = [promise getPeer];
    if (peer != nullptr) {
        // getPeer returns a retained (+1) reference, adopt it
        return Valdi::unsafeBridgeTransfer<Valdi::Promise>(peer);
    }
    auto newPeer = Valdi::makeShared<IOSPromise>(promise, valueMarshaller);
    [promise setPeer:newPeer.get()];
    return newPeer;
}

}

@implementation SCValdiBridgedPromise {
    Valdi::Ref<Valdi::Promise> _promise;
    Valdi::Ref<Valdi::ValueMarshaller<ValdiIOS::ObjCValue>> _valueMarshaller;
}

- (instancetype)initWithPromise:(const Valdi::Ref<Valdi::Promise> &)promise
                valueMarshaller:(const Valdi::Ref<Valdi::ValueMarshaller<ValdiIOS::ObjCValue>> &)valueMarshaller
{
    self = [super init];

    if (self) {
        _promise = promise;
        _valueMarshaller = valueMarshaller;
    }

    return self;
}

- (void)onCompleteWithCallback:(SCValdiPromiseCallback<id>*)callback
{
    _promise->onComplete(Valdi::makeShared<ValdiIOS::PromiseCallback>(callback, _valueMarshaller));
}

- (void)onCompleteWithCallbackBlock:(SCValdiPromiseCallbackBlock)block
{
    _promise->onComplete(Valdi::makeShared<ValdiIOS::PromiseCallback>([block copy], _valueMarshaller));
}

- (void)cancel
{
    _promise->cancel();
}

- (BOOL)isCancelable
{
    return _promise->isCancelable();
}

- (void)setPeer:(void*)peer
{
    // NO-OP, no need, we already have a bridged promise in _promise
}

- (void*)getPeer
{
    return Valdi::unsafeBridgeRetain(_promise.get());
}

@end

@implementation SCValdiBridgedPromiseCallback {
    Valdi::Ref<Valdi::PromiseCallback> _promiseCallback;
    Valdi::Ref<Valdi::ValueMarshaller<ValdiIOS::ObjCValue>> _valueMarshaller;
}

- (instancetype)initWithPromiseCallback:(const Valdi::Ref<Valdi::PromiseCallback> &)promiseCallback valueMarshaller:(const Valdi::Ref<Valdi::ValueMarshaller<ValdiIOS::ObjCValue>> &)valueMarshaller
{
    self = [super init];

    if (self) {
        _promiseCallback = promiseCallback;
        _valueMarshaller = valueMarshaller;
    }

    return self;
}

- (void)onSuccessWithValue:(id)value
{
    Valdi::SimpleExceptionTracker exceptionTracker;
    auto cppValue = _valueMarshaller->marshall(nullptr, ValdiIOS::ObjCValue::makeObject(value), Valdi::ReferenceInfoBuilder(), exceptionTracker);
    if (!exceptionTracker) {
        _promiseCallback->onFailure(exceptionTracker.extractError());
    } else {
        _promiseCallback->onSuccess(cppValue);
    }
}

- (void)onFailureWithError:(NSError *)error
{
    _promiseCallback->onFailure(ValdiIOS::ErrorFromNSError(error));
}

@end
