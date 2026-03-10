//
//  ValdiRawDocument.swift
//  Compiler
//
//  Created by Simon Corsin on 4/6/18.
//  Copyright © 2018 Snap Inc. All rights reserved.
//

import Foundation

struct ValdiRawDocument {

    var content = ""

    var scripts = [ValdiScript]()
    var styles = [ValdiStyleSheet]()
    var actions = [ValdiAction]()
    var template: ValdiRawTemplate?
    var viewModel: ValdiModel?
    var componentContext: ValdiNodeClassMapping?
    var additionalModels = [ValdiModel]()
    var classMapping: ValdiClassMapping?
}

struct ValdiStyleSheet {
    var source: String?
    var content: String?
}

struct ValdiScript {
    var source: String?
    var content: String?
    var lang: String?
    var lineOffset: Int?
}

struct ValdiRawNodeAttribute {
    var name = ""
    var value = ""
    var valueIsExpr = false
    var evaluateOnce = false
    var isViewModelField = false
}

enum ValdiActionType {
    case native
    case javaScript
}

struct ValdiAction: Equatable {
    let name: String
    let type: ValdiActionType
}

struct ValdiRawNode {
    enum Child {
        case expression(String)
        case node(ValdiRawNode)

        var node: ValdiRawNode? {
            guard case let .node(node) = self else {
                return nil
            }
            return node
        }
    }

    var nodeType = ""
    var id = ""
    var destSlotName: String?

    var forEachExpr: ValdiRawNodeAttribute?
    var ifExpr: ValdiRawNodeAttribute?
    var elseIfExpr: ValdiRawNodeAttribute?
    var elseExpr: ValdiRawNodeAttribute?
    var unlessExpr: ValdiRawNodeAttribute?
    var customAttributes = [ValdiRawNodeAttribute]()
    var isLayout: Bool?

    var children = [Child]()

    var lineNumber = -1
    var columnNumber = -1
}

struct ValdiRawTemplate {
    var rootNode: ValdiRawNode?
    var jsComponentClass: String?
}

indirect enum ValdiModelPropertyType {
    case string
    case double
    case bool
    case long
    case array(elementType: ValdiModelPropertyType)
    case bytes
    case map(keyType: ValdiModelPropertyType, valueType: ValdiModelPropertyType)
    case any
    case void
    case function(parameters: [ValdiModelProperty], returnType: ValdiModelPropertyType, isSingleCall: Bool, shouldCallOnWorkerThread: Bool)
    case object(ValdiNodeClassMapping)
    case genericTypeParameter(name: String)
    case genericObject(ValdiNodeClassMapping, typeArguments: [ValdiModelPropertyType])
    case promise(typeArgument: ValdiModelPropertyType)
    case `enum`(ValdiNodeClassMapping)
    case nullable(ValdiModelPropertyType)

    var isFunction: Bool {
        switch self {
        case .function:
            return true
        case .nullable(let innerType):
            return innerType.isFunction
        default:
            return false
        }
    }

    var isOptional: Bool {
        switch self {
        case .nullable:
            return true
        default:
            return false
        }
    }

    var isVoid: Bool {
        switch self {
        case .void:
            return true
        default:
            return false
        }
    }

    var unwrappingOptional: ValdiModelPropertyType {
        switch self {
        case .nullable(let innerType):
            return innerType.unwrappingOptional
        default:
            return self
        }
    }

    var includesGenericType: Bool {
        switch self {
        case .string, .double, .bool, .bytes, .map, .void, .object, .enum, .long, .any:
            return false
        case .array(let elementType):
            return elementType.includesGenericType
        case .function(let parameters, let returnType, _, _):
            return parameters.contains(where: { $0.type.includesGenericType }) || returnType.includesGenericType
        case .genericTypeParameter:
            return true
        case .genericObject:
            return true
        case .promise:
            return true
        case .nullable(let type):
            return type.includesGenericType
        }
    }
}

struct OmitConstructorParams: Encodable {
    let ios: Bool?
    let android: Bool?
}

struct InjectableParams {
    struct Compatibility: OptionSet {
        let rawValue: UInt
        
        static let ios: Compatibility = Compatibility(rawValue: 1 << 0)
        static let android: Compatibility = Compatibility(rawValue: 1 << 1)
        static let all: Compatibility = [.ios, .android]
    }
    
    static let empty: InjectableParams = InjectableParams(compatibility: [])
    
    let compatibility: Compatibility
}

struct ValdiModelProperty {
    var name: String
    var type: ValdiModelPropertyType
    let comments: String?
    let omitConstructor: OmitConstructorParams?
    let injectableParams: InjectableParams
}

struct ValdiTypeParameter {
    var name: String
}

struct ValdiModel {
    var tsType: String = ""
    var iosType: IOSType?
    var androidClassName: String?
    var cppType: CPPType?
    var typeParameters: [ValdiTypeParameter]?
    var exportAsInterface = false
    var legacyConstructors = false
    var usePublicFields = false
    var comments: String?
    var properties = [ValdiModelProperty]()
}

