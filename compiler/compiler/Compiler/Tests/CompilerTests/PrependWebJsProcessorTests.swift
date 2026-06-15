import XCTest
@testable import Compiler

final class PrependWebJsProcessorTests: XCTestCase {

    // MARK: - stripMultiArgRequires
    //
    // Multi-arg detection lives in the companion AST transformer
    // (WebRequireTransformer.ts). It wraps each string-literal multi-arg
    // require() call in /* @valdi-web-strip-start */ ... /* @valdi-web-strip-end */
    // sentinels. The Swift processor only rewrites sentinel-bounded regions.

    private static let stripStart = "/* @valdi-web-strip-start */"
    private static let stripEnd = "/* @valdi-web-strip-end */"

    func testStripMultiArgRequires_singleArg_unsentinelled_unchanged() {
        let input = #"const x = require("foo");"#
        XCTAssertEqual(PrependWebJSProcessor.stripMultiArgRequires(input), input)
    }

    func testStripMultiArgRequires_multiArg_withoutSentinels_unchanged() {
        // AST guarantees sentinels around every string-literal multi-arg
        // require. A raw multi-arg call (e.g. handwritten in compiled JS)
        // must be left alone.
        let input = #"const x = require("foo", true, true);"#
        XCTAssertEqual(PrependWebJSProcessor.stripMultiArgRequires(input), input)
    }

    func testStripMultiArgRequires_twoArgs() {
        let input = "const x = \(Self.stripStart) require(\"foo\", true) \(Self.stripEnd);"
        let expected = #"const x = require("foo");"#
        XCTAssertEqual(PrependWebJSProcessor.stripMultiArgRequires(input), expected)
    }

    func testStripMultiArgRequires_threeArgs() {
        let input = "const x = \(Self.stripStart) require(\"foo\", true, false) \(Self.stripEnd);"
        let expected = #"const x = require("foo");"#
        XCTAssertEqual(PrependWebJSProcessor.stripMultiArgRequires(input), expected)
    }

    func testStripMultiArgRequires_singleQuotes() {
        let input = "const x = \(Self.stripStart) require('foo', true, true) \(Self.stripEnd);"
        let expected = "const x = require('foo');"
        XCTAssertEqual(PrependWebJSProcessor.stripMultiArgRequires(input), expected)
    }

    func testStripMultiArgRequires_nestedParens() {
        let input = "const x = \(Self.stripStart) require(\"foo\", bar(1, 2)) \(Self.stripEnd);"
        let expected = #"const x = require("foo");"#
        XCTAssertEqual(PrependWebJSProcessor.stripMultiArgRequires(input), expected)
    }

    func testStripMultiArgRequires_stringInExtraArgs() {
        let input = "const x = \(Self.stripStart) require(\"foo\", \"bar,baz\") \(Self.stripEnd);"
        let expected = #"const x = require("foo");"#
        XCTAssertEqual(PrependWebJSProcessor.stripMultiArgRequires(input), expected)
    }

    func testStripMultiArgRequires_multipleRequiresInLine() {
        let input = "\(Self.stripStart) require(\"a\", true) \(Self.stripEnd); \(Self.stripStart) require(\"b\", false) \(Self.stripEnd);"
        let expected = #"require("a"); require("b");"#
        XCTAssertEqual(PrependWebJSProcessor.stripMultiArgRequires(input), expected)
    }

    func testStripMultiArgRequires_preservesNonRequireParens() {
        // foo("a", true) lacks sentinels — untouched. require("b", true) is
        // wrapped — stripped.
        let input = "foo(\"a\", true); \(Self.stripStart) require(\"b\", true) \(Self.stripEnd);"
        let expected = #"foo("a", true); require("b");"#
        XCTAssertEqual(PrependWebJSProcessor.stripMultiArgRequires(input), expected)
    }

    func testStripMultiArgRequires_escapedQuoteInPath() {
        let input = "\(Self.stripStart) require(\"foo\\\"bar\", true) \(Self.stripEnd)"
        let expected = #"require("foo\"bar")"#
        XCTAssertEqual(PrependWebJSProcessor.stripMultiArgRequires(input), expected)
    }

    func testStripMultiArgRequires_strayStartMarker_unchanged() {
        let input = "const s = '\(Self.stripStart)';"
        XCTAssertEqual(PrependWebJSProcessor.stripMultiArgRequires(input), input)
    }

    func testStripMultiArgRequires_strayEndMarker_unchanged() {
        let input = "const s = '\(Self.stripEnd)';"
        XCTAssertEqual(PrependWebJSProcessor.stripMultiArgRequires(input), input)
    }

    // MARK: - addJsExtToRelativeRequires

    func testAddJsExt_relativeWithoutExtension() {
        let input = #"require("./foo")"#
        let expected = #"require("./foo.js")"#
        XCTAssertEqual(PrependWebJSProcessor.addJsExtToRelativeRequires(input), expected)
    }

    func testAddJsExt_parentRelativeWithoutExtension() {
        let input = #"require("../../res")"#
        let expected = #"require("../../res.js")"#
        XCTAssertEqual(PrependWebJSProcessor.addJsExtToRelativeRequires(input), expected)
    }

    func testAddJsExt_alreadyHasJsExtension() {
        let input = #"require("./foo.js")"#
        XCTAssertEqual(PrependWebJSProcessor.addJsExtToRelativeRequires(input), input)
    }

    func testAddJsExt_jsonExtension_unchanged() {
        let input = #"require("./data.json")"#
        XCTAssertEqual(PrependWebJSProcessor.addJsExtToRelativeRequires(input), input)
    }

    func testAddJsExt_bareModule_unchanged() {
        let input = #"require("tslib")"#
        XCTAssertEqual(PrependWebJSProcessor.addJsExtToRelativeRequires(input), input)
    }

    func testAddJsExt_scopedPackage_unchanged() {
        let input = #"require("@protobuf-ts/runtime")"#
        XCTAssertEqual(PrependWebJSProcessor.addJsExtToRelativeRequires(input), input)
    }

    func testAddJsExt_singleQuotes() {
        let input = "require('./foo')"
        let expected = "require('./foo.js')"
        XCTAssertEqual(PrependWebJSProcessor.addJsExtToRelativeRequires(input), expected)
    }

    func testAddJsExt_multipleRequires() {
        let input = #"require("./a"); require("../../b"); require("./c.js")"#
        let expected = #"require("./a.js"); require("../../b.js"); require("./c.js")"#
        XCTAssertEqual(PrependWebJSProcessor.addJsExtToRelativeRequires(input), expected)
    }

    func testAddJsExt_deepRelativePath() {
        let input = #"require("../../../valdi_core/src/Component")"#
        let expected = #"require("../../../valdi_core/src/Component.js")"#
        XCTAssertEqual(PrependWebJSProcessor.addJsExtToRelativeRequires(input), expected)
    }

    func testAddJsExt_bareModuleWithSlash_unchanged() {
        let input = #"require("valdi_core/src/Component")"#
        XCTAssertEqual(PrependWebJSProcessor.addJsExtToRelativeRequires(input), input)
    }
}
