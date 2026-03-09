//
//  ParseTypeScriptAnnotationsProcessor.swift
//  Compiler
//
//  Created by saniul on 13/06/2019.
//

import Foundation

final class ParseTypeScriptAnnotationsProcessor: CompilationProcessor {

    var description: String {
        return "Parsing TypeScript Annotations"
    }

    private let logger: ILogger
    private let projectClassMappingManager: ProjectClassMappingManager
    private let annotationsManager: TypeScriptAnnotationsManager
    private let typeScriptCompilerManager: TypeScriptCompilerManager

    init(logger: ILogger, projectClassMappingManager: ProjectClassMappingManager, typeScriptCompilerManager: TypeScriptCompilerManager, annotationsManager: TypeScriptAnnotationsManager) {
        self.logger = logger
        self.projectClassMappingManager = projectClassMappingManager
        self.typeScriptCompilerManager = typeScriptCompilerManager
        self.annotationsManager = annotationsManager
    }

    func process(items: CompilationItems) throws -> CompilationItems {
        let selectedItems: SelectedCompilationItems<SelectedItem<DumpedTypeScriptSymbolsResult>> = items.select { item in
            guard !item.bundleInfo.disableAnnotationProcessing else {
                return nil
            }

            switch item.kind {
            case let .dumpedTypeScriptSymbols(result):
                return result
            default:
                return nil
            }
        }

        annotationsManager.clear()
        let transformed = try selectedItems.transformEachConcurrently(parseAnnotations(selectedItem:))

        return transformed
    }

    // get comments from all TypeScript files, update the annotations manager with parsed annotations and return list of the input compilation items
    // with possible injected errors
    private func parseAnnotations(selectedItem: SelectedItem<DumpedTypeScriptSymbolsResult>) throws -> [CompilationItem] {
        let typeScriptItem = selectedItem.data.typeScriptItemAndSymbols.typeScriptItem
        let result = selectedItem.data.typeScriptItemAndSymbols.dumpedSymbols
        let compilationItem = typeScriptItem.item

        let sourceURL = compilationItem.sourceURL

        do {
            let commentedFile = try TypeScriptCommentedFile(src: typeScriptItem.src,
                                                            file: typeScriptItem.file,
                                                            dumpedSymbolsResult: result,
                                                            typeScriptCompiler: typeScriptCompilerManager.typeScriptCompiler)
            for (symbolIndex, annotatedSymbol) in commentedFile.annotatedSymbols.enumerated() {

                for annotation in annotatedSymbol.annotations {
                    guard let annotationType = ValdiAnnotationType(rawValue: annotation.name) else {
                        try throwAnnotationError(annotation, commentedFile, message: "Unrecognized annotation '\(annotation.name)'")
                    }

                    let parsedAnnotation = ParsedAnnotation(type: annotationType,
                                                            annotation: annotation,
                                                            symbol: annotatedSymbol,
                                                            file: commentedFile,
                                                            compilationItem: compilationItem,
                                                            memberIndex: nil)

                    try annotationsManager.registerAnnotation(parsedAnnotation, symbolIndex: symbolIndex, sourceURL: sourceURL)

                    if case .nativeTemplateElement = annotationType {
                        guard let name = annotatedSymbol.symbol.text.nonEmpty else {
                            throw CompilerError("@NativeTemplateElement needs a name")
                        }

                        let isSlot = annotation.parameters?["slot"] == "true"
                        let isLayout = annotation.parameters?["layout"] == "true"

                        let iosTypeName = annotation.parameters?["ios"]?.nonEmpty
                        let androidClassName = annotation.parameters?["android"]?.nonEmpty
                        let webViewClassName = annotation.parameters?["web"]?.nonEmpty
                        let macosViewClassName = annotation.parameters?["macos"]?.nonEmpty
                        let jsxName = annotation.parameters?["jsx"]?.nonEmpty

                        guard isSlot || isLayout || (iosTypeName != nil && androidClassName != nil) else {
                            throw CompilerError("@NativeTemplateElement needs 'ios' and 'android' typenames")
                        }

                        let iosType = iosTypeName.map { IOSType(name: $0, bundleInfo: selectedItem.item.bundleInfo, kind: .class, iosLanguage: compilationItem.bundleInfo.iosLanguage) }
                        try projectClassMappingManager.mutate { (classMapping) in
                            try classMapping.register(bundle: compilationItem.bundleInfo,
                                                      nodeType: name,
                                                      valdiViewPath: nil,
                                                      iosType: iosType,
                                                      androidClassName: androidClassName,
                                                      webViewClassName: webViewClassName,
                                                      macosViewClassName: macosViewClassName,
                                                      jsxName: jsxName,
                                                      isLayout: isLayout,
                                                      isSlot: isSlot,
                                                      sourceFilePath: compilationItem.relativeBundleURL.path)
                        }
                    }
                }

                for (memberIndex, annotations) in annotatedSymbol.memberAnnotations.sorted(by: { $0.key < $1.key  }) {
                    for annotation in annotations {
                        guard let annotationType = ValdiAnnotationType(rawValue: annotation.name) else {
                            try throwAnnotationError(annotation, commentedFile, message: "Unrecognized member annotation '\(annotation.name)'")
                        }

                        let parsedAnnotation = ParsedAnnotation(type: annotationType,
                                                                annotation: annotation,
                                                                symbol: annotatedSymbol,
                                                                file: commentedFile,
                                                                compilationItem: compilationItem,
                                                                memberIndex: memberIndex)

                        try annotationsManager.registerAnnotation(parsedAnnotation, symbolIndex: symbolIndex, sourceURL: sourceURL)
                    }
                }
            }
            return [selectedItem.item]
        } catch let error {
            logger.error("Error parsing annotations in file \(typeScriptItem.src.compilationPath): \(error.legibleLocalizedDescription)")
            return [selectedItem.item.with(error: error)]
        }
    }
}
