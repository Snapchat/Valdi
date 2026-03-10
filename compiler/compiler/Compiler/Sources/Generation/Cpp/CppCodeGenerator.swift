//
//  CppCodeGenerator.swift
//  
//
//  Created by Simon Corsin on 4/11/23.
//

import Foundation

struct CppMethodParameter {
    let name: String
    let typeNameResolver: CPPTypeNameResolver
}

struct CppMethod {
    let returnTypeNameResolver: CPPTypeNameResolver
    let parameters: [CppMethodParameter]
}

struct CPPTypeNameResolver: CustomStringConvertible {
    let resolve: (CPPNamespace) -> String

    init(_ resolve: @escaping (CPPNamespace) -> String) {
        self.resolve = resolve
    }

    var description: String {
        return resolve(CPPNamespace.rootNamespace)
    }

    static func with(templateType: CPPTypeNameResolver, typeArguments: [CPPTypeNameResolver]) -> CPPTypeNameResolver {
        return CPPTypeNameResolver { namespace in
            let typeName = templateType.resolve(namespace)
            let typeArgumentsStr = typeArguments.map { $0.resolve(namespace) }.joined(separator: ", ")
            return "\(typeName)<\(typeArgumentsStr)>"
        }
    }

    static func with(templateType: CPPTypeNameResolver, typeArgument: CPPTypeNameResolver) -> CPPTypeNameResolver {
        return CPPTypeNameResolver { namespace in
            let typeName = templateType.resolve(namespace)
            let typeArgumentStr = typeArgument.resolve(namespace)
            return "\(typeName)<\(typeArgumentStr)>"
        }
    }

    static func with(typeName: String) -> CPPTypeNameResolver {
        return CPPTypeNameResolver { _ in typeName }
    }

    static func with(typeDeclaration: CPPTypeDeclaration) -> CPPTypeNameResolver {
        return CPPTypeNameResolver { namespace in typeDeclaration.resolveTypeName(inNamespace: namespace) }
    }

    struct Specifiers: OptionSet {
        let rawValue: Int

        static let const = Specifiers(rawValue: 1 << 0)
        static let ref = Specifiers(rawValue: 1 << 1)
        static let rValueRef = Specifiers(rawValue: 1 << 2)
        static let ptr = Specifiers(rawValue: 1 << 3)

        static let constRef: Specifiers = [.const, .ref]
    }

    static func with(specifiers: Specifiers, _ typeNameResolver: CPPTypeNameResolver) -> CPPTypeNameResolver {
        return CPPTypeNameResolver { namespace in
            var output = ""
            if specifiers.contains(.const) {
                output += "const "
            }
            output += typeNameResolver.resolve(namespace)

            if specifiers.contains(.ref) {
                output += " &"
            } else if specifiers.contains(.ptr) {
                output += " *"
            } else if specifiers.contains(.rValueRef) {
                output += " &&"
            }

            return output
        }
    }
}

struct CppTypeParser {
    let typeNameResolver: CPPTypeNameResolver
    let method: CppMethod?
    let isMovable: Bool
    let isIntrinsicallyNullable: Bool
    let defaultInitializationString: String?

    init(typeNameResolver: CPPTypeNameResolver,
         method: CppMethod?,
         isMovable: Bool,
         isIntrinsicallyNullable: Bool,
         defaultInitializationString: String?) {
        self.typeNameResolver = typeNameResolver
        self.method = method
        self.isMovable = isMovable
        self.isIntrinsicallyNullable = isIntrinsicallyNullable
        self.defaultInitializationString = defaultInitializationString
    }
}

final class CppIncludeSection: CodeWriterContent {

    private var includedFiles = Set<String>()

    @discardableResult func addInclude(path: String) -> Bool {
        return includedFiles.insert("\"\(path)\"").inserted
    }

    @discardableResult func addSystemInclude(path: String) -> Bool {
        return includedFiles.insert("<\(path)>").inserted
    }

