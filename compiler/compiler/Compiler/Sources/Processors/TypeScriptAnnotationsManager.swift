//
//  TypeScriptAnnotationsManager.swift
//  Compiler
//
//  Created by saniul on 13/06/2019.
//

import Foundation

struct ParsedAnnotation {
    let type: ValdiAnnotationType
    let annotation: ValdiTypeScriptAnnotation
    let symbol: TypeScriptAnnotatedSymbol
    let file: TypeScriptCommentedFile
    let compilationItem: CompilationItem
    let memberIndex: Int?
}

// TODO: could parse annotation parameters into the enum as associated values
enum ValdiAnnotationType: String, CaseIterable {
    case generateNativeClass = "GenerateNativeClass" // Deprecated, use ExportModel
    case generateNativeInterface = "GenerateNativeInterface" // Deprecated, use ExportProxy
    case generativeNativeEnum = "GenerateNativeEnum" // Deprecated, use ExportEnum
    case generativeNativeFunction = "GenerateNativeFunction" // Deprecated, use ExportFunction
    case exportModule = "ExportModule"
    case exportModel = "ExportModel"
    case exportProxy = "ExportProxy"
    case exportEnum = "ExportEnum"
    case exportFunction = "ExportFunction"
    case nativeTypeConverter = "NativeTypeConverter"
    case nativeClass = "NativeClass"
    case nativeInterface = "NativeInterface"
    case component = "Component"
    case action = "Action"
    case viewModel = "ViewModel"
    case context = "Context"
    case constructorOmitted = "ConstructorOmitted"
    case nativeTemplateElement = "NativeTemplateElement"
    case injectable = "Injectable"
    case singleCall = "SingleCall"
    case workerThread = "WorkerThread"
    case untypedMap = "UntypedMap"
    case untyped = "Untyped"
    
    /// Returns true if this annotation type can have ios/android parameters that indicate native exports.
    /// Used for detecting native exports in Vue files and validating annotation parameters.
    var canHaveNativeExportParameters: Bool {
        switch self {
        case .exportModule,
             .generateNativeClass,
             .generateNativeInterface,
             .generativeNativeEnum,
             .generativeNativeFunction,
             .exportModel,
             .exportProxy,
             .exportEnum,
             .exportFunction,
             .nativeClass,
             .nativeTemplateElement,
             .nativeInterface:
            return true
        case .nativeTypeConverter,
             .component,
             .action,
             .viewModel,
             .context,
             .constructorOmitted,
             .injectable,
             .singleCall,
             .workerThread,
             .untypedMap,
             .untyped:
            return false
        }
    }
    
    /// Returns the annotation names that can have native export parameters (ios/android).
    /// This is used for scanning Vue files for native exports.
    static var nativeExportAnnotationNames: [String] {
        return allCases.filter { $0.canHaveNativeExportParameters }.map { $0.rawValue }
    }
}

final class TypeScriptAnnotationsManager {
    private var records = Synchronized<[URL: [ParsedAnnotation]]>(data: [:])

    func registerAnnotation(_ parsedAnnotation: ParsedAnnotation, symbolIndex: Int, sourceURL: URL) throws {
        try TypeScriptAnnotationsManager.validateAnnotation(parsedAnnotation, symbolIndex: symbolIndex)

        records.data { data in
            data[sourceURL, default: []].append(parsedAnnotation)
        }
    }

    func listAllAnnotations() -> [(URL, ParsedAnnotation)] {
        return records.data { data in
            return data.flatMap { keyValue in
                keyValue.value.map { (keyValue.key, $0) }
            }
        }
    }

    func clear() {
        records.data { $0.removeAll() }
    }

    /// Throws if the parsed annotation's parameters are not considered valid, e.g. when the iOS/Android type names
    /// contain disallowed characters
    static func validateAnnotation(_ parsedAnnotation: ParsedAnnotation, symbolIndex: Int) throws {
        // Special validation for @ExportModule which must be at the top of the file
        if parsedAnnotation.type == .exportModule && symbolIndex != 0 {
            throw CompilerError("@ExportModule must be placed at the top of the file")
        }
        
        // Validate ios/android type names for annotations that can have native export parameters
        if parsedAnnotation.type.canHaveNativeExportParameters {
            if let iosTypeName = parsedAnnotation.annotation.parameters?["ios"] {
                guard ObjCValidation.isValidIOSTypeName(iosTypeName: iosTypeName) else {
                    throw CompilerError("Invalid iOS type name: \(iosTypeName)")
                }
            }
            if let androidTypeName = parsedAnnotation.annotation.parameters?["android"] {
                guard KotlinValidation.isValidAndroidTypeName(androidTypeName: androidTypeName) else {
                    throw CompilerError("Invalid Android type name: \(androidTypeName)")
                }
            }
        }
    }
}

func throwAnnotationError(_ annotation: ValdiTypeScriptAnnotation, _ commentedFile: TypeScriptCommentedFile, message: String) throws -> Never {
    throw CompilerError(type: "Annotation error", message: message, range: annotation.range, inDocument: commentedFile.fileContent)
}
