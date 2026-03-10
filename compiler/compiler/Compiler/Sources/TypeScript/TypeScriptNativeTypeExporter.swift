//
//  TypeScriptNativeTypeExporter.swift
//  Compiler
//
//  Created by Simon Corsin on 12/10/18.
//  Copyright © 2018 Snap Inc. All rights reserved.
//

import Foundation

private let propertyCharactersToTrim = CharacterSet.whitespacesAndNewlines.union(CharacterSet(charactersIn: "?:;"))

enum ExportedType {
    case valdiModel(ValdiModel)
    case `enum`(ExportedEnum)
    case function(ExportedFunction)
    case module(ExportedModule)
}

private struct EnumMember {
    let name: String

    enum Value {
        case string(String)
        case number(Int)
    }

    let value: Value
    let comments: String?
}

private class DocumentIndexMatcher {

    private let indexOffset: Int
    private let str: NSString
    private var currentIndex = 0

    var location: Int {
        return currentIndex + indexOffset
    }

    init(str: String, indexOffset: Int) {
        self.indexOffset = indexOffset
        self.str = str as NSString
    }

    // NOTE: Will look for a match in the full remainder of the contents, unless `limitTokens` is provided
    @discardableResult func match(str: String, limitToNextToken limitTokens: [String] = []) throws -> Int {
        let rangeLimit: Int = limitTokens.map { (limitToken: String) -> Int in
            let limitTokenRange = self.str.range(of: limitToken, options: [], range: NSRange(location: currentIndex, length: self.str.length - currentIndex))
            let rangeLimit = limitTokenRange.location == NSNotFound ? self.str.length : limitTokenRange.location
            return rangeLimit
        }.min() ?? self.str.length

        let range = self.str.range(of: str, options: [], range: NSRange(location: currentIndex, length: rangeLimit - currentIndex))
        guard range.location != NSNotFound else {
            throw CompilerError("Could not match index for string '\(str)' in '\(self.str)' after location \(currentIndex)")
        }

        currentIndex = range.upperBound
        return range.location + indexOffset
    }

    func debugDump() -> String {
        let lhs = self.str.substring(with: NSRange(location: 0, length: currentIndex))
        let rhs = self.str.substring(with: NSRange(location: currentIndex, length: self.str.length - currentIndex))
        return "\(lhs)^\(rhs)"
    }
}

private class EnumMemberSequence {
    private var sequence = 0

    func setNext(_ next: Int) {
        sequence = next
    }

    func assign() -> Int {
        let value = sequence
        sequence += 1
        return value
    }
}

final class TypeScriptNativeTypeExporter {

    private let iosType: IOSType?
    private let androidClass: String?
    private let cppType: CPPType?
    private let commentedFile: TypeScriptCommentedFile
    private let annotatedSymbol: TypeScriptAnnotatedSymbol
    private let dumpedSymbol: TS.DumpedSymbolWithComments
    private let linesIndexer: LinesIndexer
    private let nativeTypeResolver: TypeScriptNativeTypeResolver

    init(iosType: IOSType?,
         androidClass: String?,
         cppType: CPPType?,
         commentedFile: TypeScriptCommentedFile,
         annotatedSymbol: TypeScriptAnnotatedSymbol,
         dumpedSymbol: TS.DumpedSymbolWithComments,
         nativeTypeResolver: TypeScriptNativeTypeResolver) {
        self.iosType = iosType
        self.androidClass = androidClass
        self.cppType = cppType
        self.commentedFile = commentedFile
        self.annotatedSymbol = annotatedSymbol
        self.dumpedSymbol = dumpedSymbol
        self.linesIndexer = commentedFile.linesIndexer
        self.nativeTypeResolver = nativeTypeResolver
    }

    private func throwAnnotationError(comments: TS.AST.Comments, message: String) throws -> Never {
        throw CompilerError(type: "Annotation error", message: message, range: NSRange(location: comments.start, length: comments.end - comments.start), inDocument: self.commentedFile.fileContent)
    }

    private func validateWorkerThreadAnnotation(functionReturnType: ValdiModelPropertyType, leadingComments: TS.AST.Comments) throws {
        switch functionReturnType {
        case .void:
            fallthrough
        case .promise:
            // All good
            break
        default:
            try self.throwAnnotationError(comments: leadingComments, message: "@WorkerThread can only be set on functions returning a Promise or void")
        }
    }

