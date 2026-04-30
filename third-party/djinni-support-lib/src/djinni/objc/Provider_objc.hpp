/**
 * Copyright 2021 Snap, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include "DJIMarshal+Private.h"
#import "DJProvider.h"
#include <functional>

namespace djinni {

/**
 * Provider<T> type adapter for Objective-C.
 *
 * Maps C++ std::function<T()> to Objective-C DJProvider<T> generic class
 *
 * The ObjcType is 'DJProvider<T> *' which provides type-safe provider evaluation.
 * Users call the -get method to get the value: T result = [provider get];
 *
 * Analogous to Java's javax.inject.Provider<T> interface.
 *
 * Example:
 *   // From Objective-C to C++
 *   DJProvider<NSString *> *providerStr = [DJProvider providerWithBlock:^NSString *() {
 *       return @"computed value";
 *   }];
 *
 *   // From C++ to Objective-C
 *   DJProvider<NSString *> *objcProvider = ProviderAdaptor::fromCpp(cppProvider);
 *   NSString *result = [objcProvider get];
 */
template<typename T>
class ProviderAdaptor {
    using CppType = std::function<typename T::CppType()>;
    using CppResType = typename T::CppType;
    using ObjcResType = typename T::Boxed::ObjcType;

public:
    using ObjcType = id;  // DJProvider<T> *
    using Boxed = ProviderAdaptor;

    static CppType toCpp(ObjcType o) {
        assert(o);
        // Capture the DJProvider object
        id providerCopy = o;  // ARC will handle reference counting

        return [providerCopy]() -> CppResType {
            // Get the provider value
            ObjcResType result = [providerCopy get];
            return T::Boxed::toCpp(result);
        };
    }

    static ObjcType fromCpp(const CppType& c) {
        // Create a DJProvider that wraps the C++ function
        auto lambda_ptr = std::make_shared<CppType>(c);

        ObjcResType (^block)(void) = ^ObjcResType() {
            CppResType result = (*lambda_ptr)();
            return T::Boxed::fromCpp(result);
        };

        // Create and return a DJProvider instance
        return [DJProvider providerWithBlock:block];
    }
};

} // namespace djinni
