import Foundation
import valdi_core

public protocol AsyncValdiRuntimeProviding {
    @ValdiActor var runtime: SCValdiRuntimeProtocol { get }
    @ValdiActor var jsRuntime: SCValdiJSRuntime? { get }
}
