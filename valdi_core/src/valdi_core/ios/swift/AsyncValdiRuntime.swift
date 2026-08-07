import Foundation
import valdi_core

public class AsyncValdiRuntimeProvider: NSObject, AsyncValdiRuntimeProviding, SCAsyncValdiRuntimeProviding {

    private let factory: @Sendable () async -> SCValdiRuntimeProtocol
    private let deliversWarmCompletionsInline: Bool

    @ValdiActor private var initializationTask: Task<SCValdiRuntimeProtocol, Never>?

    private let cachedRuntimeLock = NSLock()
    private var _cachedRuntime: SCValdiRuntimeProtocol?

    private var cachedRuntime: SCValdiRuntimeProtocol? {
        get {
            cachedRuntimeLock.lock()
            defer { cachedRuntimeLock.unlock() }
            return _cachedRuntime
        }
        set {
            cachedRuntimeLock.lock()
            defer { cachedRuntimeLock.unlock() }
            _cachedRuntime = newValue
        }
    }

    /// - Parameter deliversWarmCompletionsInline: when true and the runtime is already initialized,
    ///   `getRuntime`/`getJSRuntime` invoke their completions synchronously on the calling thread
    ///   instead of dispatching through the ValdiActor.
    public init(
        deliversWarmCompletionsInline: Bool = false,
        factory: @escaping @Sendable () async -> SCValdiRuntimeProtocol
    ) {
        self.deliversWarmCompletionsInline = deliversWarmCompletionsInline
        self.factory = factory
        super.init()
    }

    // MARK: - ObjC Bridge

    @objc(getRuntime:)
    public func getRuntime(completion: @escaping (SCValdiRuntimeProtocol) -> Void) {
        if deliversWarmCompletionsInline, let cachedRuntime {
            completion(cachedRuntime)
            return
        }
        Task { @ValdiActor in
            completion(await self.actorRuntime)
        }
    }

    @objc(getJSRuntime:)
    public func getJSRuntime(completion: @escaping (SCValdiJSRuntime?) -> Void) {
        if deliversWarmCompletionsInline, let cachedRuntime {
            cachedRuntime.getJSRuntime { jsRuntime in
                completion(jsRuntime)
            }
            return
        }
        Task { @ValdiActor in
            let runtime = await self.actorRuntime
            runtime.getJSRuntime { jsRuntime in
                completion(jsRuntime)
            }
        }
    }

    // MARK: - Swift Async API

    nonisolated(nonsending)
    public func runtime() async -> SCValdiRuntimeProtocol {
        if let cachedRuntime {
            return cachedRuntime
        }
        return await actorRuntime
    }

    nonisolated(nonsending)
    public func jsRuntime() async -> SCValdiJSRuntime? {
        await runtime().jsRuntime()
    }

    @ValdiActor private var actorRuntime: SCValdiRuntimeProtocol {
        get async {
            if let cachedRuntime {
                return cachedRuntime
            }

            if let initializationTask {
                return await initializationTask.value
            }

            let factory = self.factory
            let initializationTask = Task {
                await factory()
            }
            self.initializationTask = initializationTask

            let runtime = await initializationTask.value
            self.cachedRuntime = runtime
            self.initializationTask = nil
            return runtime
        }
    }
}