    private func makeUnrecognizedType(type: TS.AST.TSType, annotations: [ValdiTypeScriptAnnotation]?, name: String) throws -> ValdiModelPropertyType {
        if let annotations = annotations {
            for annotation in annotations {
                guard let annotation = ValdiAnnotationType(rawValue: annotation.name) else {
                    try self.throwAnnotationError(comments: type.leadingComments!, message: "Invalid annotation")
                }

                if annotation == .untypedMap {
                    return .map(keyType: .string, valueType: .any)
                } else if annotation == .untyped {
                    return .any
                }
            }
        }

        throw CompilerError("Unrecognized type '\(name)'. Only annotated types can be exported.")
    }

    private func resolveType(type: TS.AST.TSType, references: [TS.AST.TypeReference]) throws -> ValdiModelPropertyType {
        var annotations: [ValdiTypeScriptAnnotation]? = nil
        if let leadingComments = type.leadingComments {
            annotations = try ValdiTypeScriptAnnotation.extractAnnotations(comments: leadingComments, fileContent: self.commentedFile.fileContent)
        }

        if let function = type.function {
            let functionParameters = try function.parameters.map { try parsePropertyOrParameter(propertyLikeDeclaration: $0, references: references) }
            let returnValue = try resolveType(type: function.returnValue, references: references)

            var isSingleCall = false
            var shouldCallOnWorkerThread = false
            if let annotations = annotations {
                for annotation in annotations {
                    guard let annotation = ValdiAnnotationType(rawValue: annotation.name) else {
                        try self.throwAnnotationError(comments: type.leadingComments!, message: "Invalid annotation")
                    }

                    if annotation == .singleCall {
                        isSingleCall = true
                    } else if annotation == .workerThread {
                        try validateWorkerThreadAnnotation(functionReturnType: returnValue, leadingComments: type.leadingComments!)

                        shouldCallOnWorkerThread = true
                    } else {
                        try self.throwAnnotationError(comments: type.leadingComments!, message: "Function only support the @SingleCall and @WorkerThread annotations")
                    }
                }
            }

            return .function(parameters: functionParameters, returnType: returnValue, isSingleCall: isSingleCall, shouldCallOnWorkerThread: shouldCallOnWorkerThread)
        }

        if let typeReferenceIndex = type.typeReferenceIndex {
            guard typeReferenceIndex >= 0 && typeReferenceIndex < references.count else {
                throw CompilerError("Out of bounds type reference \(typeReferenceIndex)")
            }

            let typeReference = references[typeReferenceIndex]

            if typeReference.name == "Uint8Array" {
                return .bytes
            } else if typeReference.name == "Long" {
                return .long
            } else if typeReference.name == "Number" {
                return .double
            } else if typeReference.name == "Promise" || typeReference.name == "CancelablePromise" {
                guard let typeArgument = type.typeArguments?.first else {
                    throw CompilerError("Promise types should have a single type argument")
                }
                let resolvedTypeArgument = try self.resolveType(type: typeArgument.type, references: references)
                return .promise(typeArgument: resolvedTypeArgument)
            } else if typeReference.name == "Map" {
                // TODO(simon): Real implementation
                return .map(keyType: .string, valueType: .any)
            }

            if type.isTypeParameter == true {
                return .genericTypeParameter(name: typeReference.name)
            } else if let resolvedClass = self.nativeTypeResolver.resolve(filePath: typeReference.fileName, tsTypeName: typeReference.name) {
                if resolvedClass.marshallAsUntypedMap {
                    return .map(keyType: .string, valueType: .any)
                }

                var classMapping = ValdiNodeClassMapping(tsType: typeReference.name,
                                                            iosType: resolvedClass.iosType,
                                                            androidClassName: resolvedClass.androidClass,
                                                            cppType: resolvedClass.cppType,
                                                            kind: resolvedClass.kind)
                classMapping.isGenerated = resolvedClass.isGenerated
                classMapping.marshallAsUntyped = resolvedClass.marshallAsUntyped
                classMapping.converter = resolvedClass.convertedFromFunctionPath

                if let typeArguments = type.typeArguments {
                    guard case .class = resolvedClass.kind else {
                        throw CompilerError("Generics are only supported on generated types annotated with @GenerateNativeClass")
                    }
                    let resolvedTypeArguments = try typeArguments.map { try self.resolveType(type: $0.type, references: references) }
                    return .genericObject(classMapping, typeArguments: resolvedTypeArguments)
                } else {
                    switch resolvedClass.kind {
                    case .class, .interface:
                        return .object(classMapping)
                    case .enum, .stringEnum:
                        return .enum(classMapping)
                    }
                }
            } else {
                return try makeUnrecognizedType(type: type, annotations: annotations, name: typeReference.name)
            }
        }

        if let array = type.array {
            let elementType = try resolveType(type: array, references: references)

            return .array(elementType: elementType)
        }

        if let unions = type.unions {
            var resolvedType: ValdiModelPropertyType?
            var isOptional = false

            for union in unions {
                if union.name == "null" || union.name == "undefined" {
                    isOptional = true
                } else {
                    guard resolvedType == nil else {
                        let desc = unions.map { $0.name }.joined(separator: " | ")
                        throw CompilerError("Union types are only supported with null or undefined (got \(desc)")
                    }

                    resolvedType = try resolveType(type: union, references: references)
                }
            }

            guard let type = resolvedType else {
                let desc = unions.map { $0.name }.joined(separator: " | ")
                throw CompilerError("Could not resolve type in union '\(desc)'")
            }
            if isOptional {
                return .nullable(type)
            }

            return type
        }

        let typeName = type.name
        if typeName == "string" {
            return .string
        } else if typeName == "number" {
            return .double
        } else if typeName == "boolean" {
            return .bool
        } else if typeName == "void" {
            return .void
        } else if typeName == "any" {
            // TODO: should actually be .any, but introduces lots of breaking changes
            return .map(keyType: .string, valueType: .any)
        } else if typeName == "object" {
            return .any
        } else {
            return try makeUnrecognizedType(type: type, annotations: annotations, name: typeName)
        }
    }

