import XCTest

@testable import Compiler

final class RustCodeGeneratorTests: XCTestCase {
    func testNativeLanguageValues() {
        XCTAssertEqual(NativeLanguage(rawValue: "cpp"), .cpp)
        XCTAssertEqual(NativeLanguage(rawValue: "rust"), .rust)
        XCTAssertNil(NativeLanguage(rawValue: "swift"))
    }

    func testRustIdentifierSanitizationAndKeywords() {
        XCTAssertEqual("value".rustIdentifier, "value")
        XCTAssertEqual("type".rustIdentifier, "r#type")
        XCTAssertEqual("Self".rustIdentifier, "r#Self")
        XCTAssertEqual("1st-value".rustIdentifier, "_1st_value")
        XCTAssertEqual("".rustIdentifier, "_")
        XCTAssertEqual("type".rustTypeIdentifier, "type_")
        XCTAssertEqual("someValue".snakeCased.rustIdentifier, "some_value")
    }

    func testRustIdentifierScopeRejectsSanitizationCollisions() throws {
        let scope = RustIdentifierScope()
        XCTAssertEqual(
            try scope.allocate("first-value", resolved: "first-value".rustIdentifier), "first_value")
        XCTAssertEqual(
            try scope.allocate("first-value", resolved: "first-value".rustIdentifier), "first_value")

        XCTAssertThrowsError(try scope.allocate("first_value", resolved: "first_value".rustIdentifier)) { error in
            XCTAssertEqual(
                (error as? CompilerError)?.name,
                "Rust code generation name collision: 'first-value' and 'first_value' both resolve to 'first_value'"
            )
        }
    }

    func testRustNativeSignaturesRejectForeignDeclarations() throws {
        XCTAssertNoThrow(
            try RustValidation.validateDeclarationIsLocal(
                tsType: "LocalModel",
                declarationIncludeDir: "valdi_modules/local_module",
                moduleIncludeDir: "valdi_modules/local_module",
                bundleName: "local_module"))

        for declaration in ["ForeignModel", "ForeignEnum", "ForeignProxy"] {
            XCTAssertThrowsError(
                try RustValidation.validateDeclarationIsLocal(
                    tsType: declaration,
                    declarationIncludeDir: "valdi_modules/foreign_module",
                    moduleIncludeDir: "valdi_modules/local_module",
                    bundleName: "local_module")
            ) { error in
                XCTAssertEqual(
                    (error as? CompilerError)?.name,
                    "Rust native signature references foreign declaration '\(declaration)' from Valdi module 'foreign_module'. Rust native signatures may only reference declarations from 'local_module'."
                )
            }
        }
    }

    func testRustTypesUseBorrowedModelsAndCollectionsForParameters() {
        let model = RustType.model(rust: "User", borrowed: "UserRef<'_>")
        XCTAssertEqual(model.rustOwnedType, "User")
        XCTAssertEqual(model.rustBorrowedType, "UserRef<'_>")
        XCTAssertEqual(RustType.bytes.rustOwnedType, "valdi_core_rust::BytesView")
        XCTAssertEqual(RustType.bytes.rustBorrowedType, "valdi_core_rust::BytesRef<'_>")
        XCTAssertEqual(
            RustType.valueArray(.string).rustOwnedType,
            "valdi_core_rust::Array<String>")
        XCTAssertEqual(
            RustType.valueArray(.string).rustBorrowedType,
            "valdi_core_rust::ArrayRef<'_, valdi_core_rust::StringRef<'_>>")
        XCTAssertEqual(RustType.nullable(model).rustOwnedType, "Option<User>")
        XCTAssertEqual(RustType.nullable(model).rustBorrowedType, "Option<UserRef<'_>>")

        let callback = RustCallbackType(rustName: "Completion", parameters: [], returnType: .void)
        XCTAssertEqual(RustType.callback(callback).rustBorrowedType, "CompletionRef<'_>")
        let future = RustFutureType(rustName: "ResultFuture", value: .double)
        XCTAssertEqual(RustType.future(future).rustBorrowedType, "ResultFutureRef<'_>")

        let proxy = RustType.handle(rust: "UserProxy", borrowed: "UserProxyRef<'_>")
        XCTAssertEqual(proxy.rustOwnedType, "UserProxy")
        XCTAssertEqual(proxy.rustBorrowedType, "UserProxyRef<'_>")
    }

    func testReferencedTypeDiscoveryTraversesNestedTypesOnce() {
        let callback = RustCallbackType(
            rustName: "CompletionCallback",
            parameters: [RustCallbackParameter(name: "value", type: .string)],
            returnType: .void)
        let future = RustFutureType(rustName: "ResultFuture", value: .callback(callback))
        let types: [RustType] = [
            .nullable(.valueArray(.future(future))),
            .callback(callback),
        ]

        XCTAssertEqual(
            RustReferencedTypeGenerator.referencedCallbacks(in: types).map(\.rustName),
            ["CompletionCallback"])
        XCTAssertEqual(
            RustReferencedTypeGenerator.referencedFutures(in: types).map(\.rustName), ["ResultFuture"])
    }

    func testOwnedByteResultsUseMarshallerTransferPath() {
        XCTAssertEqual(
            RustType.bytes.pushStatement(
                value: "result",
                to: "marshaller",
                owned: true,
                coreModule: "valdi_core_rust"),
            "result.push_owned_to(marshaller)?;")
        XCTAssertEqual(
            RustType.nullable(.bytes).pushStatement(
                value: "result",
                to: "marshaller",
                owned: true,
                coreModule: "valdi_core_rust"),
            "match result { Some(inner) => { inner.push_owned_to(marshaller)?; }, None => marshaller.push_undefined() };")

    }

    func testFutureCompletionUsesMarshallerWithoutTemporaryValues() {
        let voidFuture = RustFutureGenerator.generateDefinition(
            for: RustFutureType(rustName: "VoidFuture", value: .void))
        XCTAssertTrue(voidFuture.contains("self.0.resolve_with(|marshaller|"))
        XCTAssertTrue(voidFuture.contains("marshaller.push_undefined();"))
        XCTAssertFalse(voidFuture.contains("Value::undefined()"))

        let numberFuture = RustFutureGenerator.generateDefinition(
            for: RustFutureType(rustName: "NumberFuture", value: .double))
        XCTAssertTrue(numberFuture.contains("self.0.then_with(move |result|"))
        XCTAssertTrue(numberFuture.contains("self.0.resolve_with(|marshaller|"))
        XCTAssertFalse(numberFuture.contains("self.0.resolve(value.as_borrowed())"))
    }
}
