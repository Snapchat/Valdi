//
//  KotlinCodeGenerator.swift
//  Compiler
//
//  Created by Simon Corsin on 5/17/18.
//  Copyright © 2018 Snap Inc. All rights reserved.
//

import Foundation

struct KotlinTypeParser {
    let typeName: String
    let functionTypeParser: KotlinFunctionTypeParser?
    let typeArguments: [KotlinTypeParser]?
    let fullTypeName: String

    init(
        typeName: String,
        functionTypeParser: KotlinFunctionTypeParser? = nil,
        typeArguments: [KotlinTypeParser]? = nil
    ) {
        self.typeName = typeName
        self.functionTypeParser = functionTypeParser
        self.typeArguments = typeArguments
        self.fullTypeName = KotlinTypeParser.getFullTypeName(typeName: typeName, typeArguments: typeArguments)
    }

    private static func getFullTypeName(typeName: String, typeArguments: [KotlinTypeParser]?) -> String {
        if let typeArguments = typeArguments {
            let typeNameWithoutOptionalMark = typeName.hasSuffix("?") ? String(typeName.dropLast()) : typeName
            let optionalMarkIfNeeded = typeName.hasSuffix("?") ? "?" : ""
            return "\(typeNameWithoutOptionalMark)<\(typeArguments.map { $0.fullTypeName }.joined(separator: ", "))>\(optionalMarkIfNeeded)"
        } else {
            return typeName
        }
    }
}

private final class KotlinImportSection: CodeWriterContent {

    private(set) var importedClasses = Set<String>()

    func addImport(fullClassName: String) {
        importedClasses.insert(fullClassName)
    }

    var content: String {
        if importedClasses.isEmpty {
            return ""
        }
        return importedClasses.sorted().map { "import \($0)\n" }.joined() + "\n"
    }

}

struct KotlinSchemaAnnotationResult {
    let typeReferences: CodeWriterContent
    let schema: CodeWriterContent
}

class KotlinFunctionTypeParser {
    let parameterNames: [PropertyName]
    let parameterTypes: [KotlinTypeParser]
    let returnType: KotlinTypeParser

    init(parameterNames: [PropertyName], parameterTypes: [KotlinTypeParser], returnType: KotlinTypeParser) {
        self.parameterNames = parameterNames
        self.parameterTypes = parameterTypes
        self.returnType = returnType
    }

    var parametersString: String {
        return parameterNames.enumerated().map { index, name in
            return "\(name): \(parameterTypes[index].fullTypeName)"
        }.joined(separator: ", ")
    }

    var lambdaTypeName: String {
        return "(\(parametersString)) -> \(returnType.fullTypeName)"
    }

    var methodTypeName: String {
        return "(\(parametersString)): \(returnType.fullTypeName)"
    }
}

private class KotlinSchemaWriterListener: SchemaWriterListener {
    private unowned let codeGenerator: KotlinCodeGenerator
    private let emittedTypeReferences: KotlinEmittedTypeReferences

    init(codeGenerator: KotlinCodeGenerator, emittedTypeReferences: KotlinEmittedTypeReferences) {
        self.codeGenerator = codeGenerator
        self.emittedTypeReferences = emittedTypeReferences
    }

    func getClassName(nodeMapping: ValdiNodeClassMapping, typeArguments: [ValdiModelPropertyType]?) throws -> String? {
        guard let androidClassName = nodeMapping.androidClassName else {
            throw CompilerError("No Android type declared")
        }

        let jvmClass = try codeGenerator.importClass(androidClassName)

        let identifierIndex = emittedTypeReferences.getIdentifierIndex(jvmClass.name)

        return "[\(identifierIndex)]"
    }

}

class KotlinPropertyReplacements: CodeWriterContent {

    var replacements = [(Int, String)]()

    var content: String {
        let stringContent = replacements.map { "\($0):'\($1)'" }.joined(separator: ",")
        return "\"\(stringContent)\""
    }

}

class KotlinSchema: CodeWriterContent {

    let schemaWriter: KotlinSchemaWriter

    init(typeParameters: [ValdiTypeParameter]?, listener: SchemaWriterListener) {
        self.schemaWriter = KotlinSchemaWriter(typeParameters: typeParameters, listener: listener)
    }

    var content: String {
        return "\"\(self.schemaWriter.str)\""
    }

}

