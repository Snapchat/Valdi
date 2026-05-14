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

import DjinniSupportCxx
import Foundation

/// Provider of a value: equivalent to `() -> T`. Used by the Djinni `provider<T>` type
/// so generated code has a concrete nominal type (e.g. `Provider<Int32>`).
public struct Provider<T> {
    private let fn: () -> T

    public init(_ fn: @escaping () -> T) {
        self.fn = fn
    }

    public func callAsFunction() -> T {
        fn()
    }
}

// Box to hold a Swift closure so we can pass it to C++ as (callback, context).
private final class ProviderClosureBox {
    let fn: () -> djinni.swift.AnyValue
    init(_ fn: @escaping () -> djinni.swift.AnyValue) { self.fn = fn }
}

private func providerClosureCall(ctx: UnsafeMutableRawPointer?) -> djinni.swift.AnyValue {
    let box = Unmanaged<ProviderClosureBox>.fromOpaque(ctx!).takeUnretainedValue()
    return box.fn()
}

private func providerClosureRelease(ctx: UnsafeMutableRawPointer?) {
    Unmanaged<ProviderClosureBox>.fromOpaque(ctx!).release()
}

// `provider<T>` is marshalled as Provider<T> (type-alias style, like TypeScript's Provider<T> = () => T)
public enum ProviderMarshaller<T: Marshaller>: Marshaller {
    public typealias SwiftType = Provider<T.SwiftType>

    public static func fromCpp(_ v: djinni.swift.AnyValue) -> SwiftType {
        let cppFunc = v
        return Provider {
            let result = djinni.swift.callProviderFunction(cppFunc)
            return T.fromCpp(result)
        }
    }

    public static func toCpp(_ s: SwiftType) -> djinni.swift.AnyValue {
        let box = ProviderClosureBox {
            let result = s.callAsFunction()
            return T.toCpp(result)
        }
        let boxPtr = Unmanaged.passRetained(box).toOpaque()
        return djinni.swift.makeProviderFunction(providerClosureCall, boxPtr, providerClosureRelease)
    }
}
