import Foundation
import valdi_core

public protocol AsyncValdiRuntimeProviding {
    /// Returns the runtime, initializing it on first access. Runs in the caller's isolation;
    /// when the runtime is already initialized this returns without suspending.
    nonisolated(nonsending) func runtime() async -> SCValdiRuntimeProtocol

    /// Returns the JS runtime, initializing the runtime on first access.
    /// Runs in the caller's isolation.
    nonisolated(nonsending) func jsRuntime() async -> SCValdiJSRuntime?
}
