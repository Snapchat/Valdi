import XCTest
@testable import Compiler

final class ValdiDocumentParserTests: XCTestCase {

    // Same-name attributes (a plain `x` and a view-model `:x`) tie in the parser's name sort, so
    // their relative order must come from the source. It used to come from Dictionary iteration,
    // which is reseeded per process, so the generated code differed from run to run. With eight
    // pairs, a random order would pass this test with probability 1/256.
    func testSameNameAttributesKeepSourceOrder() throws {
        let names = ["a", "b", "c", "d", "e", "f", "g", "h"]
        let attributes = names.map { "\($0)=\"{{ viewModel.\($0) }}\" :\($0)=\"viewModel.\($0)Child\"" }.joined(separator: " ")
        let content = """
        <template>
          <Root>
            <view \(attributes)/>
          </Root>
        </template>
        """

        let document = try ValdiDocumentParser.parse(logger: Logger(output: BufferLoggerOutput()), content: content, iosImportPrefix: "")
        let child = try XCTUnwrap(document.template?.rootNode?.children.compactMap { $0.node }.first)

        let parsed = child.customAttributes.map { ($0.name, $0.isViewModelField) }
        let expected = names.flatMap { [($0, false), ($0, true)] }
        XCTAssertEqual(parsed.map { $0.0 }, expected.map { $0.0 })
        XCTAssertEqual(parsed.map { $0.1 }, expected.map { $0.1 })
    }
}