    var content: String {
        return includedFiles.sorted().map { "#include \($0)\n" }.joined()
    }
}

final class CppForwardDeclarationSection: CodeWriterContent {
    private var includedTypes = Set<CPPTypeReference>()

    func addForwardDeclaration(typeReference: CPPTypeReference) {
        includedTypes.insert(typeReference)
    }

    var content: String {
        if includedTypes.isEmpty {
            return ""
        }

        let typeDeclarationsByNamespace = includedTypes.groupBy { $0.declaration.namespace }
        let namespaces = typeDeclarationsByNamespace.keys.sorted()

        let body = CodeWriter()

        for namespace in namespaces {
            body.appendBody("namespace \(namespace.description) {\n")

            let types = typeDeclarationsByNamespace[namespace]!.map {
                if let typeArguments = $0.typeArguments {
                    let typeArgumentsStr = typeArguments.map { "typename \($0)" }.joined(separator: ", ")
                    let prefix = "template<\(typeArgumentsStr)>\n"
                    return "\(prefix)\($0.declaration.symbolType.declarationString) \($0.declaration.name);\n"
                } else {
                    return "\($0.declaration.symbolType.declarationString) \($0.declaration.name);\n"
                }
            }.sorted()

            for t in types {
                body.appendBody(t)
            }

            body.appendBody("}\n\n")
        }

        return body.content
    }
}

class CppCodeWriter: CodeWriter {

}

struct CPPFunctionArgument {
    let typeResolver: CPPTypeNameResolver
    let name: String

    func resolve(namespace: CPPNamespace) -> String {
        var output = typeResolver.resolve(namespace)

        if output.last == "*" || output.last == "&" {
            output += name
        } else {
            output += " "
            output += name
        }

        return output
    }
}

enum CppMethodSpecifiers {
    case none
    case const
    case static_
}

struct CppClassWriter {
    let namespace: CPPNamespace
    let className: String
    let header: CppCodeWriter
    let impl: CppCodeWriter
    let inlineImplementation: Bool

    private let headerNamespace: CPPNamespace

    init(namespace: CPPNamespace, className: String, header: CppCodeWriter, impl: CppCodeWriter, inlineImplementation: Bool) {
        self.namespace = namespace
        self.className = className
        self.header = header
        self.impl = impl
        self.inlineImplementation = inlineImplementation
        self.headerNamespace = namespace.appending(component: className)
    }

    private func resolveMethodReturnType(namespace: CPPNamespace, returnTypeResolver: CPPTypeNameResolver) -> String {
        let returnType = returnTypeResolver.resolve(namespace)
        if returnType.last == "*" || returnType.last == "&" {
            return returnType
        } else {
            return "\(returnType) "
        }
    }

    private func resolveArguments(namespace: CPPNamespace, arguments: [CPPFunctionArgument]) -> String {
        return arguments.map { $0.resolve(namespace: namespace) }.joined(separator: ", ")
    }

    func writeMethod(name: String, arguments: [CPPFunctionArgument], returnType: CPPTypeNameResolver, specifiers: CppMethodSpecifiers, writeBody: (CodeWriter) -> Void) {
        if specifiers == .static_ {
            header.appendBody("static ")
        }

        if inlineImplementation {
            let methodPrefix = resolveMethodReturnType(namespace: headerNamespace, returnTypeResolver: returnType)
            header.appendBody("\(methodPrefix)\(name)(\(self.resolveArguments(namespace: headerNamespace, arguments: arguments)))")
            if specifiers == .const {
                header.appendBody(" const")
            }
            writeMethodImpl(writer: header, writeBody: writeBody)
        } else {
            let methodPrefixHeader = resolveMethodReturnType(namespace: headerNamespace, returnTypeResolver: returnType)
            let methodPrefixImpl = resolveMethodReturnType(namespace: namespace, returnTypeResolver: returnType)
            header.appendBody("\(methodPrefixHeader)\(name)(\(self.resolveArguments(namespace: headerNamespace, arguments: arguments)))")
            if specifiers == .const {
                header.appendBody(" const")
            }
            header.appendBody(";\n\n")
            impl.appendBody("\(methodPrefixImpl)\(className)::\(name)(\(self.resolveArguments(namespace: namespace, arguments: arguments)))")
            if specifiers == .const {
                impl.appendBody(" const")
            }
            writeMethodImpl(writer: impl, writeBody: writeBody)
        }
    }