class KotlinMarshallableObjectDescriptorAnnotation: CodeWriter {

    enum ObjectType {
        case valdiClass
        case valdiInterface
        case stringEnum
        case intEnum
        case valdiFunction
    }

    let type: ObjectType
    private unowned let generator: KotlinCodeGenerator
    private let schema: KotlinSchema
    private let emittedTypeReferences = KotlinEmittedTypeReferences()
    private let propertyReplacements = KotlinPropertyReplacements()

    init(type: ObjectType, generator: KotlinCodeGenerator, typeParameters: [ValdiTypeParameter]?, additionalParameters: [(String, CodeWriterContent)]) throws {
        self.type = type
        self.generator = generator
        self.schema = KotlinSchema(typeParameters: typeParameters, listener: KotlinSchemaWriterListener(codeGenerator: generator, emittedTypeReferences: self.emittedTypeReferences))

        super.init()

        var allParameters = [(String, CodeWriterContent)]()
        allParameters.append(("schema", schema))

        let annotationName: String
        switch type {
        case .valdiClass:
            annotationName = "ValdiClass"
            allParameters.append(("typeReferences", emittedTypeReferences))
            allParameters.append(("propertyReplacements", propertyReplacements))
        case .valdiInterface:
            annotationName = "ValdiInterface"
            allParameters.append(("typeReferences", emittedTypeReferences))
            allParameters.append(("propertyReplacements", propertyReplacements))
        case .stringEnum:
            annotationName = "ValdiEnum"
            allParameters.append(("propertyReplacements", propertyReplacements))
        case .intEnum:
            annotationName = "ValdiEnum"
            allParameters.append(("propertyReplacements", propertyReplacements))
        case .valdiFunction:
            annotationName = "ValdiFunctionClass"
            allParameters.append(("typeReferences", emittedTypeReferences))
            allParameters.append(("propertyReplacements", propertyReplacements))
        }
        allParameters.append(contentsOf: additionalParameters)

        appendBody(try generator.getJavaAnnotationContent(annotationType: "com.snap.valdi.schema.\(annotationName)", parameters: allParameters))
    }

    func appendField(propertyIndex: Int,
                     propertyName: String,
                     kotlinFieldName: String,
                     expectedKotlinFieldName: String,
                     schemaBuilder: (SchemaWriter) throws -> Void) throws -> CodeWriterContent {
        if propertyIndex > 0 {
            self.schema.schemaWriter.appendComma()
        }

        if kotlinFieldName != expectedKotlinFieldName {
            // Kotlin field name cannot be infered, add an entry in the mapping array
            let tuple = (propertyIndex, kotlinFieldName)
            propertyReplacements.replacements.append(tuple)
        }

        self.schema.schemaWriter.appendPropertyName(propertyName)
        try schemaBuilder(self.schema.schemaWriter)

        return try generator.getJavaAnnotationContent(annotationType: "com.snap.valdi.schema.ValdiField")
    }
}

final class KotlinCodeGenerator: CodeWriter {

    static let maxFunctionParametersCount = 16

    private let package: String?
    private let classMapping: ResolvedClassMapping?
    private let importSection = KotlinImportSection()

    init(package: String? = nil, classMapping: ResolvedClassMapping? = nil) {
        self.package = package
        self.classMapping = classMapping
        super.init()

        if let package = package {
            appendHeader("package \(package)\n\n")
        }

        appendHeader(importSection)
    }

    private func importClassIfNeeded(_ jvmClass: JVMClass) {
        if let package = package, jvmClass.package == package {
            return
        }

        importSection.addImport(fullClassName: jvmClass.fullClassName)
    }

    @discardableResult func importClass(_ className: String) throws -> JVMClass {
        if className.isEmpty {
            throw CompilerError("No class defined")
        }
        let jvmClass = JVMClass(fullClassName: className)

        importClassIfNeeded(jvmClass)

        return jvmClass
    }

    private func resolveTypeName(typeName: String, isOptional: Bool) -> String {
        return isOptional ? "\(typeName)?" : typeName
    }

    private func getPrimitiveTypeParser(primitiveType: String, propertyName: String?, isOptional: Bool) -> KotlinTypeParser {
        let resolvedType = resolveTypeName(typeName: primitiveType, isOptional: isOptional)
        return KotlinTypeParser(typeName: resolvedType)
    }