struct ExportedModule {
    var model: ValdiModel
    var modulePath: String
}

protocol EnumCaseValueType { }
extension String: EnumCaseValueType { }
extension Int: EnumCaseValueType { }

struct EnumCase<T: EnumCaseValueType> {
    let name: String
    let value: T
    let comments: String?
}

extension EnumCase: Encodable where T: Encodable {

}

enum NativeTypeKind: Codable {
    case interface
    case `class`
    case `enum`
    case stringEnum
}

enum IOSLanguage: String, Codable {
    case swift = "swift"
    case objc = "objc"
    case both = "both"

    static func split(_ input: String) -> [String] {
        let components = input.components(separatedBy: ",")
        let trimmed = components.map { $0.trimmingCharacters(in: .whitespaces) }
        return trimmed.sorted()
    }
    
    static func parse(input: String) throws -> IOSLanguage {
        switch input {
        case "objc": return objc
        case "swift": return swift
        case let components where split(components) == ["objc", "swift"]:
            return both
        default:
            throw CompilerError("Unsupported ios language '\(input)'")
        }
    }
}

struct IOSType: Codable {
    static let enumTypeGroupingPriority = 0
    static let enumStringTypeGroupingPriority = 1
    static let classTypeGroupingPriority = 2
    static let interfaceTypeGroupingPriority = 3
    static let functionTypeGroupingPriority = 4
    static let idsTypeGroupingPriority = 5
    static let viewTypeGroupingPriority = 6

    let name: String
    let swiftName: String
    var importPrefix: String?
    let importSuffix: String
    var isImportPrefixOverridden = false
    var fromSingleFileCodegen = false
    var kind: NativeTypeKind
    var iosLanguage: IOSLanguage

    init(name: String, swiftName: String? = nil, bundleInfo: CompilationItem.BundleInfo?, kind: NativeTypeKind, iosLanguage: IOSLanguage) {
        self.name = name
        self.swiftName = swiftName ?? name
        if let bundleInfo {
            self.fromSingleFileCodegen = bundleInfo.singleFileCodegen
            self.importSuffix = bundleInfo.singleFileCodegen ? bundleInfo.iosModuleName : name
        } else {
            self.importSuffix = name
        }
        self.kind = kind
        self.iosLanguage = iosLanguage
    }

    var importHeaderStatement: String {
        return self.importHeaderStatement(kind: .apiOnly)
    }

    enum HeaderImportKind {
        case apiOnly
        case withUtilities

        static let apiOnlyModuleNameSuffix = "Types"
    }

    func importHeaderFilename(kind: HeaderImportKind) -> String {
        switch kind {
        case .apiOnly:
            if fromSingleFileCodegen {
                return "\(importSuffix)\(HeaderImportKind.apiOnlyModuleNameSuffix).h"
            } else {
                return "\(importSuffix).h"
            }
        case .withUtilities:
            return "\(importSuffix).h"
        }
    }

    func importHeaderStatement(kind: HeaderImportKind) -> String {
        let filename = importHeaderFilename(kind: kind)
        if let importPrefix = importPrefix {
            if isImportPrefixOverridden {
                if fromSingleFileCodegen {
                    // If the import prefix is explicitly overridden (eg. with iosImportPrefix)
                    // AND the type comes from a single_file_codegen module, then the final 
                    // output type should map to an external native header.
                    // For example, if iosImportPrefix is "valdi_core" and the type is "INavigator",
                    // then output should be <valdi_core/INavigator.h>, not <valdi_core/SCCNavigationTypes.h>
                    // Return the header with the type's name, instead of using the consolidated module header.
                    return "<\(importPrefix)/\(name).h>"
                }
                return "<\(importPrefix)/\(filename)>"
            } else {
                let suffix: String
                switch kind {
                case .apiOnly: suffix = HeaderImportKind.apiOnlyModuleNameSuffix
                case .withUtilities: suffix = ""
                }

                return "<\(importPrefix)\(suffix)/\(filename)>"
            }
        }
        return "\"\(filename)\""
    }

    var typeDeclaration: String {
        switch kind {
        case .class:
            return "\(name) *"
        case .interface:
            return "id<\(name)>"
        case .enum:
            return name
        case .stringEnum:
            return name
        }
    }

    mutating func applyImportPrefix(iosImportPrefix: String, isOverridden: Bool) {
        self.importPrefix = String.sanitizePathComponents(iosImportPrefix)

        self.isImportPrefixOverridden = isOverridden
    }
}

struct CPPNamespace: Codable, Hashable, Comparable {
    static let rootNamespace = CPPNamespace(components: [])
    
    let components: [String]