    func writeConstructor(arguments: [CPPFunctionArgument], memberInitializerList: String, writeBody: (CodeWriter) -> Void) {
        if inlineImplementation {
            header.appendBody("\(className)(\(self.resolveArguments(namespace: headerNamespace, arguments: arguments)))")
            if !memberInitializerList.isEmpty {
                header.appendBody(": \(memberInitializerList)")
            }
            writeMethodImpl(writer: header, writeBody: writeBody)
        } else {
            header.appendBody("\(className)(\(self.resolveArguments(namespace: headerNamespace, arguments: arguments)));\n\n")
            impl.appendBody("\(className)::\(className)(\(self.resolveArguments(namespace: namespace, arguments: arguments)))")
            if !memberInitializerList.isEmpty {
                impl.appendBody(": \(memberInitializerList)")
            }
            writeMethodImpl(writer: impl, writeBody: writeBody)
        }
    }

    private func writeMethodImpl(writer: CodeWriter, writeBody: (CodeWriter) -> Void) {
        writer.appendBody(" {\n")
        writeBody(writer)
        writer.appendBody("}\n\n")
    }
}

final class CppFileGenerator: CodeWriterContent {

    static let valdiNamespace = CPPNamespace(components: ["Valdi"])

    var content: String {
        return writer.content
    }

    let includeSection = CppIncludeSection()
    let forwardDeclarations = CppForwardDeclarationSection()

    let body = CppCodeWriter()

    private let writer = CodeWriter()

    init(namespace: CPPNamespace, isHeader: Bool) {
        if isHeader {
            writer.appendHeader("#pragma once\n")
        }
        writer.appendHeader(includeSection)
        writer.appendHeader("\n")

        if namespace.components.isEmpty {
            writer.appendBody(forwardDeclarations)
            writer.appendBody(body)
        } else {
            writer.appendBody(forwardDeclarations)
            writer.appendBody("namespace \(namespace.description) {\n\n")
            writer.appendBody(body)
            writer.appendBody("\n}")
        }
    }

    static func getValdiTypeDeclaration(typeName: String) -> CPPTypeDeclaration {
        return CPPTypeDeclaration(namespace: CppFileGenerator.valdiNamespace, name: typeName, symbolType: .class)
    }
}

struct CppTypeAlias {
    let statement: CPPTypeNameResolver
    let isOnMethod: Bool
}

protocol CppCodeGeneratorNamespaceResolver: AnyObject {
    func getNamespace(forTypeAlias typeAlias: CppTypeAlias) -> CPPNamespace
}


final class CppCodeGeneratorSingleNamespaceResolver: CppCodeGeneratorNamespaceResolver {
    private let classNamespace: CPPNamespace

    init(classNamespace: CPPNamespace) {
        self.classNamespace = classNamespace
    }

    func getNamespace(forTypeAlias typeAlias: CppTypeAlias) -> CPPNamespace {
        return classNamespace
    }
}

final class CppCodeGenerator {

    let header: CppFileGenerator
    let impl: CppFileGenerator

    private var referencedTypesIndex = Set<CPPTypeReference>()
    private(set) var referencedTypes: [CPPTypeReference] = []
    private(set) var referenceTypeKeys: [String] = []
    private(set) var typealiases: [CppTypeAlias] = []
    private(set) var dependenciesInSameHeaderFile: [String] = []

    private let selfIncludePath: String
    private let namespaceResolver: CppCodeGeneratorNamespaceResolver

