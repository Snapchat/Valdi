// Copyright © 2026 Snap, Inc. All rights reserved.

import Foundation

private final class RustSchemaWriterListener: SchemaWriterListener {
    func getClassName(
        nodeMapping: ValdiNodeClassMapping,
        typeArguments: [ValdiModelPropertyType]?
    ) throws -> String? {
        guard nodeMapping.isGenerated else { return nil }
        guard let cppType = nodeMapping.cppType else {
            throw CompilerError("Missing native type for Rust schema declaration '\(nodeMapping.tsType)'")
        }
        return cppType.declaration.fullTypeName
    }
}

final class RustSchemaWriter: SchemaWriter {
    init(typeParameters: [ValdiTypeParameter]?) {
        super.init(
            typeParameters: typeParameters,
            listener: RustSchemaWriterListener(),
            alwaysBoxFunctionParametersAndReturnValue: false,
            boxIntEnums: false)
    }
}

struct RustSchemaGenerator {
    private init() {}

    static func symbol(bundleName: String, cppType: CPPType) -> String {
        return "valdi_\(bundleName)_\(cppType.declaration.fullTypeName)_schema".rustCSymbol.lowercased()
    }

    static func references(in properties: [ValdiModelProperty]) -> [ValdiNodeClassMapping] {
        var references: [ValdiNodeClassMapping] = []
        var seen = Set<String>()

        func add(_ mapping: ValdiNodeClassMapping) {
            guard mapping.isGenerated,
                let name = mapping.cppType?.declaration.fullTypeName,
                seen.insert(name).inserted
            else {
                return
            }
            references.append(mapping)
        }

        func visit(_ type: ValdiModelPropertyType) {
            switch type {
            case .object(let mapping), .enum(let mapping):
                add(mapping)
            case .genericObject(let mapping, let arguments):
                add(mapping)
                arguments.forEach(visit)
            case .array(let element), .promise(let element), .nullable(let element):
                visit(element)
            case .map(let key, let value):
                visit(key)
                visit(value)
            case .function(let parameters, let returnType, _, _, _):
                parameters.forEach { visit($0.type) }
                visit(returnType)
            case .string, .double, .bool, .long, .bigInt, .bytes, .any, .void, .genericTypeParameter:
                break
            }
        }

        properties.forEach { visit($0.type) }
        return references.sorted {
            $0.cppType!.declaration.fullTypeName < $1.cppType!.declaration.fullTypeName
        }
    }

    private static func registration(
        bundleInfo: CompilationItem.BundleInfo,
        cppType: CPPType,
        schema: String,
        references: [ValdiNodeClassMapping],
        kind: String
    ) -> String {
        let registrationSymbol = symbol(bundleName: bundleInfo.name, cppType: cppType)
        let dependencies = references.compactMap { mapping -> String? in
            guard let dependencyType = mapping.cppType else { return nil }
            let dependencySymbol = symbol(bundleName: bundleInfo.name, cppType: dependencyType)
            return dependencySymbol == registrationSymbol ? nil : dependencySymbol
        }.joined(separator: ", ")

        return """
            #[doc(hidden)]
            #[allow(non_snake_case)]
            pub extern "C" fn \(registrationSymbol)() -> *mut std::ffi::c_void {
                static SCHEMA: std::sync::OnceLock<usize> = std::sync::OnceLock::new();
                valdi_core_rust::register_schema_once(
                    &SCHEMA,
                    "\(schema.escapedForRustStringLiteral)",
                    valdi_core_rust::SchemaKind::\(kind),
                    &[\(dependencies)],
                )
            }

            """
    }

    static func classRegistration(
        bundleInfo: CompilationItem.BundleInfo,
        cppType: CPPType,
        schema: String,
        references: [ValdiNodeClassMapping]
    ) -> String {
        return registration(
            bundleInfo: bundleInfo,
            cppType: cppType,
            schema: schema,
            references: references,
            kind: "Class")
    }

    static func genericRegistration(
        bundleInfo: CompilationItem.BundleInfo,
        cppType: CPPType,
        schema: String,
        references: [ValdiNodeClassMapping]
    ) -> String {
        return registration(
            bundleInfo: bundleInfo,
            cppType: cppType,
            schema: schema,
            references: references,
            kind: "Generic")
    }

    static func enumRegistration(
        bundleInfo: CompilationItem.BundleInfo,
        cppType: CPPType,
        schema: String
    ) -> String {
        return registration(
            bundleInfo: bundleInfo,
            cppType: cppType,
            schema: schema,
            references: [],
            kind: "Enum")
    }
}
