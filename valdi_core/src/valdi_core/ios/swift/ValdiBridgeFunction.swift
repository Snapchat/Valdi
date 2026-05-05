import valdi_core

public protocol ValdiBridgeFunction {
    static var className: String { get }
    static func modulePath() -> String
    static var asyncStrictMode: Bool { get }
}

public extension ValdiBridgeFunction {
    /// When async_strict_mode is enabled, asserts that the current thread is not the main thread (to avoid ANRs).
    /// Call this at the start of resolution (e.g. init(jsRuntime:)) to avoid generating the same check in every function class.
    static func assertResolutionNotOnMainThreadIfNeeded(asyncStrictMode: Bool) {
        if asyncStrictMode {
            assert(!Thread.isMainThread,
                   "When async_strict_mode is enabled, function resolution must not be called from the main thread (to avoid ANRs). Use a background thread, the JS thread, or invokeWithJSRuntime.")
        }
    }

    static func createBridgeFunction(jsRuntime: SCValdiJSRuntime) throws -> ValdiFunction {
        return try withMarshaller { marshaller in 
            ValdiMarshallableObjectRegistry.shared.setActiveSchemeOfClassInMarshaller(className: Self.className, marshaller: marshaller)
            let index = jsRuntime.pushModuleAthPath(Self.modulePath(), in: OpaquePointer(marshaller.marshallerCpp))
            try marshaller.checkError()
            return try marshaller.getTypedObjectOrMapProperty(index, 0, Self.className) { try marshaller.getFunction($0) }
        }
    }
}

public func createBridgeFunctionWrapper(_ bridgeFunction: @escaping (ValdiMarshaller) throws -> Void) -> (ValdiMarshaller) -> Bool {
    return { marshaller in
        do {
            try bridgeFunction(marshaller)
            return true
        } catch let error {
            marshaller.setError(error.localizedDescription)
            return false
        }
    }
}