    init(namespace: CPPNamespace,
         selfIncludePath: String,
         namespaceResolver: CppCodeGeneratorNamespaceResolver) {
        self.header = CppFileGenerator(namespace: namespace, isHeader: true)
        self.impl = CppFileGenerator(namespace: namespace, isHeader: false)
        self.selfIncludePath = selfIncludePath
        self.namespaceResolver = namespaceResolver
    }

    static func getValdiTypeName(typeName: String) -> CPPTypeNameResolver {
        let typeDeclaration = CppFileGenerator.getValdiTypeDeclaration(typeName: typeName)

        return CPPTypeNameResolver.with(typeDeclaration: typeDeclaration)
    }

    static func makeRefType(typeName: CPPTypeNameResolver) -> CPPTypeNameResolver {
        let refTypeName = getValdiTypeName(typeName: "Ref")

        return CPPTypeNameResolver.with(templateType: refTypeName, typeArgument: typeName)
    }

    private static func resolveGetRegisteredClassExpression(namespace: CPPNamespace, typeReference: CPPTypeReference) -> String {
        let resolvedTypeName = typeReference.resolveTypeName(inNamespace: namespace)
        switch typeReference.declaration.symbolType {
        case .class:
            return "\(resolvedTypeName)::getRegisteredClass()"
        case .enum:
            return "::\(typeReference.declaration.namespace.description)::getRegisteredEnumClass(static_cast<const \(resolvedTypeName) *>(nullptr))"
        }
    }

    func getTypeReferencesVecExpression(inNamespace namespace: CPPNamespace) -> String {
        let typeReferencesVecString = referencedTypes.map { CppCodeGenerator.resolveGetRegisteredClassExpression(namespace: namespace, typeReference: $0) }.joined(separator: ", ")

        return """
        []() -> Valdi::TypeReferencesVec {
            return {\(typeReferencesVecString)};
         }
        """
    }

    private func getPrimitiveTypeParser(primitiveType: String,
                                        isMovable: Bool,
                                        isIntrinsicallyNullable: Bool,
                                        defaultInitializationString: String?) -> CppTypeParser {
        return CppTypeParser(
            typeNameResolver: CPPTypeNameResolver.with(typeName: primitiveType),
            method: nil,
            isMovable: isMovable,
            isIntrinsicallyNullable: isIntrinsicallyNullable,
            defaultInitializationString: defaultInitializationString
        )
    }

    private static func elementTypeIsObject(_ elementType: ValdiModelPropertyType) -> Bool {
        switch elementType {
        case .object:
            return true
        case .genericObject:
            return true
        default:
            return false
        }
    }

    private func toOptionalTypeParser(elementType: ValdiModelPropertyType, typeParser: CppTypeParser) -> CppTypeParser {
        if typeParser.isIntrinsicallyNullable {
            return typeParser
        }

        if CppCodeGenerator.elementTypeIsObject(elementType) {
            // For optional of object references, we turn them into Ref<> so that we can support
            // circular type references.
            let refType = CppCodeGenerator.makeRefType(typeName: typeParser.typeNameResolver)
            return CppTypeParser(
                typeNameResolver: refType,
                method: typeParser.method,
                isMovable: true,
                isIntrinsicallyNullable: true,
                defaultInitializationString: nil)
        } else {
            let parentTypeNameResolver = typeParser.typeNameResolver
            return CppTypeParser(
                typeNameResolver: CPPTypeNameResolver.with(templateType: .with(typeName: "std::optional"), typeArgument: parentTypeNameResolver),
                method: typeParser.method,
                isMovable: typeParser.isMovable,
                isIntrinsicallyNullable: true,
                defaultInitializationString: nil)
        }
    }