    private struct PropertyMetadata {
        let comments: String?
        let omitConstructor: OmitConstructorParams?
        let injectableParams: InjectableParams
        let shouldCallOnWorkerThread: Bool
    }

    private func getPropertyMetadata(leadingComments: TS.AST.Comments) throws -> PropertyMetadata {
        let annotations = try ValdiTypeScriptAnnotation.extractAnnotations(comments: leadingComments,
                                                                              fileContent: commentedFile.fileContent)

        var omitConstructor: OmitConstructorParams?
        if let annotation = annotations.first(where: { $0.name == "ConstructorOmitted" }) {
            let ios = annotation.parameters?["ios"].map { $0 == "true" }
            let android = annotation.parameters?["android"].map { $0 == "true" }

            omitConstructor = OmitConstructorParams(ios: ios, android: android)
        }

        var injectableParams: InjectableParams = .empty
        if let injectableAnnotation = annotations.first(where: { $0.name == ValdiAnnotationType.injectable.rawValue }) {
            var injectableParamsCompatibility: InjectableParams.Compatibility = []
            
            // no parameters mean its injectable on both
            if injectableAnnotation.parameters?.isEmpty != false {
                injectableParamsCompatibility = InjectableParams.Compatibility.all
            } else {
                let iosOnly = injectableAnnotation.parameters?["iosOnly"].map { $0 == "true" } ?? false
                let androidOnly = injectableAnnotation.parameters?["androidOnly"].map { $0 == "true" } ?? false
                if iosOnly && androidOnly {
                    throw CompilerError("The @Injectable annotation should either be iosOnly or androidOnly. If you want it injectable on both platforms, remove any parameters.")
                }

                if iosOnly {
                    injectableParamsCompatibility.insert(.ios)
                }

                if androidOnly {
                    injectableParamsCompatibility.insert(.android)
                }
            }
            
            injectableParams = InjectableParams(compatibility: injectableParamsCompatibility)
        }

        let shouldCallOnWorkerThread = annotations.contains(where: { $0.name == ValdiAnnotationType.workerThread.rawValue })

        let commentsWithoutAnnotations = TypeScriptAnnotatedSymbol.mergedCommentsWithoutAnnotations(fullComments: leadingComments.text, annotations: annotations)
        return PropertyMetadata(comments: commentsWithoutAnnotations.nonEmpty, omitConstructor: omitConstructor, injectableParams: injectableParams, shouldCallOnWorkerThread: shouldCallOnWorkerThread)
    }

