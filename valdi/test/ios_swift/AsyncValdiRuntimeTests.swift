import XCTest
import ValdiCoreSwift
import valdi_core

class MockValdiRuntime: NSObject, SCValdiRuntimeProtocol {
    func registerNativeModuleFactory(_ moduleFactory: SCNValdiCoreModuleFactory) {}
    func getJSRuntime(_ block: (@Sendable ((any SCValdiJSRuntime)?) -> Void)) {}
    func jsRuntime() -> (any SCValdiJSRuntime)? { nil }
    func inflateView(_ view: UIView & SCValdiRootViewProtocol, owner: Any?, viewModel: Any?, componentContext: Any?) {}
    func inflateView(_ view: UIView & SCValdiRootViewProtocol, owner: Any?, cppMarshaller: UnsafeMutableRawPointer) {}
    func createContext(withComponentPath componentPath: String, viewModel: Any?, componentContext: Any?) -> SCValdiContextProtocol? { nil }
    func createContext(withViewClass viewClass: AnyClass, viewModel: Any?, componentContext: Any?) -> SCValdiContextProtocol? { nil }
    func loadView(withComponentPath componentPath: String, owner: Any?) throws -> UIView & SCValdiRootViewProtocol { fatalError() }
    func loadView(withComponentPath componentPath: String, owner: Any?, viewModel: Any?, componentContext: Any?) throws -> UIView & SCValdiRootViewProtocol { fatalError() }
    func executeMainThreadBatch(_ function: @escaping () -> Void) {}
    func dumpLogMetadata() -> String { "" }
    func dumpLogs() -> String { "" }
    func currentContext() -> SCValdiContextProtocol? { nil }
    func makeViewFactory(_ viewFactory: SCValdiViewFactoryBlock, attributesBinder: SCValdiBindAttributesCallback?, for viewClass: AnyClass) -> (any SCValdiViewFactory) { fatalError() }
    func manager() -> SCValdiRuntimeManagerProtocol? { nil }
    func asset(withModuleName moduleName: String, path: String) -> SCNValdiCoreAsset? { nil }
    func asset(withURL url: String) -> SCNValdiCoreAsset? { nil }
}

class AsyncValdiRuntimeTests: XCTestCase {
    
    /// Core test: getRuntime must not block the caller
    func testGetRuntimeDoesNotBlock() throws {
        let factoryDelay: TimeInterval = 0.1
        
        let asyncRuntime = AsyncValdiRuntimeProvider(
            factory: {
                Thread.sleep(forTimeInterval: factoryDelay)
                return MockValdiRuntime()
            }
        )
        
        let startTime = Date()
        let expectation = self.expectation(description: "Callback called")
        
        asyncRuntime.getRuntime { _ in
            expectation.fulfill()
        }
        
        let elapsed = Date().timeIntervalSince(startTime)
        
        // Call should return immediately, not wait for factory
        XCTAssertLessThan(elapsed, factoryDelay)
        
        waitForExpectations(timeout: 1.0)
    }
    
    /// Core test: multiple callers all receive the runtime
    func testMultipleCallersAllReceiveRuntime() throws {
        let asyncRuntime = AsyncValdiRuntimeProvider(
            factory: {
                Thread.sleep(forTimeInterval: 0.05)
                return MockValdiRuntime()
            }
        )
        
        let expectations = (0..<3).map { self.expectation(description: "Callback \($0)") }
        var callbackCount = 0
        let lock = NSLock()
        
        for i in 0..<3 {
            asyncRuntime.getRuntime { _ in
                lock.lock()
                callbackCount += 1
                lock.unlock()
                expectations[i].fulfill()
            }
        }
        
        waitForExpectations(timeout: 2.0)
        XCTAssertEqual(callbackCount, 3)
    }
}