    private func getArrayTypeParser(elementType: ValdiModelPropertyType, namePaths: [String], nameAllocator: PropertyNameAllocator) throws -> CppTypeParser {
        let elementTypeParser = try getTypeParser(type: elementType, namePaths: namePaths + ["Element"], nameAllocator: nameAllocator)

        return CppTypeParser(typeNameResolver: CPPTypeNameResolver.with(templateType: .with(typeName: "std::vector"), typeArgument: elementTypeParser.typeNameResolver),
                             method: nil,
                             isMovable: true,
                             isIntrinsicallyNullable: false,
                             defaultInitializationString: nil)
    }

    private func appendReferencedType(_ referencedType: CPPTypeReference, propertyType: ValdiModelPropertyType, includePath: String, isRefType: Bool) {
        let fromSameIncludePath = includePath == self.selfIncludePath
        if !fromSameIncludePath {
            header.includeSection.addInclude(path: includePath)
        }

        if let typeArguments = referencedType.typeArguments {
            let forwardDeclaredTypeArguments = typeArguments.enumerated().map { "T\($0.offset)" }
            header.forwardDeclarations.addForwardDeclaration(typeReference: CPPTypeReference(declaration: referencedType.declaration, typeArguments: forwardDeclaredTypeArguments))
        } else {
            header.forwardDeclarations.addForwardDeclaration(typeReference: referencedType)
        }

        let inserted = referencedTypesIndex.insert(referencedType).inserted
        if inserted {
            if fromSameIncludePath && !isRefType {
                // If we are depending on a file in the same header and that the type is not boxed in a Ref, then we have
                // a hard dependency on that type and the referenced type definition needs to appear before our type when
                // using single file codegen.
                dependenciesInSameHeaderFile.append("\(referencedType.declaration.name).hpp")
            }
            referencedTypes.append(referencedType)
            referenceTypeKeys.append(CppCodeGenerator.makeCanonicalTypeKey(type: propertyType))
        }
    }

    private func getTypeReferenceTypeParser(referencedType: CPPType,
                                            referencedPropertyType: ValdiModelPropertyType,
                                            isObject: Bool,
                                            isRefType: Bool,
                                            nameAllocator: PropertyNameAllocator) -> CppTypeParser {
        appendReferencedType(CPPTypeReference(declaration: referencedType.declaration, typeArguments: nil),
                             propertyType: referencedPropertyType,
                             includePath: referencedType.includePath,
                             isRefType: isRefType)

        return CppTypeParser(
            typeNameResolver: isRefType ? CppCodeGenerator.makeRefType(typeName: .with(typeDeclaration: referencedType.declaration)) : .with(typeDeclaration: referencedType.declaration),
            method: nil,
            isMovable: isObject,
            isIntrinsicallyNullable: isRefType,
            defaultInitializationString: nil
        )
    }

    private func getGenericTypeReferenceTypeParser(referencedType: CPPType,
                                                   referencedPropertyType: ValdiModelPropertyType,
                                                   referenceTypeIsInterface: Bool,
                                                   typeParsers: [CppTypeParser],
                                                   nameAllocator: PropertyNameAllocator) -> CppTypeParser {
        let typeReference = CPPTypeReference(declaration: referencedType.declaration, typeArguments: typeParsers.map { $0.typeNameResolver.resolve(.rootNamespace) })
        appendReferencedType(typeReference, propertyType: referencedPropertyType, includePath: referencedType.includePath, isRefType: false)

        let resolveTypeArguments = typeParsers.map { $0.typeNameResolver }

        let typeNameResolver = CPPTypeNameResolver.with(templateType: .with(typeDeclaration: referencedType.declaration), typeArguments: resolveTypeArguments)

        return CppTypeParser(
            typeNameResolver: referenceTypeIsInterface ? CppCodeGenerator.makeRefType(typeName: typeNameResolver) : typeNameResolver,
            method: nil,
            isMovable: true,
            isIntrinsicallyNullable: referenceTypeIsInterface,
            defaultInitializationString: nil
        )
    }

