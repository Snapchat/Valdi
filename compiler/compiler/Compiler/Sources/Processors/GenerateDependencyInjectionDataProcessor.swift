//
//  GenerateDependencyInjectionDataProcessor.swift
//
//
//  Created by John Corbett on 5/10/23.
//

import Foundation

struct IntermediateDependencyMetadata {
    let model: ValdiModel
    let classMapping: ResolvedClassMapping
    let sourceFilename: GeneratedSourceFilename
}

final class GenerateDependencyInjectionDataProcessor: CompilationProcessor {
    let onlyFocusProcessingForModules: Set<String>
    private let logger: ILogger

    init(logger: ILogger, onlyFocusProcessingForModules: Set<String>) {
        self.logger = logger
        self.onlyFocusProcessingForModules = onlyFocusProcessingForModules
    }

    var description: String {
        "Generating Dependency Injection Data"
    }

    private func processDependencyInjectionData(groupedItems: GroupedItems<CompilationItem.BundleInfo, SelectedItem<IntermediateDependencyMetadata>>) throws -> [CompilationItem] {
        var out: [CompilationItem] = []
        let bundleInfo = groupedItems.key
        let items = groupedItems.items

        let outputTarget = items[0].item.outputTarget

        assert(items.allSatisfy { selectedItem in selectedItem.item.outputTarget == outputTarget })

        // Process iOS dependency data.
        let intermediateMetadata = items.map { item in item.data }
        let generator = ObjCDependencyRepresentationGenerator(items: intermediateMetadata, singleFileCodegen: bundleInfo.singleFileCodegen)
        let metadataModel = try DependencyMetadata(file: generator.generate())

        out.append(CompilationItem(sourceURL: bundleInfo.baseDir,
                                   relativeProjectPath: nil,
                                   kind: .dependencyMetadata(metadataModel),
                                   bundleInfo: bundleInfo,
                                   platform: .ios,
                                   outputTarget: outputTarget))

        // on Android, each item is processed individually as we generate the factory in the Valdi compiler.
        for item in items {
            let model = item.data.model
            let resolvedClassMapping = item.data.classMapping
            let sourceFileName = item.data.sourceFilename
            guard let androidClassName = model.androidClassName else {
                continue
            }

            do {
                let jvmClass = JVMClass(fullClassName: androidClassName)
                let modelGenerator = KotlinFactoryGenerator(jvmClass: jvmClass,
                                                            typeParameters: model.typeParameters,
                                                            properties: model.properties,
                                                            classMapping: resolvedClassMapping,
                                                            sourceFileName: sourceFileName)
                let file = try modelGenerator.write()
                let nativeSource = KotlinCodeGenerator.makeNativeSource(bundleInfo: item.item.bundleInfo,
                                                                        jvmClass: jvmClass,
                                                                        file: file,
                                                                        fileNameOverride: "\(jvmClass.name)_ContextFactory.\(FileExtensions.kotlin)")
                out.append(CompilationItem(sourceURL: bundleInfo.baseDir,
                                           relativeProjectPath: nil,
                                           kind: .nativeSource(nativeSource),
                                           bundleInfo: bundleInfo,
                                           platform: .android,
                                           outputTarget: outputTarget))
            } catch {
                logger.error("Failed to generate factory for \(item.item.sourceURL.path): \(error.legibleLocalizedDescription)")
                out.append(item.item.with(error: error))
            }
        }

        return out
    }

    private func shouldProcessItem(item: CompilationItem) -> Bool {
        guard !onlyFocusProcessingForModules.isEmpty else {
            return true
        }
        return onlyFocusProcessingForModules.contains(item.bundleInfo.name)
    }

    func process(items: CompilationItems) throws -> CompilationItems {
        let bundlesToContexts = items.allItems.compactMap { dependencyItem -> (CompilationItem.BundleInfo, String)? in
            guard case let .exportedDependencyMetadata(model, _, _) = dependencyItem.kind,
                  let iosType = model.iosType,
                  model.properties.contains(where: { param in param.injectableParams.compatibility.contains(.ios) })
            else {
                return nil
            }

            return (dependencyItem.bundleInfo, iosType.name)
        }.reduce(into: [String: [String]]()) { partialResult, bundleTypePair in
            let bundleName = bundleTypePair.0.iosModuleName
            let iosTypeName = bundleTypePair.1
            partialResult[bundleName, default: []].append(iosTypeName)
        }

        return try items
            .select { item -> IntermediateDependencyMetadata? in
                guard case let .exportedDependencyMetadata(model, classMapping, filename) = item.kind,
                      shouldProcessItem(item: item)
                else {
                    return nil
                }

                return IntermediateDependencyMetadata(
                    model: model,
                    classMapping: classMapping,
                    sourceFilename: filename
                )
            }
            .groupBy { item -> CompilationItem.BundleInfo in
                item.item.bundleInfo
            }
            .transformEachConcurrently(processDependencyInjectionData)
    }
}
