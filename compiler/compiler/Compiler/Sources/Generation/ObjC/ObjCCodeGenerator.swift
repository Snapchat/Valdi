//
//  ObjCCodeGenerator.swift
//  Compiler
//
//  Created by saniul on 25/03/2019.
//

import Foundation

struct GeneratedCode {
    let apiHeader: CodeWriter
    let apiImpl: CodeWriter
    let header: CodeWriter
    let impl: CodeWriter
}

struct ObjCTypeParser {
    let typeName: String
    let propertyAttribute: String
    let objcSelector: ObjCSelector?
    let cType: CType
    // Type name safe to use in the global header scope
    let headerDeclarationSafeTypeName: String
    let propertyAdditionalHeaderDeclaration: String?
}

struct ObjCProperty {
    let propertyName: String
    let modelProperty: ValdiModelProperty
}

private final class ObjCImportSection: CodeWriterContent {

    private(set) var importedClasses = Set<String>()

    func addImport(path: String) {
        importedClasses.insert(path)
    }

    var content: String {
        return importedClasses.sorted().map { "#import \($0)\n" }.joined()
    }
}

private final class ObjCForwardDeclarationSection: CodeWriterContent {

    private(set) var forwardsSet = Set<String>()

    func addForwardDeclaration(value: String) {
        forwardsSet.insert(value)
    }

    var content: String {
        return forwardsSet.sorted().map { "\($0)\n" }.joined()
    }

}

class ObjCCodeGenerator: CodeWriter {}

final class ObjCFileGenerator: ObjCCodeGenerator {

    private let importSection = ObjCImportSection()
    private let forwardDeclarationSection = ObjCForwardDeclarationSection()
    private let declarationSection = JoinedContent(separator: "\n")

    override init() {
        super.init()

        appendHeader(importSection)
        appendHeader("\n")
        appendHeader(forwardDeclarationSection)
        appendHeader("\n")
        appendHeader(declarationSection)
        appendHeader("\n")
    }

    func addImport(path: String) {
        importSection.addImport(path: path)
    }

    func addForwardDeclaration(type: String) {
        forwardDeclarationSection.addForwardDeclaration(value: type)
    }

    func addDeclaration(type: String) {
        declarationSection.append(content: type)
    }

    func appendSwiftName(_ swiftName: String) {
        appendBody("NS_SWIFT_NAME(\(swiftName))")
    }
}

struct ObjCFunctionParameterParser {
    let name: PropertyName
    let parser: ObjCTypeParser
}

struct ObjCFunctionParser {
    let parameters: [ObjCFunctionParameterParser]
    let returnParser: ObjCTypeParser
    let objcSelector: ObjCSelector?
    let typeName: String
    let propertyAdditionalHeaderDeclaration: String?

    init(parameters: [ObjCFunctionParameterParser],
         returnParser: ObjCTypeParser,
         objcSelectr: ObjCSelector?,
         typeName: String,
         propertyAdditionalHeaderDeclaration: String?) {
        self.parameters = parameters
        self.returnParser = returnParser
        self.objcSelector = objcSelectr
        self.typeName = typeName
        self.propertyAdditionalHeaderDeclaration = propertyAdditionalHeaderDeclaration
    }
}

final class ObjCClassGenerator {

    let nameAllocator = PropertyNameAllocator.forObjC()

    let apiHeader = ObjCFileGenerator()
    let apiImpl = ObjCFileGenerator()

    let header = ObjCFileGenerator()
    let impl = ObjCFileGenerator()

    let emittedIdentifiers = ObjCEmittedIdentifiers()
    let typeArgumentIdentifiers = ObjCEmittedIdentifiers()
    let emittedFunctions = ObjCEmittedTrampolineFunctions()

    private let className: String

    init(className: String) {
        self.className = className

        //TODO(3521): Update to valid_core
        apiHeader.addImport(path: "<Foundation/Foundation.h>")
        apiImpl.addImport(path: "<valdi_core/SCValdiBuiltinTrampolines.h>")

        header.addImport(path: "<valdi_core/SCValdiMarshaller.h>")
        impl.addImport(path: "<valdi_core/SCValdiJSConversionUtils.h>")

        // We reserve marshaller as a keyword to simplify code gen
        _ = nameAllocator.allocate(property: "marshaller")
    }