    private func getGenericTypeParameterTypeParser(name: String, propertyName: String?, isOptional: Bool, nameAllocator: PropertyNameAllocator) throws -> KotlinTypeParser {
        // properties with generic parameter types have to always accept optional values
        let isOptional = propertyName != nil || isOptional
        let resolvedTypeName = resolveTypeName(typeName: name, isOptional: isOptional)
        return KotlinTypeParser(typeName: resolvedTypeName)
    }

    private func getObjectTypeParser(typeName: String, mapping: ValdiNodeClassMapping, propertyName: String?, isOptional: Bool, nameAllocator: PropertyNameAllocator) -> KotlinTypeParser {
        let resolvedTypeName = resolveTypeName(typeName: typeName, isOptional: isOptional)
        return KotlinTypeParser(typeName: resolvedTypeName)
    }

    private func getGenericObjectTypeParser(typeName: String, mapping: ValdiNodeClassMapping, propertyName: String?, isOptional: Bool, nameAllocator: PropertyNameAllocator, typeArgumentsTypeParsers: [KotlinTypeParser]) throws -> KotlinTypeParser {
        let resolvedTypeName = resolveTypeName(typeName: typeName, isOptional: isOptional)
        let typeParser = KotlinTypeParser(
            typeName: resolvedTypeName,
            typeArguments: typeArgumentsTypeParsers
        )
        return typeParser
    }

    func getJavaAnnotationContent(annotationType: String) throws -> CodeWriterContent {
        return try getJavaAnnotationContent(annotationType: annotationType, parameters: [])
    }

    func getJavaAnnotationContent(annotationType: String, parameters: [(String, CodeWriterContent)]) throws -> CodeWriterContent {
        let annotationClassType = try importClass(annotationType)

        let output = CodeWriter()
        output.appendBody("@\(annotationClassType.name)")

        if !parameters.isEmpty {
            output.appendBody("(")
            if parameters.count  == 1 {
                output.appendBody("\(parameters[0].0) = ")
                output.appendBody(parameters[0].1)
            } else {
                var first = true
                for parameter in parameters {
                    if !first {
                        output.appendBody(",")
                    }
                    first = false
                    output.appendBody("\n    \(parameter.0) = ")
                    output.appendBody(parameter.1)
                }
                output.appendBody("\n" )
            }
            output.appendBody(")")
        }

        output.appendBody("\n")

        return output
    }

    func getDescriptorAnnotation(type: KotlinMarshallableObjectDescriptorAnnotation.ObjectType, typeParameters: [ValdiTypeParameter]?, additionalParameters: [(String, CodeWriterContent)]) throws -> KotlinMarshallableObjectDescriptorAnnotation {
        return try KotlinMarshallableObjectDescriptorAnnotation(type: type, generator: self, typeParameters: typeParameters, additionalParameters: additionalParameters)
    }

    private func getFunctionTypeParser(returnType: ValdiModelPropertyType,
                               parameters: [ValdiModelProperty],
                               nameAllocator: PropertyNameAllocator) throws -> KotlinFunctionTypeParser {
        guard parameters.count <= KotlinCodeGenerator.maxFunctionParametersCount else {
            throw CompilerError("Exported functions to Kotlin can only support up to \(KotlinCodeGenerator.maxFunctionParametersCount) parameters.")
        }

        let parameterNames = parameters.map { nameAllocator.allocate(property: $0.name) }
        let parameterParsers = try parameters.map { try getTypeParser(type: $0.type.unwrappingOptional, isOptional: $0.type.isOptional, propertyName: nil, nameAllocator: nameAllocator) }
        let returnParser = try getTypeParser(type: returnType.unwrappingOptional, isOptional: returnType.isOptional, propertyName: nil, nameAllocator: nameAllocator)

        return KotlinFunctionTypeParser(parameterNames: parameterNames, parameterTypes: parameterParsers, returnType: returnParser)
    }

