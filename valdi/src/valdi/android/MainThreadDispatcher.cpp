//
//  MainThreadDispatcher.cpp
//  ValdiAndroidNative
//
//  Created by Simon Corsin on 6/13/18.
//  Copyright © 2018 Snap Inc. All rights reserved.
//

#include "valdi/android/MainThreadDispatcher.hpp"
#include <future>

namespace ValdiAndroid {

MainThreadDispatcher::MainThreadDispatcher(JavaEnv env, jobject object)
    : GlobalRefJavaObjectBase(env, object, "MainThreadManager") {
    auto obj = toObject();
    auto cls = obj.getClass();
    cls.getMethod("runOnMainThread", _runOnMainThreadMethod);
    cls.getMethod("runOnMainThreadDelayed", _runOnMainThreadDelayedMethod);
}

MainThreadDispatcher::~MainThreadDispatcher() = default;

void MainThreadDispatcher::dispatch(Valdi::DispatchFunction* function, bool sync) {
    if (sync) {
        std::promise<void> promise;
        auto future = promise.get_future();

        // Capture `function` by value (copy the pointer) to avoid a dangling reference.
        // The original code used [&] which captured `function` by reference to the stack
        // variable. After promise.set_value(), the calling thread wakes up from future.get()
        // and could potentially destroy its stack frame before `delete function` executes,
        // causing a use-after-free on the `function` pointer itself.
        auto wrappedFn = new Valdi::DispatchFunction([function, &promise]() {
            (*function)();
            promise.set_value();
            delete function;
        });

        _runOnMainThreadMethod.call(toObject(), reinterpret_cast<int64_t>(wrappedFn));
        future.get();
    } else {
        _runOnMainThreadMethod.call(toObject(), reinterpret_cast<int64_t>(function));
    }
}

void MainThreadDispatcher::dispatchAfter(int64_t delayMs, Valdi::DispatchFunction* function) {
    _runOnMainThreadDelayedMethod.call(toObject(), delayMs, reinterpret_cast<int64_t>(function));
}

} // namespace ValdiAndroid
