//
//  GenerateViewModelsProcessor.swift
//  Compiler
//
//  Created by Simon Corsin on 7/24/18.
//  Copyright © 2018 Snap Inc. All rights reserved.
//

import Foundation

// [.document, .model] -> [.document, .nativeSource]
final class GenerateModelsProcessor: CompilationProcessor {

    fileprivate struct GroupingKey: Equatable, Hashable {
        let platform: Platform?
        let groupingIdentifier: String
    }

    private let logger: ILogger
    private let compilerConfig: CompilerConfig
    private let generateNativeSources: Bool

    init(logger: ILogger, compilerConfig: CompilerConfig, generateNativeSources: Bool) {
        self.logger = logger
        self.compilerConfig = compilerConfig
        self.generateNativeSources = generateNativeSources
    }

    var description: String {
        return "Generating Native Models"
    }

    private func doGenerate<T: NativeSourceGenerator>(item: CompilationItem,
                                                      intermediateItem: IntermediateItem,
                                                      iosType: IOSType?,
                                                      androidClassName: String?,
                                                      cppType: CPPType?,
                                                      generationType: String,
                                                      generator: T) -> [CompilationItem] {
        do {
            let nativeSourceParameters = NativeSourceParameters(bundleInfo: item.bundleInfo,
                                                                sourceFileName: intermediateItem.sourceFilename,
                                                                classMapping: intermediateItem.classMapping,
                                                                iosType: iosType,
                                                                androidTypeName: androidClassName,
                                                                cppType: cppType)

            let codes = try generator.generate(parameters: nativeSourceParameters)
            for code in codes {
                logger.debug("-- Generated \(code.source.filename)")
            }

            return codes.map { item.with(newKind: .nativeSource($0.source), newPlatform: $0.platform) }
        } catch let error {
            logger.error("Failed to generate \(generationType) of \(item.sourceURL.path): \(error.legibleLocalizedDescription)")
            return [item.with(error: error)]
        }
    }

    private struct IntermediateItem {
        let sourceFilename: GeneratedSourceFilename
        let exportedType: ExportedType
        let classMapping: ResolvedClassMapping
    }

    private func typeDescription(for exportedType: ExportedType, baseline: String?) -> GeneratedTypeDescription {
        switch exportedType {
        case let .valdiModel(model):
            if model.exportAsInterface {
                return .interface(GeneratedNativeInterfaceDescription(model: model, baseline: baseline))
            } else {
                return .class(GeneratedNativeClassDescription(model: model, baseline: baseline))
            }
        case let .enum(exportedEnum):
            return .enum(GeneratedEnumDescription.from(exportedEnum: exportedEnum, baseline: baseline))
        case let .function(exportedFunction):
            return .function(GeneratedFunctionDescription.from(exportedFunction: exportedFunction, baseline: baseline))
        case let .module(exportedModule):
            return .module(GeneratedNativeInterfaceDescription(model: exportedModule.model, baseline: baseline))
        }
    }

    private func generate(selectedItem: SelectedItem<[IntermediateItem]>) -> [CompilationItem] {
        var out = [CompilationItem]()
        for item in selectedItem.data {
            if generateNativeSources {
                switch item.exportedType {
                case .valdiModel(let valdiModel):
                    out += doGenerate(item: selectedItem.item,
                                      intermediateItem: item,
                                      iosType: valdiModel.iosType,
                                      androidClassName: valdiModel.androidClassName,
                                      cppType: valdiModel.cppType,
                                      generationType: "model",
                                      generator: ValdiModelGenerator(model: valdiModel))
                case .enum(let exportedEnum):
                    out += doGenerate(item: selectedItem.item,
                                      intermediateItem: item,
                                       iosType: exportedEnum.iosType,
                                       androidClassName: exportedEnum.androidTypeName,
                                       cppType: exportedEnum.cppType,
                                       generationType: "enum",
                                       generator: ExportedEnumGenerator(exportedEnum: exportedEnum))
                case .function(let exportedFunc):
                    out += doGenerate(item: selectedItem.item,
                                      intermediateItem: item,
                                      iosType: exportedFunc.containingIosType,
                                      androidClassName: exportedFunc.containingAndroidTypeName,
                                      cppType: exportedFunc.containingCppType,
                                      generationType: "function",
                                      generator: ExportedFunctionGenerator(exportedFunction: exportedFunc, modulePath: selectedItem.item.relativeBundleURL.deletingPathExtension().absoluteString))
                case .module(let exportedModule):
                    out += doGenerate(item: selectedItem.item,
                                      intermediateItem: item,
                                      iosType: exportedModule.model.iosType,
                                      androidClassName: exportedModule.model.androidClassName,
                                      cppType: exportedModule.model.cppType,
                                      generationType: "module",
                                      generator: ExportedModuleGenerator(bundleInfo: selectedItem.item.bundleInfo, exportedModule: exportedModule))
                }
            }

            let baseline = selectedItem.item.bundleInfo.projectConfig.nativeApiMinVersion.map(String.init)
            let description = typeDescription(for: item.exportedType, baseline: baseline)
            let newItem = selectedItem.item.with(
                newKind: .generatedTypeDescription(
                    description,
                    src: item.sourceFilename.src
                ),
                newPlatform: .none
            )
            out.append(newItem)
        }

        if case .document = selectedItem.item.kind {
            // For documents, we want to keep the original file.
            out.append(selectedItem.item)
        }

        return out
    }

    private func shouldProcessItem(item: CompilationItem) -> Bool {
        return shouldProcessBundle(bundle: item.bundleInfo)
    }

    private func shouldProcessBundle(bundle: CompilationItem.BundleInfo) -> Bool {
        guard !compilerConfig.onlyGenerateNativeCodeForModules.isEmpty else {
            return true
        }
        return compilerConfig.onlyGenerateNativeCodeForModules.contains(bundle.name)
    }

    func process(items: CompilationItems) throws -> CompilationItems {
        let intermediateItems = items.select { (item) -> [IntermediateItem]? in
            guard shouldProcessItem(item: item) else {
                return nil
            }

            if case .document(let result) = item.kind, let viewModel = result.originalDocument.viewModel {
                let generatedSourceFilename = GeneratedSourceFilename(
                    filename: result.componentPath.fileName,
                    symbolName: result.componentPath.exportedMember,
                    src: TypeScriptItemSrc(
                        compilationPath: item.relativeProjectPath,
                        sourceURL: item.sourceURL
                    )
                )
                var out = [IntermediateItem]()
                out.append(IntermediateItem(sourceFilename: generatedSourceFilename,
                                            exportedType: .valdiModel(viewModel),
                                            classMapping: result.classMapping))

                for childModel in result.originalDocument.additionalModels {
                    out.append(IntermediateItem(sourceFilename: generatedSourceFilename,
                                                exportedType: .valdiModel(childModel),
                                                classMapping: result.classMapping))
                }
                return out
            } else if case .exportedType(let exportedType, let classMapping, let generatedSourceFilename) = item.kind {
                return [IntermediateItem(sourceFilename: generatedSourceFilename,
                                         exportedType: exportedType,
                                         classMapping: classMapping)]
            } else {
                return nil
            }
        }

        return intermediateItems.transformEachConcurrently(generate)
    }
}
