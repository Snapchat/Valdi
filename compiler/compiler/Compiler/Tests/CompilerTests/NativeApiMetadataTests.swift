import Foundation
import XCTest
@testable import Compiler

final class NativeApiMetadataTests: XCTestCase {
    private func apiDescription() -> GeneratedTypeDescription {
        return .function(
            GeneratedFunctionDescription(
                containingIosTypeName: nil,
                containingAndroidTypeName: nil,
                containingCppTypeName: nil,
                functionName: "exportedFunction",
                parameters: [],
                returnType: PropertyTypeDescription(propType: .void),
                declaredVersion: nil,
                effectiveVersion: "0"
            )
        )
    }

    func testVersionInheritance() {
        XCTAssertEqual(nativeApiEffectiveMemberVersion(declared: nil, container: "3"), "3")
        XCTAssertEqual(nativeApiEffectiveMemberVersion(declared: "2", container: "3"), "3")
        XCTAssertEqual(nativeApiEffectiveMemberVersion(declared: "7", container: "3"), "7")
        XCTAssertEqual(nativeApiEffectiveMemberVersion(declared: "__PLACEHOLDER__", container: "3"), "__PLACEHOLDER__")
        XCTAssertEqual(nativeApiEffectiveMemberVersion(declared: "future", container: "3"), "future")
        XCTAssertEqual(nativeApiEffectiveMemberVersion(declared: "2", container: "future"), "future")
    }

    func testVersionsAlwaysEncodeAsStringsOrNull() throws {
        let data = try JSONEncoder().encode(apiDescription())
        let object = try XCTUnwrap(JSONSerialization.jsonObject(with: data) as? [String: Any])
        let function = try XCTUnwrap(object["function"] as? [String: Any])

        XCTAssertTrue(function["declaredVersion"] is NSNull)
        XCTAssertEqual(function["effectiveVersion"] as? String, "0")
    }

    func testGeneratedModelDescriptionUsesExportedVersionMetadata() throws {
        var model = ValdiModel()
        model.declaredVersion = "2"
        model.isNativeApi = true
        model.properties = [
            ValdiModelProperty(
                name: "value",
                type: .string,
                comments: nil,
                omitConstructor: nil,
                injectableParams: .empty,
                declaredVersion: "5"
            ),
        ]

        let description = GeneratedNativeClassDescription(model: model, baseline: "0")
        XCTAssertTrue(description.isNativeApi)
        XCTAssertEqual(description.declaredVersion, "2")
        XCTAssertEqual(description.effectiveVersion, "2")
        XCTAssertEqual(description.properties[0].declaredVersion, "5")
        XCTAssertEqual(description.properties[0].effectiveVersion, "5")

        var ordinaryModel = ValdiModel()
        ordinaryModel.properties = [
            ValdiModelProperty(
                name: "value",
                type: .string,
                comments: nil,
                omitConstructor: nil,
                injectableParams: .empty,
                declaredVersion: nil
            ),
        ]

        let ordinaryDescription = GeneratedNativeClassDescription(model: ordinaryModel, baseline: "9")
        XCTAssertFalse(ordinaryDescription.isNativeApi)
        XCTAssertNil(ordinaryDescription.effectiveVersion)
    }

    func testCompilationMetadataEncodingAndLegacyDecoding() throws {
        let metadata = CompilationMetadata(
            classMappings: [:],
            nativeTypes: SerializedTypeScriptNativeTypeResolver(entries: []),
            nativeApiMinVersion: "0",
            generatedTypes: [
                GeneratedTypesSummary(
                    sourceFilePath: "module/src/Api",
                    generatedTypes: [apiDescription()]
                ),
            ]
        )
        let encoded = try JSONEncoder().encode(metadata)
        let object = try XCTUnwrap(JSONSerialization.jsonObject(with: encoded) as? [String: Any])

        XCTAssertEqual(object["nativeApiMinVersion"] as? String, "0")
        let generatedTypes = try XCTUnwrap(object["generatedTypes"] as? [[String: Any]])
        XCTAssertEqual(generatedTypes.count, 1)
        XCTAssertEqual(generatedTypes[0]["sourceFilePath"] as? String, "module/src/Api")

        let decoded = try JSONDecoder().decode(CompilationMetadata.self, from: encoded)
        XCTAssertTrue(decoded.generatedTypes.isEmpty)

        let unversionedMetadata = CompilationMetadata(
            classMappings: [:],
            nativeTypes: SerializedTypeScriptNativeTypeResolver(entries: []),
            nativeApiMinVersion: nil,
            generatedTypes: []
        )
        let unversionedData = try JSONEncoder().encode(unversionedMetadata)
        let unversionedObject = try XCTUnwrap(JSONSerialization.jsonObject(with: unversionedData) as? [String: Any])
        XCTAssertTrue(unversionedObject["nativeApiMinVersion"] is NSNull)

        let legacy = Data(#"{"classMappings":{},"nativeTypes":{"entries":[]}}"#.utf8)
        let decodedLegacy = try JSONDecoder().decode(CompilationMetadata.self, from: legacy)

        XCTAssertNil(decodedLegacy.nativeApiMinVersion)
        XCTAssertTrue(decodedLegacy.generatedTypes.isEmpty)
    }

    func testWireTypeDescriptionIsRecursive() throws {
        let callback = ValdiModelPropertyType.function(
            parameters: [
                ValdiModelProperty(
                    name: "values",
                    type: .array(elementType: .map(keyType: .string, valueType: .nullable(.long))),
                    comments: "not metadata",
                    omitConstructor: nil,
                    injectableParams: .empty,
                    declaredVersion: nil
                ),
            ],
            returnType: .promise(typeArgument: .genericTypeParameter(name: "T")),
            isSingleCall: true,
            shouldCallOnWorkerThread: true,
            allowSyncCall: true
        )
        let data = try JSONEncoder().encode(PropertyTypeDescription(propType: callback))
        let json = String(decoding: data, as: UTF8.self)

        XCTAssertTrue(json.contains(#""typeStr":"function""#))
        XCTAssertTrue(json.contains(#""typeStr":"array""#))
        XCTAssertTrue(json.contains(#""typeStr":"map""#))
        XCTAssertTrue(json.contains(#""typeStr":"nullable""#))
        XCTAssertTrue(json.contains(#""typeStr":"promise""#))
        XCTAssertTrue(json.contains(#""typeParameterName":"T""#))
        XCTAssertFalse(json.contains("declaredVersion"))
        XCTAssertFalse(json.contains("not metadata"))
        XCTAssertFalse(json.contains("sourcePosition"))
    }
}