    private func parsePropertyLike(name: String,
                                   type: TS.AST.TSType,
                                   isOptional: Bool,
                                   leadingComments: TS.AST.Comments?,
                                   references: [TS.AST.TypeReference]) throws -> ValdiModelProperty {
        var parsedType = try resolveType(type: type, references: references)
        if isOptional {
            parsedType = .nullable(parsedType)
        }

        var resolvedPropertyMetadata: PropertyMetadata? = nil
        if let leadingComments {
            let propertyMetadata = try getPropertyMetadata(leadingComments: leadingComments)
            resolvedPropertyMetadata = propertyMetadata

            if propertyMetadata.omitConstructor != nil && !parsedType.isOptional {
                throw CompilerError("@ConstructorOmitted can only be set on an optional property or function")
            }

            if propertyMetadata.injectableParams.compatibility.contains(.android) && propertyMetadata.injectableParams.compatibility.contains(.ios) && parsedType.isOptional {
                throw CompilerError("@Injectable can only be set on non-optional properties")
            }

            if propertyMetadata.shouldCallOnWorkerThread {
                guard case let .function(parameters, returnType, isSingleCall, _) = parsedType.unwrappingOptional else {
                    try throwAnnotationError(comments: leadingComments, message: "@WorkerThread can only be set on functions or methods")
                }
                try validateWorkerThreadAnnotation(functionReturnType: returnType, leadingComments: leadingComments)

                let newType = ValdiModelPropertyType.function(parameters: parameters, returnType: returnType, isSingleCall: isSingleCall, shouldCallOnWorkerThread: true)
                if parsedType.isOptional {
                    parsedType = ValdiModelPropertyType.nullable(newType)
                } else {
                    parsedType = newType
                }
            }
        }

        return ValdiModelProperty(name: name,
                                  type: parsedType,
                                  comments: resolvedPropertyMetadata?.comments,
                                  omitConstructor: resolvedPropertyMetadata?.omitConstructor,
                                  injectableParams: resolvedPropertyMetadata?.injectableParams ?? .empty)
    }

    private func parsePropertyOrParameter(propertyLikeDeclaration: TS.AST.PropertyLikeDeclaration, references: [TS.AST.TypeReference]) throws -> ValdiModelProperty {
        return try parsePropertyLike(name: propertyLikeDeclaration.name,
                                 type: propertyLikeDeclaration.type,
                                 isOptional: propertyLikeDeclaration.isOptional,
                                 leadingComments: propertyLikeDeclaration.leadingComments,
                                 references: references)
    }

    func export() -> Promise<(ExportedType, ValdiClassMapping)> {
        if dumpedSymbol.enum != nil {
            return exportEnum()
        } else {
            return exportValdiModel()
        }
    }

    private func parseEnumMember(enumMember: TS.AST.EnumMember, sequence: EnumMemberSequence) throws -> EnumMember {
        if let numberValue = enumMember.numberValue {
            guard let enumValue = Int(numberValue) else {
                throw CompilerError("Could not parse number '\(numberValue)' in enum member \(enumMember.name) ")
            }
            sequence.setNext(enumValue + 1)
            return EnumMember(name: enumMember.name, value: .number(enumValue), comments: enumMember.leadingComments?.text)
        }

        if let stringValue = enumMember.stringValue {
            return EnumMember(name: enumMember.name, value: .string(stringValue), comments: enumMember.leadingComments?.text)
        }

        return EnumMember(name: enumMember.name, value: .number(sequence.assign()), comments: enumMember.leadingComments?.text)
    }

    func exportFunction() -> Promise<(ExportedFunction, ValdiClassMapping)> {
        guard let dumpedFunction = dumpedSymbol.function else {
            return Promise(error: CompilerError("Missing function data for '\(dumpedSymbol.text)'"))
        }

        let comments = annotatedSymbol.mergedCommentsWithoutAnnotations()

        do {
            let parameters = try dumpedFunction.type.parameters.map { try self.parsePropertyOrParameter(propertyLikeDeclaration: $0, references: self.commentedFile.references) }
            let returnType = try self.resolveType(type: dumpedFunction.type.returnValue, references: self.commentedFile.references)
            let exportedFunction = ExportedFunction(containingIosType: self.iosType,
                                                    containingAndroidTypeName: self.androidClass,
                                                    containingCppType: self.cppType,
                                                    functionName: dumpedFunction.name,
                                                    parameters: parameters,
                                                    returnType: returnType,
                                                    comments: comments)
            let classMapping = ValdiClassMapping()
            return Promise(data: (exportedFunction, classMapping))
        } catch {
            return Promise(error: error)
        }
    }

