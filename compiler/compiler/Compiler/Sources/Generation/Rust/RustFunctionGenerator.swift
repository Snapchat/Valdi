// Copyright © 2026 Snap, Inc. All rights reserved.

import Foundation

final class RustFunctionGenerator {
    private let exportedFunction: ExportedFunction
    private let modulePath: String
    private let cppType: CPPType
    private let bundleInfo: CompilationItem.BundleInfo
    private let sourceFileName: GeneratedSourceFilename

    init(
        exportedFunction: ExportedFunction,
        modulePath: String,
        cppType: CPPType,
        bundleInfo: CompilationItem.BundleInfo,
        classMapping: ResolvedClassMapping,
        sourceFileName: GeneratedSourceFilename
    ) {
        self.exportedFunction = exportedFunction
        self.modulePath = modulePath
        self.cppType = cppType
        self.bundleInfo = bundleInfo
        self.sourceFileName = sourceFileName
        _ = classMapping
    }

    func write() throws -> [NativeSource] {
        let resolver = RustTypeResolver(
            bundleInfo: bundleInfo,
            moduleIncludeDir: cppType.includeDir,
            ownerName: cppType.declaration.name)
        let parameterNames = RustIdentifierScope()
        let parameters = try exportedFunction.parameters.map { parameter in
            (
                name: try parameterNames.allocate(
                    parameter.name, resolved: parameter.name.snakeCased.rustIdentifier),
                type: try resolver.resolve(
                    parameter.type, namePath: [exportedFunction.functionName, parameter.name])
            )
        }
        let returnType = try resolver.resolve(
            exportedFunction.returnType,
            namePath: [exportedFunction.functionName, "Return"])
        let rustName = cppType.declaration.name.rustTypeIdentifier

        let rustParameters = parameters.map { "\($0.name): \($0.type.rustBorrowedType)" }.joined(
            separator: ", ")
        let callMarshaller = parameters.isEmpty ? "_marshaller" : "marshaller"
        let serializedParameters = parameters.map { parameter in
            "                \(parameter.type.pushStatement(value: parameter.name, to: callMarshaller, asReturnValue: false, coreModule: "valdi_core_rust"))"
        }.joined(separator: "\n")
        let resultReader =
            returnType.rustOwnedType == "()"
            ? "|_| Ok(())"
            : "|value| \(returnType.deserializeExpression(from: "value", borrowed: false, coreModule: "valdi_core_rust"))"

        let rust = """
            pub struct \(rustName)(valdi_core_rust::Function);

            impl \(rustName) {
                pub fn resolve(runtime: valdi_core_rust::RuntimeRef<'_>) -> Result<Self, String> {
                    let value = valdi_core_rust::resolve_exported_function(
                        runtime,
                        "\("\(bundleInfo.name)/\(modulePath)".escapedForRustStringLiteral)",
                        \(RustSchemaGenerator.symbol(bundleName: bundleInfo.name, cppType: cppType))(),
                    )?;
                    Ok(Self(valdi_core_rust::Function::from_value(value)))
                }

                pub fn call(&self\(rustParameters.isEmpty ? "" : ", \(rustParameters)")) -> Result<\(returnType.rustOwnedType), String> {
                    self.0.as_borrowed().call_with(
                        |\(callMarshaller)| {
            \(serializedParameters)
                            Ok(())
                        },
                        \(resultReader),
                    )
                }
            }

            """

        let property = ValdiModelProperty(
            name: exportedFunction.functionName,
            type: .function(
                parameters: exportedFunction.parameters,
                returnType: exportedFunction.returnType,
                isSingleCall: false,
                shouldCallOnWorkerThread: false,
                allowSyncCall: exportedFunction.allowSyncCall),
            comments: exportedFunction.comments,
            omitConstructor: nil,
            injectableParams: .empty,
            declaredVersion: nil)
        let schemaWriter = RustSchemaWriter(typeParameters: nil)
        try schemaWriter.appendClass(
            cppType.declaration.fullTypeName,
            properties: [property],
            asyncStrictMode: bundleInfo.asyncStrictMode)
        let schema = RustSchemaGenerator.classRegistration(
            bundleInfo: bundleInfo,
            cppType: cppType,
            schema: schemaWriter.str,
            references: RustSchemaGenerator.references(in: [property]))

        let referencedTypes = parameters.map(\.type) + [returnType]
        return RustNativeSourceGenerator.generate(
            bundleInfo: bundleInfo,
            cppType: cppType,
            sourceFileName: sourceFileName,
            files: RustReferencedTypeGenerator.addDefinitions(
                requiredBy: referencedTypes,
                to: RustGeneratedFiles(rust: rust + schema, cHeader: "", registrationSymbols: [])))
    }
}
