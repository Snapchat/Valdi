import XCTest
@testable import Compiler

final class CombineNativeSourcesProcessorTests: XCTestCase {

    // Under single_file_codegen, merged headers previously contained imports of
    // themselves (eg. `#import <TestTypes/TestTypes.h>` inside TestTypes.h).
    // Those are no-ops under header maps but break clang module builds used
    // for Swift interop, so they must be dropped while merging.
    func testFilterSelfImportsDropsAllSelfImportForms() {
        let content = [
            "#import <Foundation/Foundation.h>",
            "#import <TestTypes/TestTypes.h>",  // canonical module-prefixed form
            "#import <TestTypes.h>",            // bare angled form
            "#import \"TestTypes.h\"",          // quoted form
            "@interface TestTypes : NSObject",
            "@end",
        ].joined(separator: "\n")

        let filtered = CombineNativeSourcesProcessor.filterSelfImports(from: content, outputFilename: "TestTypes.h")

        XCTAssertFalse(filtered.contains("#import <TestTypes/TestTypes.h>"))
        XCTAssertFalse(filtered.contains("#import <TestTypes.h>"))
        XCTAssertFalse(filtered.contains("#import \"TestTypes.h\""))
        XCTAssertTrue(filtered.contains("#import <Foundation/Foundation.h>"))
        XCTAssertTrue(filtered.contains("@interface TestTypes : NSObject"))
    }

    func testFilterSelfImportsKeepsImportsOfOtherHeaders() {
        // The module's main header must survive inside the Types header, and
        // vice versa; only imports of the merged output file itself are dropped.
        let content = [
            "#import <Test/Test.h>",
            "#import \"Test.h\"",
            "#import <valdi_core/SCVTypes.h>",
            "#import <TestTypes/TestTypes.h>",
        ].joined(separator: "\n")

        let filtered = CombineNativeSourcesProcessor.filterSelfImports(from: content, outputFilename: "TestTypes.h")

        XCTAssertTrue(filtered.contains("#import <Test/Test.h>"))
        XCTAssertTrue(filtered.contains("#import \"Test.h\""))
        XCTAssertTrue(filtered.contains("#import <valdi_core/SCVTypes.h>"))
        XCTAssertFalse(filtered.contains("#import <TestTypes/TestTypes.h>"))
    }

    func testFilterSelfImportsMatchesIndentedLines() {
        let content = "  #import <TestTypes/TestTypes.h>\n@interface Foo : NSObject\n@end\n"

        let filtered = CombineNativeSourcesProcessor.filterSelfImports(from: content, outputFilename: "TestTypes.h")

        XCTAssertEqual("@interface Foo : NSObject\n@end\n\n", filtered)
    }

    func testFilterSelfImportsPreservesContentWithoutSelfImports() {
        let content = "#import <Foundation/Foundation.h>\n\n@interface Foo : NSObject\n@end\n"

        let filtered = CombineNativeSourcesProcessor.filterSelfImports(from: content, outputFilename: "TestTypes.h")

        XCTAssertEqual("#import <Foundation/Foundation.h>\n\n@interface Foo : NSObject\n@end\n\n", filtered)
    }
}