    private func getUntypedTypeParser() -> CppTypeParser {
        header.includeSection.addInclude(path: "valdi_core/cpp/Utils/Value.hpp")

        return CppTypeParser(
            typeNameResolver: CppCodeGenerator.getValdiTypeName(typeName: "Value"),
            method: nil,
            isMovable: true,
            isIntrinsicallyNullable: true,
            defaultInitializationString: nil
        )
    }

    private func getPromiseTypeParser(valueTypeParser: CppTypeParser) -> CppTypeParser {
        header.includeSection.addInclude(path: "valdi_core/cpp/Utils/Future.hpp")

        return CppTypeParser(
            typeNameResolver: CPPTypeNameResolver.with(templateType: CppCodeGenerator.getValdiTypeName(typeName: "Future"), typeArgument: valueTypeParser.typeNameResolver),
            method: nil,
            isMovable: true,
            isIntrinsicallyNullable: false,
            defaultInitializationString: nil
        )
    }

    private func getFunctionTypeParser(parameters: [ValdiModelProperty], returnType: ValdiModelPropertyType, namePaths: [String], nameAllocator: PropertyNameAllocator) throws -> CppTypeParser {
        header.includeSection.addInclude(path: "valdi_core/cpp/Utils/Function.hpp")

        let returnTypeParser = try getTypeParser(type: returnType, namePaths: namePaths + ["Return"], nameAllocator: nameAllocator)

        let parameterTypeParsers = try parameters.map { ($0.name, try getTypeParser(type: $0.type, namePaths: namePaths + [$0.name], nameAllocator: nameAllocator)) }

        let parameterTypeNameResolvers = parameterTypeParsers.map { $1.typeNameResolver }

        let typealiasFunctionName = nameAllocator.allocate(property: (namePaths + ["Fn"]).map { $0.pascalCased }.joined()).name

        let functionType = CppCodeGenerator.getValdiTypeName(typeName: "Function")
        let returnTypeNameResolver = returnTypeParser.typeNameResolver

        let realTypeNameResolver = CPPTypeNameResolver.with(templateType: functionType, typeArgument: CPPTypeNameResolver { namespace in
            let returnTypeName = returnTypeNameResolver.resolve(namespace)
            let parameterTypeNames = parameterTypeNameResolvers.map { $0.resolve(namespace) }.joined(separator: ", ")
            return "\(returnTypeName)(\(parameterTypeNames))"
        })

        var cppMethod: CppMethod?
        if namePaths.count == 1 {
            cppMethod = CppMethod(returnTypeNameResolver: returnTypeNameResolver, parameters: parameterTypeParsers.map { CppMethodParameter(name: $0, typeNameResolver: $1.typeNameResolver) })
        }

        let typealiasStatement = CPPTypeNameResolver { namespace in
            return "using \(typealiasFunctionName) = \(realTypeNameResolver.resolve(namespace));\n"
        }

        let typeAlias = CppTypeAlias(statement: typealiasStatement, isOnMethod: cppMethod != nil)
        typealiases.append(typeAlias)

        let namespace = self.namespaceResolver.getNamespace(forTypeAlias: typeAlias)
        return CppTypeParser(typeNameResolver: .with(typeDeclaration: CPPTypeDeclaration(namespace: namespace, name: typealiasFunctionName, symbolType: .class)),
                             method: cppMethod,
                             isMovable: true,
                             isIntrinsicallyNullable: false,
                             defaultInitializationString: nil)
    }

