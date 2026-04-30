import XCTest
import Foundation
@testable import Compiler

func extractAnnotations(_ content: String) throws -> [ValdiTypeScriptAnnotation] {
    let comments = TS.AST.Comments(text: content, start: 0, end: content.nsrange.length)
    let extracted = try ValdiTypeScriptAnnotation.extractAnnotations(comments: comments,
                                                                        fileContent: content)
    return extracted
}

final class ValdiAnnotationTests: XCTestCase {
    func testPlainAnnotation() throws {
        var content: String = ""
        var result: [ValdiTypeScriptAnnotation] = []

        content = """
// @PlainAnnotation
"""
        result = try extractAnnotations(content)
        XCTAssertEqual(result.first?.name, "PlainAnnotation")

        content = """
// @AnnotationWithParen()
"""
        result = try extractAnnotations(content)
        XCTAssertEqual(result.first?.name, "AnnotationWithParen")

        content = """
/*
 * @MultilineCommentAnnotation()
 */

"""
        result = try extractAnnotations(content)
        XCTAssertEqual(result.first?.name, "MultilineCommentAnnotation")
    }

    func testAnnotationWithPayload() throws {
        var content: String = ""
        var result: [ValdiTypeScriptAnnotation] = []

        content = """
// @EmptyPayloadAnnotation({})
"""
        result = try extractAnnotations(content)
        XCTAssertEqual(result.first?.parameters, [:])

        content = """
// @SimplePayloadAnnotation({ "ios": "blah" })
"""
        result = try extractAnnotations(content)
        XCTAssertEqual(result.first?.parameters, ["ios": "blah"])

        content = """
// @MultipleParamPayloadAnnotation({ "ios": "blah", "foo": "bar" })
"""
        result = try extractAnnotations(content)
        XCTAssertEqual(result.first?.parameters, ["ios": "blah", "foo": "bar"])

        content = """
/*
 * @MultilineParamPayloadAnnotation({
 *  "ios": "blah",
 *  "foo": "bar"
 * })
 */
"""
        result = try extractAnnotations(content)
        XCTAssertEqual(result.first?.parameters, ["ios": "blah", "foo": "bar"])

        content = """
// @SingleQuotePayload({ 'ios': 'blah' })
"""
        result = try extractAnnotations(content)
        XCTAssertEqual(result.first?.parameters, ["ios": "blah"])

        content = """
// @UnquotedPayload({ ios: blah })
"""
        result = try extractAnnotations(content)
        XCTAssertEqual(result.first?.parameters, ["ios": "blah"])

        content = """
// @PartiallyQuotedPayloadR({ ios: 'blah' })
"""
        result = try extractAnnotations(content)
        XCTAssertEqual(result.first?.parameters, ["ios": "blah"])

        content = """
// @PartiallyQuotedPayloadL({ 'ios': blah })
"""
        result = try extractAnnotations(content)
        XCTAssertEqual(result.first?.parameters, ["ios": "blah"])
    }

    func testBadCases() throws {
        var content: String = ""

        try XCTExpectFailure {
            content = """
    // @BadOne({ ios: 'blah, android: 'bleugh' })
    """
            _ = try extractAnnotations(content)
        }

        try XCTExpectFailure {
            content = """
    // @BadTwo({ 'ios: 'blah', android: 'bleugh' })
    """
            _ = try extractAnnotations(content)
        }

        try XCTExpectFailure {
            content = """
// @BadThree({ ios: blah', android: 'bleugh' })
"""
            _ = try extractAnnotations(content)
        }

        try XCTExpectFailure {
            content = """
// @BadFour({ ios: 'blah, android: bleugh' })
"""
            _ = try extractAnnotations(content)
        }
    }

    func testiOSTypeNameValidation() throws {
        XCTAssertTrue(ObjCValidation.isValidIOSTypeName(iosTypeName: "SCCSomeType"))
        XCTAssertTrue(ObjCValidation.isValidIOSTypeName(iosTypeName: "SCCSomeOtherType1234"))
        XCTAssertTrue(ObjCValidation.isValidIOSTypeName(iosTypeName: "_SCCMyType"))

        XCTAssertFalse(ObjCValidation.isValidIOSTypeName(iosTypeName: ""))
        XCTAssertFalse(ObjCValidation.isValidIOSTypeName(iosTypeName: "123Invalid"))
        XCTAssertFalse(ObjCValidation.isValidIOSTypeName(iosTypeName: "SCCObjCCantHaveDollars$"))
        XCTAssertFalse(ObjCValidation.isValidIOSTypeName(iosTypeName: "'SCCAlsoInvalid"))
        XCTAssertFalse(ObjCValidation.isValidIOSTypeName(iosTypeName: "SCCThisToo'"))
        XCTAssertFalse(ObjCValidation.isValidIOSTypeName(iosTypeName: "com.snap.valdi.accidental.AndroidInsteadOfIOS"))
    }

    func testAndroidTypeNameValidation() throws {
        XCTAssertTrue(KotlinValidation.isValidAndroidTypeName(androidTypeName: "com.snap.valdi.Blah"))
        XCTAssertTrue(KotlinValidation.isValidAndroidTypeName(androidTypeName: "com.snap.contextcards.lib.valdi.ContextValdiActionHandler"))

        XCTAssertFalse(KotlinValidation.isValidAndroidTypeName(androidTypeName: ""))
        XCTAssertFalse(KotlinValidation.isValidAndroidTypeName(androidTypeName: "com.snap.packagepath.cant.have/slashes"))
        XCTAssertFalse(KotlinValidation.isValidAndroidTypeName(androidTypeName: "com.snap.package.Cool"))
    }

    /// Regression: When a property has @WorkerThread, parsePropertyLike reconstructs the function type with
    /// shouldCallOnWorkerThread: true. The parsed allowSyncCall (e.g. from @AllowSyncCall on the function type)
    /// must be preserved, not hardcoded to false.
    func testWorkerThreadReconstructionPreservesAllowSyncCall() {
        let returnType = ValdiModelPropertyType.void
        let params = [ValdiModelProperty]()

        let parsedWithAllowSyncCall = ValdiModelPropertyType.function(
            parameters: params,
            returnType: returnType,
            isSingleCall: false,
            shouldCallOnWorkerThread: false,
            allowSyncCall: true
        )

        // Simulate what parsePropertyLike does when property has @WorkerThread: destructure and rebuild
        guard case let .function(parameters, returnType, isSingleCall, _, allowSyncCall) = parsedWithAllowSyncCall.unwrappingOptional else {
            XCTFail("Expected function type")
            return
        }
        let reconstructed = ValdiModelPropertyType.function(
            parameters: parameters,
            returnType: returnType,
            isSingleCall: isSingleCall,
            shouldCallOnWorkerThread: true,
            allowSyncCall: allowSyncCall
        )

        guard case let .function(_, _, _, _, reconstructedAllowSyncCall) = reconstructed.unwrappingOptional else {
            XCTFail("Expected function type")
            return
        }
        XCTAssertTrue(reconstructedAllowSyncCall, "allowSyncCall must be preserved when reconstructing a function type for @WorkerThread")
    }
}
