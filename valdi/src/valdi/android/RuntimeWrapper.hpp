//
//  RuntimeWrapper.hpp
//  ValdiAndroidNative
//
//  Created by Simon Corsin on 6/6/18.
//  Copyright © 2018 Snap Inc. All rights reserved.
//

#pragma once

#include "valdi/runtime/Runtime.hpp"

namespace ValdiAndroid {

class RuntimeManagerWrapper;

class RuntimeWrapper {
public:
    RuntimeWrapper(const Valdi::SharedRuntime& runtime, RuntimeManagerWrapper* runtimeManagerWrapper);
    ~RuntimeWrapper();

    Valdi::Runtime& getRuntime() const;
    const Valdi::SharedRuntime& getSharedRuntime() const;
    RuntimeManagerWrapper* getRuntimeManagerWrapper() const;

    float getPointScale() const;

private:
    Valdi::SharedRuntime _runtime;
    RuntimeManagerWrapper* _runtimeManagerWrapper;
};

} // namespace ValdiAndroid