    static func makeCanonicalTypeKey(type: ValdiModelPropertyType) -> String {
        switch type {
        case .string:
            return "string"
        case .double:
            return "double"
        case .bool:
            return "bool"
        case .long:
            return "long"
        case .array(elementType: let elementType):
            return "[\(makeCanonicalTypeKey(type: elementType))]"
        case .bytes:
            return "bytes"
        case .map:
            return "map"
        case .any:
            return "any"
        case .void:
            return "void"
        case .function(parameters: let parameters, returnType: let returnType, _, _):
            let parameterStrings = parameters.map { makeCanonicalTypeKey(type: $0.type) }.joined(separator: ", ")
            return "(\(parameterStrings)): \(makeCanonicalTypeKey(type: returnType))"
        case .object(let type):
            if !type.isGenerated {
                return "any"
            }

            return type.cppType?.declaration.fullTypeName ?? "any"
        case .genericTypeParameter(let name):
            return name
        case .genericObject(let type, let typeArguments):
            if !type.isGenerated {
                return "any"
            }

            let objectType = type.cppType?.declaration.fullTypeName ?? "any"

            let str = typeArguments.map { makeCanonicalTypeKey(type: $0) }.joined(separator: ", ")

            return "\(objectType)<\(str)>"
        case .promise(let type):
            let element = makeCanonicalTypeKey(type: type)
            return "promise<\(element)>"
        case .enum(let type):
            if !type.isGenerated {
                return "any"
            }

            return type.cppType?.declaration.fullTypeName ?? "any"
        case .nullable(let elementType):
            return "\(makeCanonicalTypeKey(type: elementType))?"
        }
    }