    private func newUniqueType(namePaths: [String], suffix: String) -> PropertyName {
        let innerIdentifier = namePaths.map { $0.pascalCased }.joined()
        let identifier = "\(className)\(innerIdentifier)\(suffix)"

        // We use the root name allocator since the type will be declared in both the header and impl
        return nameAllocator.allocate(property: identifier)
    }

    func writeFunctionTypeParser(returnType: ValdiModelPropertyType,
                                 parameters: [ValdiModelProperty],
                                 namePaths: [String],
                                 isInterface: Bool,
                                 includesGenericType: Bool,
                                 nameAllocator: PropertyNameAllocator) throws -> ObjCFunctionParser {
        let parsers: [ObjCTypeParser] = try parameters.map {
            // emitMarshall/unmarshall values passed to the arguments parsers is reversed here
            // If we are marshalling a function, we need unmarshall of the parameters, and we need marshall of return value
            try writeTypeParser(type: $0.type.unwrappingOptional,
                                isOptional: $0.type.isOptional,
                                namePaths: namePaths + [$0.name],
                                allowValueType: true,
                                isInterface: false,
                                nameAllocator: nameAllocator.scoped())
        }

        let returnParser = try writeTypeParser(type: returnType.unwrappingOptional,
                                               isOptional: returnType.isOptional,
                                               namePaths: namePaths + ["Return"],
                                               allowValueType: true,
                                               isInterface: false,
                                               nameAllocator: nameAllocator.scoped())

        let parametersAndParsers: [ObjCFunctionParameterParser] = parsers.enumerated().map { index, parser in
            let name = nameAllocator.allocate(property: parameters[index].name)
            return ObjCFunctionParameterParser(name: name, parser: parser)
        }

        let objcSelector: ObjCSelector?
        let typeName: String
        var propertyAdditionalHeaderDeclaration: String?

        if isInterface {
            let methodName = namePaths[0]
            objcSelector = ObjCSelector(returnType: returnParser.typeName, methodName: methodName, parameters: parametersAndParsers.map { ObjcMessageParameter(name: $0.name.name, type: $0.parser.typeName) })
            typeName = objcSelector!.messageDeclaration
        } else {
            objcSelector = nil
            let blockName = newUniqueType(namePaths: namePaths, suffix: "Block")
            let headerSafeParameterString = parametersAndParsers.map { propertyAndParser -> String in
                "\(propertyAndParser.parser.headerDeclarationSafeTypeName) \(propertyAndParser.name)"
                }.joined(separator: ", ")

            let headerDeclarationSafeTypeDef = "typedef \(returnParser.headerDeclarationSafeTypeName)(^\(blockName))(\(headerSafeParameterString));"
            typeName = blockName.name

            // we can't use typedefs for any function types that include generic types, since the types are declared _outside_ of a context that declares the type parameters
            if includesGenericType {
                let actualParameterString = parametersAndParsers.map { propertyAndParser -> String in
                    "\(propertyAndParser.parser.typeName) \(propertyAndParser.name)"
                    }.joined(separator: ", ")
                let actualTypeDef = "typedef \(returnParser.typeName)(^\(blockName))(\(actualParameterString));"
                var declarations = parametersAndParsers.compactMap { propertyAndParser -> String? in
                    propertyAndParser.parser.propertyAdditionalHeaderDeclaration
                }
                if let returnAdditionalHeaderDeclaration = returnParser.propertyAdditionalHeaderDeclaration {
                    declarations.append(returnAdditionalHeaderDeclaration)
                }
                declarations.append(actualTypeDef)
                propertyAdditionalHeaderDeclaration = declarations.joined(separator: "\n")
            } else {
                apiHeader.addDeclaration(type: headerDeclarationSafeTypeDef)
            }
        }

        emittedFunctions.appendBlockSupportIfNeeded(parameters: parametersAndParsers.map { $0.parser.cType }, returnType: returnParser.cType, functionType: isInterface ? .objcMethod : .block)

        return ObjCFunctionParser(parameters: parametersAndParsers,
                                  returnParser: returnParser,
                                  objcSelectr: objcSelector,
                                  typeName: typeName,
                                  propertyAdditionalHeaderDeclaration: propertyAdditionalHeaderDeclaration)
    }

