import Foundation
import valdi_core

public class AsyncValdiRuntimeProvider: NSObject, AsyncValdiRuntimeProviding, SCAsyncValdiRuntimeProviding {

    private let runtimeLazy: ValdiLazy<SCValdiRuntimeProtocol>

    public init(runtimeLazy: ValdiLazy<SCValdiRuntimeProtocol>) {
        self.runtimeLazy = runtimeLazy
        super.init()
    }

    public convenience init(factory: @escaping @Sendable () -> SCValdiRuntimeProtocol) {
        self.init(runtimeLazy: ValdiLazy(initializer: factory))
    }

    // MARK: - ObjC Bridge

    @objc(getRuntime:)
    public func getRuntime(completion: @escaping (SCValdiRuntimeProtocol) -> Void) {
        Task { @ValdiActor in
            completion(runtimeLazy.value)
        }
    }

    @objc(getJSRuntime:)
    public func getJSRuntime(completion: @escaping (SCValdiJSRuntime?) -> Void) {
        Task { @ValdiActor in
            let runtime = runtimeLazy.value
            runtime.getJSRuntime { jsRuntime in
                completion(jsRuntime)
            }
        }
    }

    // MARK: - Swift Async API

    @ValdiActor public var runtime: SCValdiRuntimeProtocol {
        runtimeLazy.value
    }

    @ValdiActor public var jsRuntime: SCValdiJSRuntime? {
        runtimeLazy.value.jsRuntime()
    }
}