    func getTypeParser(type: ValdiModelPropertyType, namePaths: [String], nameAllocator: PropertyNameAllocator) throws -> CppTypeParser {
        switch type {
        case .string:
            header.includeSection.addInclude(path: "valdi_core/cpp/Utils/StringBox.hpp")
            return CppTypeParser(
                typeNameResolver: CppCodeGenerator.getValdiTypeName(typeName: "StringBox"),
                method: nil,
                isMovable: true,
                isIntrinsicallyNullable: false,
                defaultInitializationString: nil
            )
        case .double:
            return getPrimitiveTypeParser(primitiveType: "double", isMovable: false, isIntrinsicallyNullable: false, defaultInitializationString: "0.0")
        case .bool:
            return getPrimitiveTypeParser(primitiveType: "bool", isMovable: false, isIntrinsicallyNullable: false, defaultInitializationString: "false")
        case .long:
            return getPrimitiveTypeParser(primitiveType: "int64_t", isMovable: false, isIntrinsicallyNullable: false, defaultInitializationString: "0")
        case .array(elementType: let elementType):
            return try getArrayTypeParser(elementType: elementType, namePaths: namePaths, nameAllocator: nameAllocator)
        case .bytes:
            header.includeSection.addInclude(path: "valdi_core/cpp/Utils/Bytes.hpp")
            return CppTypeParser(
                typeNameResolver: CppCodeGenerator.getValdiTypeName(typeName: "BytesView"),
                method: nil,
                isMovable: true,
                isIntrinsicallyNullable: false,
                defaultInitializationString: nil
            )
        case .map:
            header.includeSection.addInclude(path: "valdi_core/cpp/Utils/ValueMap.hpp")
            return CppTypeParser(
                typeNameResolver: CppCodeGenerator.makeRefType(typeName: CppCodeGenerator.getValdiTypeName(typeName: "ValueMap")),
                method: nil,
                isMovable: true,
                isIntrinsicallyNullable: true,
                defaultInitializationString: nil
            )
        case .any:
            return getUntypedTypeParser()
        case .void:
            return CppTypeParser(
                typeNameResolver: .with(typeName: "void"),
                method: nil,
                isMovable: false,
                isIntrinsicallyNullable: false,
                defaultInitializationString: nil
            )
        case .function(parameters: let parameters, returnType: let returnType, _, _):
            return try getFunctionTypeParser(parameters: parameters, returnType: returnType, namePaths: namePaths, nameAllocator: nameAllocator)
        case .object(let objectType):
            if !objectType.isGenerated {
                // No support for @NativeClass and @NativeInterface for now.
                // We just export them as untyped
                return getUntypedTypeParser()
            }

            guard let cppType = objectType.cppType else {
                throw CompilerError("No C++ type declared for referenced type \(objectType.tsType)")
            }

            let isInterface = objectType.kind == .interface

            return getTypeReferenceTypeParser(referencedType: cppType,
                                              referencedPropertyType: type,
                                              isObject: true,
                                              isRefType: isInterface,
                                              nameAllocator: nameAllocator)
        case .genericTypeParameter(let name):
            return CppTypeParser(
                typeNameResolver: .with(typeName: name),
                method: nil,
                isMovable: true,
                isIntrinsicallyNullable: false,
                defaultInitializationString: "{}"
            )
        case .genericObject(let objectType, let typeArguments):
            if !objectType.isGenerated {
                // No support for @NativeClass and @NativeInterface for now.
                // We just export them as untyped
                return getUntypedTypeParser()
            }

            guard let cppType = objectType.cppType else {
                throw CompilerError("No C++ type declared for referenced type \(objectType.tsType)")
            }


            let typeParsers = try typeArguments.map { try getTypeParser(type: $0, namePaths: namePaths, nameAllocator: nameAllocator) }
            let isInterface = objectType.kind == .interface

            return getGenericTypeReferenceTypeParser(referencedType: cppType,
                                                     referencedPropertyType: type,
                                                     referenceTypeIsInterface: isInterface,
                                                     typeParsers: typeParsers,
                                                     nameAllocator: nameAllocator)
        case .promise(let type):
            let innerType = try getTypeParser(type: type, namePaths: namePaths, nameAllocator: nameAllocator)

            return getPromiseTypeParser(valueTypeParser: innerType)
        case .enum(let enumType):
            if !enumType.isGenerated {
                // No support for @NativeClass and @NativeInterface for now.
                // We just export them as untyped
                return getUntypedTypeParser()
            }

            guard let cppType = enumType.cppType else {
                throw CompilerError("No C++ type declared for referenced enum \(enumType.tsType)")
            }

            return getTypeReferenceTypeParser(referencedType: cppType,
                                              referencedPropertyType: type,
                                              isObject: false,
                                              isRefType: false,
                                              nameAllocator: nameAllocator)
        case .nullable(let elementType):
            // For nullable object types, we need to handle them specially because they get wrapped
            // in Ref<> (which only requires a forward declaration), but the recursive getTypeParser
            // call would register a hard dependency before we have a chance to wrap it.
            switch elementType {
            case .object(let objectType):
                if !objectType.isGenerated {
                    return getUntypedTypeParser()
                }
                guard let cppType = objectType.cppType else {
                    throw CompilerError("No C++ type declared for referenced type \(objectType.tsType)")
                }
                // Pass isRefType: true since nullable objects become Ref<T> which only needs forward declaration
                // Use elementType as referencedPropertyType to maintain consistent canonical type keys
                return getTypeReferenceTypeParser(referencedType: cppType,
                                                  referencedPropertyType: elementType,
                                                  isObject: true,
                                                  isRefType: true,
                                                  nameAllocator: nameAllocator)
            case .genericObject(let objectType, let typeArguments):
                if !objectType.isGenerated {
                    return getUntypedTypeParser()
                }
                guard let cppType = objectType.cppType else {
                    throw CompilerError("No C++ type declared for referenced type \(objectType.tsType)")
                }
                let typeParsers = try typeArguments.map { try getTypeParser(type: $0, namePaths: namePaths, nameAllocator: nameAllocator) }
                // Pass referenceTypeIsInterface: true since nullable generic objects become Ref<T<...>> which only needs forward declaration
                // Use elementType as referencedPropertyType to maintain consistent canonical type keys
                return getGenericTypeReferenceTypeParser(referencedType: cppType,
                                                         referencedPropertyType: elementType,
                                                         referenceTypeIsInterface: true,
                                                         typeParsers: typeParsers,
                                                         nameAllocator: nameAllocator)
            default:
                let elementTypeParser = try getTypeParser(type: elementType, namePaths: namePaths, nameAllocator: nameAllocator)
                return toOptionalTypeParser(elementType: elementType, typeParser: elementTypeParser)
            }
        }
    }

}