    func getTypeParser(type: ValdiModelPropertyType, isOptional: Bool, propertyName: String?, nameAllocator: PropertyNameAllocator) throws -> KotlinTypeParser {
        switch type {
        case .string:
            return getPrimitiveTypeParser(primitiveType: "String", propertyName: propertyName, isOptional: isOptional)
        case .double:
            return getPrimitiveTypeParser(primitiveType: "Double", propertyName: propertyName, isOptional: isOptional)
        case .bool:
            return getPrimitiveTypeParser(primitiveType: "Boolean", propertyName: propertyName, isOptional: isOptional)
        case .long:
            return getPrimitiveTypeParser(primitiveType: "Long", propertyName: propertyName, isOptional: isOptional)
        case .array(let elementType):
            let childParser = try getTypeParser(type: elementType.unwrappingOptional, isOptional: elementType.isOptional, propertyName: nil, nameAllocator: nameAllocator)
            let typeName = "List<\(childParser.fullTypeName)>"

            return KotlinTypeParser(typeName: resolveTypeName(typeName: typeName, isOptional: isOptional))
        case .bytes:
            return getPrimitiveTypeParser(primitiveType: "ByteArray", propertyName: propertyName, isOptional: isOptional)
        case .map:
            let typeName = resolveTypeName(typeName: "Map<String, Any?>", isOptional: isOptional)
            return KotlinTypeParser(typeName: typeName)
        case .any:
            let isOptional = true
            let typeName = resolveTypeName(typeName: "Any", isOptional: isOptional)
            return KotlinTypeParser(typeName: typeName)
        case .void:
            let typeName = "Unit"
            return KotlinTypeParser(typeName: typeName)
        case .function(let parameters, let returnType, _, _):
            let functionTypeParser = try getFunctionTypeParser(returnType: returnType, parameters: parameters, nameAllocator: nameAllocator)

            let typeName = functionTypeParser.lambdaTypeName

            let resolvedTypeName: String
            if isOptional {
                resolvedTypeName = "(\(typeName))?"
            } else {
                resolvedTypeName = typeName
            }

            return KotlinTypeParser(typeName: resolvedTypeName, functionTypeParser: functionTypeParser)
        case .enum(let mapping):
            guard let resolvedType = mapping.androidClassName else {
                throw CompilerError("No android type declared")
            }

            let typeName = try importClass(resolvedType).name
            return getObjectTypeParser(typeName: typeName, mapping: mapping, propertyName: propertyName, isOptional: isOptional, nameAllocator: nameAllocator)
        case .object(let mapping):
            guard let resolvedType = mapping.androidClassName else {
                throw CompilerError("No android class declared")
            }

            let typeName = try importClass(resolvedType).name
            return getObjectTypeParser(typeName: typeName, mapping: mapping, propertyName: propertyName, isOptional: isOptional, nameAllocator: nameAllocator)
        case .genericTypeParameter(let name):
            return try getGenericTypeParameterTypeParser(name: name, propertyName: propertyName, isOptional: isOptional, nameAllocator: nameAllocator)
        case .genericObject(let mapping, let typeArguments):
            guard let resolvedType = mapping.androidClassName else {
                throw CompilerError("No android class declared")
            }

            let typeArgumentsTypeParsers = try typeArguments.map {
                try getTypeParser(type: $0, isOptional: false, propertyName: nil, nameAllocator: nameAllocator)
            }

            let typeName = try importClass(resolvedType).name
            return try getGenericObjectTypeParser(typeName: typeName, mapping: mapping, propertyName: propertyName, isOptional: isOptional, nameAllocator: nameAllocator, typeArgumentsTypeParsers: typeArgumentsTypeParsers)
        case .promise(let typeArgument):
            let promiseTypeName = try importClass("com.snap.valdi.promise.Promise").name
            let childParser = try getTypeParser(type: typeArgument.unwrappingOptional, isOptional: typeArgument.isOptional, propertyName: nil, nameAllocator: nameAllocator)
            let typeName = "\(promiseTypeName)<\(childParser.fullTypeName)>"

            return getPrimitiveTypeParser(primitiveType: typeName, propertyName: nil, isOptional: isOptional)
        case .nullable(let type):
            return try getTypeParser(type: type, isOptional: true, propertyName: propertyName, nameAllocator: nameAllocator)
        }
    }

    static func makeNativeSource(bundleInfo: CompilationItem.BundleInfo, jvmClass: JVMClass, file: File, fileNameOverride: String? = nil) -> NativeSource {
        let filename = fileNameOverride ?? "\(jvmClass.name).\(FileExtensions.kotlin)"

        return NativeSource(relativePath: jvmClass.filePath,
                            filename: filename,
                            file: file,
                            groupingIdentifier: "\(bundleInfo.name).\(FileExtensions.kotlin)",
                            groupingPriority: 0)
    }

}
