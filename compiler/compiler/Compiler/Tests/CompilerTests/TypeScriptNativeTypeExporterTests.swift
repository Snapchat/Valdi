import XCTest
import Foundation
@testable import Compiler

// Reference table mirroring what the companion emits for `number & { ... }` and `string & { ... }` brands.
private let references = [
    TS.AST.TypeReference(name: "Number", fileName: "/lib/lib.es5.d.ts"),
    TS.AST.TypeReference(name: "__type", fileName: "/proj/ids.ts"),
    TS.AST.TypeReference(name: "String", fileName: "/lib/lib.es5.d.ts"),
    TS.AST.TypeReference(name: "String", fileName: "/proj/String.ts"),
]

private func makeReference(_ index: Int) -> TS.AST.TSType {
    return TS.AST.TSType(name: "reference", typeReferenceIndex: index, isTypeParameter: false)
}

private func makeMember(name: String, type: TS.AST.TSType) -> TS.AST.PropertyLikeDeclaration {
    return TS.AST.PropertyLikeDeclaration(start: 0, end: 0, name: name, isOptional: false, type: type, leadingComments: nil)
}

private func exportModel(members: [TS.AST.PropertyLikeDeclaration]) throws -> ValdiModel {
    let interface = TS.AST.Interface(start: 0, end: 0, name: "Capture", members: members, typeParameters: nil, supertypes: nil)
    let dumpedSymbol = TS.DumpedSymbolWithComments(nodeType: "interface",
                                                   start: 0,
                                                   leadingComments: nil,
                                                   text: "Capture",
                                                   kind: .interfaceDeclaration,
                                                   modifiers: nil,
                                                   function: nil,
                                                   enum: nil,
                                                   interface: interface,
                                                   variable: nil,
                                                   exportedTypeAlias: nil)
    let commentedFile = TypeScriptCommentedFile(src: TypeScriptItemSrc(compilationPath: "proj/capture.ts", sourceURL: URL(fileURLWithPath: "/proj/capture.ts")),
                                                fileContent: "",
                                                dumpedSymbols: [dumpedSymbol],
                                                references: references)
    let exporter = TypeScriptNativeTypeExporter(iosType: nil,
                                                androidClass: nil,
                                                cppType: nil,
                                                commentedFile: commentedFile,
                                                annotatedSymbol: commentedFile.annotatedSymbols[0],
                                                dumpedSymbol: dumpedSymbol,
                                                nativeTypeResolver: TypeScriptNativeTypeResolver(rootURL: URL(fileURLWithPath: "/proj")))

    guard case .valdiModel(let model) = try exporter.export().waitForData().0 else {
        throw CompilerError("Expected a Valdi model")
    }
    return model
}

final class TypeScriptNativeTypeExporterTests: XCTestCase {

    func testIntersectionWithoutPrimitiveIsRejected() {
        let objectsOnly = TS.AST.TSType(name: "intersection", intersections: [makeReference(1), makeReference(1)])

        XCTAssertThrowsError(try exportModel(members: [makeMember(name: "value", type: objectsOnly)])) { error in
            XCTAssertTrue("\(error)".contains("exactly one string, number or boolean member"), "\(error)")
        }
    }

    func testIntersectionWithTwoPrimitivesIsRejected() {
        let twoPrimitives = TS.AST.TSType(name: "intersection", intersections: [makeReference(0), makeReference(2)])

        XCTAssertThrowsError(try exportModel(members: [makeMember(name: "value", type: twoPrimitives)]))
    }

    func testUserTypeNamedLikePrimitiveIsNotExportedAsPrimitive() {
        XCTAssertThrowsError(try exportModel(members: [makeMember(name: "value", type: makeReference(3))])) { error in
            XCTAssertTrue("\(error)".contains("Unrecognized type 'String'"), "\(error)")
        }
    }
}