    func ensureTypeAvailability(type: IOSType) {
        apiHeader.addImport(path: type.importHeaderStatement(kind: .apiOnly))
        header.addImport(path: type.importHeaderStatement(kind: .withUtilities))
        if type.kind == .interface {
            apiHeader.addForwardDeclaration(type: "@protocol \(type.name);")
        } else if type.kind == .class {
            apiHeader.addForwardDeclaration(type: "@class \(type.name);")
        }
    }

    func writeObjectDescriptorGetter(resolvedProperties: [ObjCProperty],
                                     objcSelectors: [ObjCSelector?],
                                     typeParameters: [ValdiTypeParameter]?,
                                     objectDescriptorType: String) throws -> CodeWriter {
        let output = ObjCCodeGenerator()

        output.appendBody("""
        + (SCValdiMarshallableObjectDescriptor)valdiMarshallableObjectDescriptor
        {

        """)

        let blocksParam: String

        if emittedFunctions.needBlockSupport {
            blocksParam = "kBlocks"
            output.appendBody("static const SCValdiMarshallableObjectBlockSupport \(blocksParam)[] = {\n")
            output.appendBody(emittedFunctions.blocksSection)
            output.appendBody("{ .typeEncoding = nil },\n")
            output.appendBody("};\n")
        } else {
            blocksParam = "nil"
        }

        output.appendBody("static const SCValdiMarshallableObjectFieldDescriptor kFields[] = {\n")

        for (index, objcProperty) in resolvedProperties.enumerated() {
            let selName: String
            let objcSelector = objcSelectors[index]
            if let objcSelector = objcSelector {
                selName = "\"\(objcSelector.selectorName)\""
            } else if objcProperty.propertyName != objcProperty.modelProperty.name {
                selName = "\"\(objcProperty.propertyName)\""
            } else {
                selName = "nil"
            }

            let schemaWriter = ObjCSchemaWriter(typeParameters: typeParameters, emittedIdentifiers: emittedIdentifiers)
            try schemaWriter.appendType(objcProperty.modelProperty.type, asBoxed: false, isMethod: objcSelector != nil)

            output.appendBody("{.name = \"\(objcProperty.modelProperty.name)\", .selName = \(selName), .type = \"\(schemaWriter.str)\"},\n")
        }

        output.appendBody("{.name = nil},\n")
        output.appendBody("};\n")

        let identifiersParam: String

        if !emittedIdentifiers.isEmpty {
            identifiersParam = "kIdentifiers"

            output.appendBody("static const char *const \(identifiersParam)[] = ")
            output.appendBody(emittedIdentifiers.initializationString)
            output.appendBody(";\n")
        } else {
            identifiersParam = "nil"
        }

        output.appendBody("return SCValdiMarshallableObjectDescriptorMake(kFields, \(identifiersParam), \(blocksParam), \(objectDescriptorType));\n")

        output.appendBody("}\n\n")

        return output
    }