    func resolve(other: CPPNamespace) -> CPPNamespace {
        var outComponents = components

        for component in other.components {
            if outComponents.first == component {
                outComponents.removeFirst()
            } else {
                break
            }
        }

        return CPPNamespace(components: outComponents)
    }

    var description: String {
        return components.joined(separator: "::")
    }

    func appending(component: String) -> CPPNamespace {
        var components = self.components
        components.append(component)
        return CPPNamespace(components: components)
    }

    static func < (lhs: CPPNamespace, rhs: CPPNamespace) -> Bool {
        return lhs.description < rhs.description
    }
}

struct CPPTypeDeclaration: Codable, Hashable {
    enum SymbolType: Codable, Hashable, Comparable {
        case `class`
        case `enum`

        var declarationString: String {
            switch self {
            case .class:
                return "class"
            case .enum:
                return "enum class"
            }
        }
    }

    let namespace: CPPNamespace
    let name: String
    let symbolType: SymbolType

    init(namespace: CPPNamespace,
         name: String,
         symbolType: SymbolType) {
        self.namespace = namespace
        self.name = name
        self.symbolType = symbolType
    }

    init(namespace: String,
         name: String,
         symbolType: SymbolType) {
        self.namespace = CPPNamespace(components: namespace.components(separatedBy: "::"))
        self.name = name
        self.symbolType = symbolType
    }

    init(name: String,
         symbolType: SymbolType) {
        let components = name.components(separatedBy: "::")
        if components.count == 1 {
            self.namespace = CPPNamespace(components: [])
            self.name = name
        } else {
            self.namespace = CPPNamespace(components: components[0..<(components.count - 1)].map { String($0) })
            self.name = String(components.last!)
        }
        self.symbolType = symbolType
    }

    var fullTypeName: String {
        return resolveTypeName(inNamespace: CPPNamespace(components: []))
    }

    func resolveTypeName(inNamespace: CPPNamespace) -> String {
        let resolved = namespace.resolve(other: inNamespace)

        if resolved.components.isEmpty {
            return name
        } else {
            return "\(resolved.components.joined(separator: "::"))::\(name)"
        }
    }
}

struct CPPTypeReference: Hashable {
    let declaration: CPPTypeDeclaration
    let typeArguments: [String]?

    var fullTypeName: String {
        if let typeArguments {
            return "\(declaration.fullTypeName)<\(typeArguments.joined(separator: ", "))>"
        } else {
            return declaration.fullTypeName
        }
    }

    func resolveTypeName(inNamespace: CPPNamespace) -> String {
        if let typeArguments {
            return "\(declaration.resolveTypeName(inNamespace: inNamespace))<\(typeArguments.joined(separator: ","))>"
        } else {
            return declaration.resolveTypeName(inNamespace: inNamespace)
        }
    }
}

struct CPPType: Codable {
    let declaration: CPPTypeDeclaration
    let includePath: String
    let includePrefix: String?

    var includeDir: String? {
        guard let index = self.includePath.lastIndex(of: "/") else {
            return ""
        }

        return String(self.includePath[self.includePath.startIndex..<index])
    }

    init(declaration: CPPTypeDeclaration,
         module: CompilationItem.BundleInfo,
         includePrefix: String?) {
        self.declaration = declaration
        self.includePath = CPPType.resolveIncludePath(moduleName: module.name,
                                                      prefix: includePrefix,
                                                      typeName: declaration.name,
                                                      singleFileCodegen: module.singleFileCodegen)
        self.includePrefix = includePrefix
    }

    private static func resolveIncludePath(moduleName: String,
                                           prefix: String?,
                                           typeName: String,
                                           singleFileCodegen: Bool) -> String {
        let resolvedIncludePrefix: String
        if let prefix = prefix {
            resolvedIncludePrefix = "\(prefix)\(moduleName)"
        } else {
            resolvedIncludePrefix = moduleName
        }

        if singleFileCodegen {
            return "\(resolvedIncludePrefix)/\(moduleName).hpp"
        } else {
            return "\(resolvedIncludePrefix)/\(typeName).hpp"
        }
    }
}

// TODO: rename to ValdiNodeTypeMapping?
struct ValdiNodeClassMapping {
    let tsType: String
    var iosType: IOSType?
    // TODO: use JVMClass/JVMType, for consistency
    var androidClassName: String?
    var cppType: CPPType?
    var kind: NativeTypeKind

    var isGenerated = false
    var marshallAsUntyped = false
    var converter: String? = nil

    init(tsType: String,
         iosType: IOSType?,
         androidClassName: String?,
         cppType: CPPType?,
         kind: NativeTypeKind) {
        self.tsType = tsType
        self.iosType = iosType
        self.androidClassName = androidClassName
        self.cppType = cppType
        self.kind = kind
    }
}

// TODO: rename to ValdiTypeMapping?
struct ValdiClassMapping {
    var nodeMappingByClass = [String: ValdiNodeClassMapping]()
}