    func exportModule() -> Promise<(ExportedModule, ValdiClassMapping)>  {
        do {
            var model = ValdiModel()
            model.exportAsInterface = true
            model.cppType = cppType
            model.iosType = iosType
            model.androidClassName = androidClass

            for annotatedSymbol in commentedFile.annotatedSymbols where annotatedSymbol.symbol.modifiers?.contains("export") == true {
                if let function = annotatedSymbol.symbol.function {
                    let wrappedType = TS.AST.TSType(name: function.name,
                                                    leadingComments: nil,
                                                    function: function.type,
                                                    unions: nil,
                                                    typeReferenceIndex: nil,
                                                    array: nil,
                                                    typeArguments: nil,
                                                    isTypeParameter: nil)
                    model.properties.append(try parsePropertyLike(name: function.name,
                                                                  type: wrappedType,
                                                                  isOptional: false,
                                                                  leadingComments: dumpedSymbol.leadingComments,
                                                                  references: self.commentedFile.references))
                }

                if let variable = annotatedSymbol.symbol.variable {
                    model.properties.append(try parsePropertyLike(name: variable.name, type: variable.type, isOptional: false, leadingComments: variable.leadingComments, references: self.commentedFile.references))
                }
            }

            let modulePath = commentedFile.src.compilationPath.removing(suffixes: FileExtensions.typescriptFileExtensionsDotted)

            let classMapping = ValdiClassMapping()
            return Promise(data: (ExportedModule(model: model, modulePath: modulePath), classMapping))
        } catch let error {
            return Promise(error: error)
        }
    }

    private func exportEnum() -> Promise<(ExportedType, ValdiClassMapping)> {
        guard let dumpedEnum = dumpedSymbol.enum else {
            return Promise(error: CompilerError("Missing enum data for '\(dumpedSymbol.text)'"))
        }

        let comments = annotatedSymbol.mergedCommentsWithoutAnnotations()

        do {
            let enumSequence = EnumMemberSequence()
            let enumMembers = try dumpedEnum.members.map { try self.parseEnumMember(enumMember: $0, sequence: enumSequence) }

            let classMapping = ValdiClassMapping()

            let stringCases: [EnumCase<String>] = enumMembers.compactMap { member in
                guard case let .string(value) = member.value else { return nil }
                let comments = member.comments.map {
                    TypeScriptAnnotatedSymbol.cleanCommentString($0)
                }

                return EnumCase(name: member.name, value: value, comments: comments)
            }
            let numberCases: [EnumCase<Int>] = enumMembers.compactMap { member in
                guard case let .number(value) = member.value else { return nil }
                let comments = member.comments.map {
                    TypeScriptAnnotatedSymbol.cleanCommentString($0)
                }

                return EnumCase(name: member.name, value: value, comments: comments)
            }

            let valid = stringCases.isEmpty != numberCases.isEmpty
            guard valid else {
                return Promise(error: CompilerError("Invalid enum members: \(enumMembers)") )
            }

            let enumCases: ExportedEnum.Cases
            if !stringCases.isEmpty {
                enumCases = .stringEnum(stringCases)
            } else {
                enumCases = .enum(numberCases)
            }

            let exportedEnum = ExportedEnum(iosType: self.iosType,
                                            androidTypeName: self.androidClass,
                                            cppType: self.cppType,
                                            cases: enumCases,
                                            comments: comments)

            return Promise(data: (.enum(exportedEnum), classMapping))
        } catch {
            return Promise(error: error)
        }
    }

    private func exportValdiModel() -> Promise<(ExportedType, ValdiClassMapping)> {
        guard let dumpedInterface = dumpedSymbol.interface else {
            return Promise(error: CompilerError("Missing interface data for '\(dumpedSymbol.text)'"))
        }

        let comments = annotatedSymbol.mergedCommentsWithoutAnnotations()

        do {
            var properties: [ValdiModelProperty] = []
            for member in dumpedInterface.members {
                do {
                    let property = try self.parsePropertyOrParameter(propertyLikeDeclaration: member, references: self.commentedFile.references)
                    properties.append(property)
                } catch let error {
                    throw CompilerError("Failed to parse property \(member.name): \(error.legibleLocalizedDescription)")
                }
            }

            var model = ValdiModel()
            model.tsType = self.dumpedSymbol.text
            model.iosType = self.iosType
            model.androidClassName = self.androidClass
            model.cppType = self.cppType
            model.properties = properties
            model.comments = comments
            model.typeParameters = dumpedInterface.typeParameters?.map { ValdiTypeParameter(name: $0.name) }
            
            // Check for usePublicFields parameter in ExportModel annotation
            if let exportModelAnnotation = self.annotatedSymbol.annotations.first(where: { $0.name == "ExportModel" }) {
                model.usePublicFields = exportModelAnnotation.parameters?["usePublicFields"] == "true"
            }

            let classMapping = ValdiClassMapping()

            return Promise(data: (.valdiModel(model), classMapping))
        } catch {
            return Promise(error: error)
        }
    }
}
