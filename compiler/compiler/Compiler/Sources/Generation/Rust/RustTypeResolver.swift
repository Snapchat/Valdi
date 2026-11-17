// Copyright © 2026 Snap, Inc. All rights reserved.

import Foundation

final class RustTypeResolver {
    private let bundleInfo: CompilationItem.BundleInfo
    private let moduleIncludeDir: String?
    private let ownerName: String

    init(bundleInfo: CompilationItem.BundleInfo, moduleIncludeDir: String?, ownerName: String) {
        self.bundleInfo = bundleInfo
        self.moduleIncludeDir = moduleIncludeDir
        self.ownerName = ownerName
    }

    func resolve(_ type: ValdiModelPropertyType, namePath: [String]) throws -> RustType {
        switch type {
        case .void: return .void
        case .bool: return .bool
        case .double: return .double
        case .bigInt: return .handle(rust: "valdi_core_rust::BigInt", borrowed: "valdi_core_rust::BigIntRef<'_>")
        case .long: return .long
        case .string: return .string
        case .bytes: return .bytes
        case .enum(let mapping):
            guard mapping.isGenerated else { return valueType() }
            try validateLocal(mapping)
            return .enumType((mapping.cppType?.declaration.name ?? mapping.tsType).rustTypeIdentifier)
        case .object(let mapping):
            guard mapping.isGenerated else { return valueType() }
            try validateLocal(mapping)
            let name = (mapping.cppType?.declaration.name ?? mapping.tsType).rustTypeIdentifier
            return mapping.kind == .interface
                ? .handle(rust: name, borrowed: "\(name)Ref<'_>")
                : .model(rust: name, borrowed: "\(name)Ref<'_>")
        case .genericObject(let mapping, let arguments):
            guard mapping.isGenerated else { return valueType() }
            try validateLocal(mapping)
            let name = (mapping.cppType?.declaration.name ?? mapping.tsType).rustTypeIdentifier
            let resolvedArguments = try arguments.enumerated().map { index, argument in
                try resolve(argument, namePath: namePath + ["Argument\(index)"])
            }
            let ownedArguments = resolvedArguments.map(\.rustOwnedType).joined(separator: ", ")
            let borrowedArguments = resolvedArguments.map(\.rustNestedBorrowedType).joined(separator: ", ")
            return .model(
                rust: "\(name)<\(ownedArguments)>",
                borrowed: "\(name)Ref<'_, \(borrowedArguments)>")
        case .array(let element):
            return .valueArray(try resolve(element, namePath: namePath + ["Element"]))
        case .map(let key, let value):
            // Valdi's native map contract is type-erased. Resolve both declared
            // types only to enforce the local native-declaration boundary.
            _ = try resolve(key, namePath: namePath + ["Key"])
            _ = try resolve(value, namePath: namePath + ["Value"])
            return .handle(
                rust: "valdi_core_rust::Map<String, valdi_core_rust::Value>",
                borrowed: "valdi_core_rust::MapRef<'_, String, valdi_core_rust::Value>")
        case .any:
            return valueType()
        case .function(let parameters, let returnType, _, _, _):
            let baseName = ([ownerName] + namePath).map(\.pascalCased).joined()
            let callbackName = (baseName.hasSuffix("Callback") ? baseName : baseName + "Callback")
                .rustTypeIdentifier
            let parameterNames = RustIdentifierScope()
            let resolvedParameters = try parameters.map { parameter in
                RustCallbackParameter(
                    name: try parameterNames.allocate(
                        parameter.name, resolved: parameter.name.snakeCased.rustIdentifier),
                    type: try resolve(parameter.type, namePath: namePath + [parameter.name]))
            }
            return .callback(
                RustCallbackType(
                    rustName: callbackName,
                    parameters: resolvedParameters,
                    returnType: try resolve(returnType, namePath: namePath + ["Return"])))
        case .promise(let value):
            let baseName = ([ownerName] + namePath + ["Future"]).map(\.pascalCased).joined()
            return .future(
                RustFutureType(
                    rustName: baseName.rustTypeIdentifier,
                    value: try resolve(value, namePath: namePath + ["Value"])))
        case .genericTypeParameter(let name):
            return .genericParameter(name.rustTypeIdentifier)
        case .nullable(let value):
            return .nullable(try resolve(value, namePath: namePath))
        }
    }

    private func valueType() -> RustType {
        .handle(
            rust: "valdi_core_rust::Value",
            borrowed: "valdi_core_rust::ValueRef<'_>")
    }

    private func validateLocal(_ mapping: ValdiNodeClassMapping) throws {
        guard mapping.isGenerated else { return }
        try RustValidation.validateDeclarationIsLocal(
            tsType: mapping.tsType,
            declarationIncludeDir: mapping.cppType?.includeDir,
            moduleIncludeDir: moduleIncludeDir,
            bundleName: bundleInfo.name)
    }
}
