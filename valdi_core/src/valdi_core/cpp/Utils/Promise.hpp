//
//  Promise.hpp
//  valdi-core
//
//  Created by Simon Corsin on 8/07/23.
//

#pragma once

#include "valdi_core/cpp/Utils/Function.hpp"
#include "valdi_core/cpp/Utils/Result.hpp"
#include "valdi_core/cpp/Utils/ValdiObject.hpp"
#include "valdi_core/cpp/Utils/Value.hpp"

namespace Valdi {

using PromiseCallbackAsFunction = Function<void(const Result<Value>&)>;

class PromiseCallback : public SimpleRefCountable {
public:
    virtual void onSuccess(const Value& value) = 0;
    virtual void onFailure(const Error& error) = 0;
};

class Promise : public ValdiObject {
public:
    Promise();
    ~Promise() override;

    /**
     Register a callback for the promise's terminal result. The callback is invoked exactly once —
     immediately if the promise already settled (including with the cancellation failure described
     on cancel()), otherwise when it settles — and released afterwards.
     */
    virtual void onComplete(const Ref<PromiseCallback>& callback) = 0;
    void onComplete(PromiseCallbackAsFunction callback);

    /**
     Request cancellation of the pending operation. Contract (reference implementation:
     ResolvablePromise):

     - Canceling an already-settled or already-canceled promise is a no-op.
     - The producer's cancel callback (if any) runs first. If the producer settles the promise from
       it, pending callbacks receive that real result.
     - If the producer does not settle during cancellation, the promise settles with
       Error("Promise canceled", kPromiseCanceledErrorCode) and pending callbacks receive it as a
       failure.
     - Either way, every pending callback is invoked exactly once and then released; callbacks
       registered after cancellation are served the recorded result immediately.
     - A producer that settles after cancellation already recorded the canceled failure is dropped
       silently.
     */
    virtual void cancel() = 0;

    virtual bool isCancelable() const = 0;

    VALDI_CLASS_HEADER(Promise);
};

} // namespace Valdi