    func writeTypeParser(type: ValdiModelPropertyType,
                         isOptional: Bool,
                         namePaths: [String],
                         allowValueType: Bool,
                         isInterface: Bool,
                         nameAllocator: PropertyNameAllocator,
                         typeParameterSafe: Bool = false) throws -> ObjCTypeParser {
        let typeName: String
        let cElementType: CType

        var isVoid = false
        var objcSelector: ObjCSelector?
        var propertyAttribute: String
        var propertyAdditionalHeaderDeclaration: String?
        var headerDeclarationSafeTypeNameOverride: String?

        switch type {
        case .string:
            typeName = "NSString *"
            propertyAttribute = "copy"
            cElementType = .pointer
        case .double:
            if isOptional || !allowValueType {
                typeName = "NSNumber *"
                propertyAttribute = "strong"
                cElementType = .pointer
            } else {
                typeName = "double"
                propertyAttribute = "assign"
                cElementType = .double
            }
        case .bool:
            if isOptional || !allowValueType {
                typeName = "NSNumber *"
                propertyAttribute = "strong"
                cElementType = .pointer
            } else {
                typeName = "BOOL"
                propertyAttribute = "assign"
                cElementType = .bool
            }
        case .long:
            if isOptional || !allowValueType {
                typeName = "NSNumber *"
                propertyAttribute = "strong"
                cElementType = .pointer
            } else {
                typeName = "int64_t"
                propertyAttribute = "assign"
                cElementType = .long
            }
        case .array(let elementType):
            let childParser = try writeTypeParser(type: elementType.unwrappingOptional,
                                                  isOptional: elementType.isOptional,
                                                  namePaths: namePaths + ["element"],
                                                  allowValueType: false,
                                                  isInterface: false,
                                                  nameAllocator: nameAllocator.scoped(),
                                                  typeParameterSafe: true)

            propertyAdditionalHeaderDeclaration = childParser.propertyAdditionalHeaderDeclaration
            typeName = "NSArray<\(childParser.typeName)> *"
            headerDeclarationSafeTypeNameOverride = "NSArray<\(childParser.headerDeclarationSafeTypeName)> *"
            propertyAttribute = "copy"
            cElementType = .pointer
        case .bytes:
            typeName = "NSData *"
            propertyAttribute = "copy"
            cElementType = .pointer
        case .map:
            typeName = "NSDictionary <NSString *, id> *"
            propertyAttribute = "copy"
            cElementType = .pointer
        case .any:
            typeName = "NSObject *"
            propertyAttribute = "strong"
            cElementType = .pointer
        case .void:
            if typeParameterSafe {
                let undefinedImportPath = "<valdi_core/SCValdiUndefinedValue.h>"
                apiHeader.addImport(path: undefinedImportPath)
                header.addImport(path: undefinedImportPath)

                typeName = "SCValdiUndefinedValue *"
                propertyAttribute = "strong"
                cElementType = .pointer
            } else {
                typeName = "void"
                propertyAttribute = ""
                isVoid = true
                cElementType = .void
            }
        case .function(let parameters, let returnType, _, _):
            let functionParser = try writeFunctionTypeParser(returnType: returnType,
                                                             parameters: parameters,
                                                             namePaths: namePaths,
                                                             isInterface: isInterface,
                                                             includesGenericType: type.includesGenericType,
                                                             nameAllocator: nameAllocator)

            typeName = functionParser.typeName
            objcSelector = functionParser.objcSelector
            propertyAdditionalHeaderDeclaration = functionParser.propertyAdditionalHeaderDeclaration
            propertyAttribute = "copy"
            cElementType = .pointer
        case .enum(let mapping):
            guard let resolvedType = mapping.iosType else {
                throw CompilerError("No iOS type declared")
            }

            ensureTypeAvailability(type: resolvedType)

            if mapping.kind == .enum && (isOptional || !allowValueType) {
                typeName = "NSNumber *"
                propertyAttribute = "strong"
                cElementType = .pointer
            } else {
                if isOptional {
                    typeName = ObjCEnumGenerator.nullableTypeName(typename: resolvedType.name)
                } else {
                    typeName = resolvedType.name
                }

                if mapping.kind == .stringEnum {
                    propertyAttribute = "copy"
                    cElementType = .pointer
                } else {
                    propertyAttribute = "assign"
                    cElementType = .int
                }
            }
        case .genericTypeParameter(let name):
            typeName = name
            headerDeclarationSafeTypeNameOverride = "id"
            propertyAttribute = "strong"
            cElementType = .pointer
        case .genericObject(let mapping, let typeArguments):
            guard let resolvedType = mapping.iosType else {
                throw CompilerError("No iOS type declared")
            }

            ensureTypeAvailability(type: resolvedType)

            guard case .class = mapping.kind else {
                throw CompilerError("Generic object must be a class, not a protocol")
            }

            let typeArgumentsParsers: [ObjCTypeParser] = try typeArguments.map {
                let typeArgumentTypeParser = try writeTypeParser(type: $0,
                                                             isOptional: $0.isOptional,
                                                             namePaths: namePaths,
                                                             allowValueType: false,
                                                             isInterface: false,
                                                             nameAllocator: nameAllocator)
                return typeArgumentTypeParser
            }
            let typeArgumentsList = typeArgumentsParsers.map(\.headerDeclarationSafeTypeName).joined(separator: ", ")
            let resolvedTypeName = "\(resolvedType.name)<\(typeArgumentsList)> *"
            propertyAdditionalHeaderDeclaration = typeArgumentsParsers.compactMap(\.propertyAdditionalHeaderDeclaration).joined(separator: "\n")

            typeName = resolvedTypeName

            guard mapping.isGenerated else {
                throw CompilerError("Generic objects must be generated")
            }
            propertyAttribute = "strong"
            cElementType = .pointer
        case .object(let mapping):
            guard let resolvedType = mapping.iosType else {
                throw CompilerError("No iOS type declared")
            }

            ensureTypeAvailability(type: resolvedType)

            let resolvedTypeName: String
            switch mapping.kind {
            case .interface:
                resolvedTypeName = "id<\(resolvedType.name)>"
            case .class:
                resolvedTypeName = "\(resolvedType.name) *"
            case .enum, .stringEnum:
                resolvedTypeName = resolvedType.name
            }

            typeName = resolvedTypeName
            propertyAttribute = "strong"
            cElementType = .pointer
        case .promise(let typeArgument):
            let promiseImportPath = "<valdi_core/SCValdiPromise.h>"
            apiHeader.addImport(path: promiseImportPath)
            header.addImport(path: promiseImportPath)

            let childParser = try writeTypeParser(type: typeArgument.unwrappingOptional,
                                                  isOptional: typeArgument.isOptional,
                                                  namePaths: namePaths + ["element"],
                                                  allowValueType: false,
                                                  isInterface: false,
                                                  nameAllocator: nameAllocator.scoped(),
                                                  typeParameterSafe: true)

            propertyAdditionalHeaderDeclaration = childParser.propertyAdditionalHeaderDeclaration
            typeName = "SCValdiPromise<\(childParser.typeName)> *"
            headerDeclarationSafeTypeNameOverride = "SCValdiPromise<\(childParser.headerDeclarationSafeTypeName)> *"
            propertyAttribute = "strong"
            cElementType = .pointer
        case .nullable(let type):
            return try writeTypeParser(type: type, isOptional: true, namePaths: namePaths, allowValueType: false, isInterface: isInterface, nameAllocator: nameAllocator)
        }

        let isObjCMessage = objcSelector != nil

        func resolveTypeName(_ typeName: String) -> String {
            if isOptional && !isObjCMessage {
                if !isVoid && !isObjCMessage && allowValueType && !typeParameterSafe {
                    return "\(typeName) _Nullable"
                }
            } else {
                if !propertyAttribute.contains("assign") && !isVoid && !isObjCMessage && allowValueType && !typeParameterSafe {
                    return "\(typeName) _Nonnull"
                }
            }
            return typeName
        }

        let resolvedTypeName = resolveTypeName(typeName)
        let headerDeclarationSafeTypeName = headerDeclarationSafeTypeNameOverride.map(resolveTypeName) ?? resolvedTypeName

        return ObjCTypeParser(typeName: resolvedTypeName,
                              propertyAttribute: propertyAttribute,
                              objcSelector: objcSelector,
                              cType: cElementType,
                              headerDeclarationSafeTypeName: headerDeclarationSafeTypeName,
                              propertyAdditionalHeaderDeclaration: propertyAdditionalHeaderDeclaration)
    }

}
