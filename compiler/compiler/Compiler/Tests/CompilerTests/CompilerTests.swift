import XCTest
import class Foundation.Bundle
@testable import Compiler

final class CompilerTests: XCTestCase {

    func testJsThreadDispatchAttributionIdentifiesGeneratedCallsite() {
        XCTAssertEqual(
            JSThreadDispatchAttribution.generatedFunction(
                bundleName: "valdi_test", modulePath: "src/FunctionTest", functionName: "makeTestObject"),
            "generated.invokeWithJSRuntime:valdi_test/src/FunctionTest#makeTestObject"
        )
    }
}
