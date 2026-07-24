//
//  GenerateViewClassesProcessor.swift
//  Compiler
//
//  Created by Simon Corsin on 7/24/18.
//  Copyright © 2018 Snap Inc. All rights reserved.
//

import Foundation

// [.document] -> [.document, .nativeSource]
class GenerateViewClassesProcessor: CompilationProcessor {

    private let logger: ILogger
    private let compilerConfig: CompilerConfig

    init(logger: ILogger, compilerConfig: CompilerConfig) {
        self.logger = logger
        self.compilerConfig = compilerConfig
    }

    var description: String {
        return "Generating View Classes"
    }

    private func generateViewClass(item: CompilationItem, compilationResult: CompilationResult) -> [CompilationItem] {
        let rawDocument = compilationResult.originalDocument
        let compiledDocument = compilationResult.templateResult

        let rootNodeType = rawDocument.template?.rootNode?.nodeType ?? ""
        guard let classMapping = rawDocument.classMapping?.nodeMappingByClass[rootNodeType] else {
                logger.debug("Not generating view class for \(item.sourceURL.path) because the root node doesn't have a custom class.")
                return []
        }
        let generatedSourceFilename = GeneratedSourceFilename(
            filename: item.relativeProjectPath,
            symbolName: compilationResult.componentPath.exportedMember,
            src: TypeScriptItemSrc(
                compilationPath: item.relativeProjectPath,
                sourceURL: item.sourceURL
            )
        )
        let iosType = classMapping.iosType
        let androidClassName = classMapping.androidClassName

        let generator = ViewClassGenerator(bundleInfo: item.bundleInfo,
                                           logger: logger,
                                           compiledDocument: compiledDocument,
                                           rawDocument: rawDocument,
                                           componentPath: compilationResult.componentPath,
                                           sourceFilename: generatedSourceFilename,
                                           iosType: iosType,
                                           iosLanguage: item.bundleInfo.iosLanguage,
                                           androidClassName: androidClassName)
        do {
            guard let result = try generator.generate() else {
                return []
            }

            for nativeSource in result.nativeSources {
                logger.debug("-- Generated \(nativeSource.source.filename)")
            }
            let nativeSourceItems = result.nativeSources.map { item.with(newKind: .nativeSource($0.source), newPlatform: $0.platform) }
            let typeDescription = GeneratedTypeDescription.viewClass(result.description)
            let descriptionItem = item.with(
                newKind: .generatedTypeDescription(
                    typeDescription,
                    src: generatedSourceFilename.src
                ),
                newPlatform: .none
            )
            return nativeSourceItems + [descriptionItem]
        } catch let error {
            logger.error("Failed to generate View class for \(item.sourceURL.path): \(error.legibleLocalizedDescription)")
            return []
        }
    }

    func generateViewClass(selectedItem: SelectedItem<CompilationResult>) -> [CompilationItem] {
        var out = generateViewClass(item: selectedItem.item,
                                    compilationResult: selectedItem.data)
        out.append(selectedItem.item)
        return out
    }

    private func shouldProcessItem(item: CompilationItem) -> Bool {
        guard !compilerConfig.onlyGenerateNativeCodeForModules.isEmpty else {
            return true
        }
        return compilerConfig.onlyGenerateNativeCodeForModules.contains(item.bundleInfo.name)
    }

    func process(items: CompilationItems) throws -> CompilationItems {
        return items.select { (item) -> CompilationResult? in
            guard shouldProcessItem(item: item), case .document(let result) = item.kind else {
                return nil
            }

            return result
        }.transformEachConcurrently(generateViewClass)
    }

}
