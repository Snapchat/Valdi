//
//  RuntimeWrapper.cpp
//  ValdiAndroidNative
//
//  Created by Simon Corsin on 6/6/18.
//  Copyright © 2018 Snap Inc. All rights reserved.
//
#include "valdi/android/RuntimeWrapper.hpp"
#include "valdi/android/RuntimeManagerWrapper.hpp"

namespace ValdiAndroid {

RuntimeWrapper::RuntimeWrapper(const Valdi::SharedRuntime& runtime,
                               RuntimeManagerWrapper* runtimeManagerWrapper)
    : _runtime(runtime), _runtimeManagerWrapper(runtimeManagerWrapper) {}

RuntimeWrapper::~RuntimeWrapper() {
    _runtime->fullTeardown();
}

Valdi::Runtime& RuntimeWrapper::getRuntime() const {
    return *_runtime;
}

const Valdi::SharedRuntime& RuntimeWrapper::getSharedRuntime() const {
    return _runtime;
}

RuntimeManagerWrapper* RuntimeWrapper::getRuntimeManagerWrapper() const {
    return _runtimeManagerWrapper;
}

float RuntimeWrapper::getPointScale() const {
    return _runtimeManagerWrapper->getPointScale();
}

} // namespace ValdiAndroid
