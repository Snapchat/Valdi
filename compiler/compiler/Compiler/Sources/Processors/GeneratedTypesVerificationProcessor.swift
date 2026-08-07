//
//  GeneratedTypesVerificationProcessor.swift
//  Compiler
//
//  Created by saniul on 25/07/2019.
//

import Foundation

class GeneratedTypesVerificationProcessor: CompilationProcessor {
    let projectConfig: ValdiProjectConfig

    var description: String {
        return "Verifying generated types"
    }

    private let logger: ILogger

    init(logger: ILogger, projectConfig: ValdiProjectConfig) {
        self.logger = logger
        self.projectConfig = projectConfig
    }

    func process(items: CompilationItems) throws -> CompilationItems {
        try items.select { item -> GeneratedTypeDescription? in
            guard case let .generatedTypeDescription(generatedTypeDescription, _) = item.kind else {
                return nil
            }
            return generatedTypeDescription
            }.processAll { selectedItems in
                let duplicatesByPlatform = Platform.allCases.associate { (platform: Platform) -> (Platform, [String: [CompilationItem]]) in
                    let grouped = selectedItems.compactMap { selectedItem -> (String, CompilationItem)? in
                        guard let typename = selectedItem.data.typeName(for: platform) else { return nil }
                        return (typename, selectedItem.item)
                        }
                        .group { typeName, item in
                            (typeName, item)
                        }
                        .filter { _, items in
                            items.count > 1
                    }
                    return (platform, grouped)
                    }
                    .filter { _, itemsByType in
                        itemsByType.count > 0
                }
                // duplicatesByPlatform should be a [Platform: [String: [CompilationItem]]] which only contains duplicated types

                guard !duplicatesByPlatform.isEmpty else {
                    return
                }

                for (platform, dupedTypes) in duplicatesByPlatform {
                    logger.error("🚨 Conflicting generated type names for platform: \(platform.rawValue, color: .red)🚨")

                    for (index, tuple) in dupedTypes.enumerated() {
                        let (dupedType, items) = tuple
                        logger.error("\(index+1). Type: \(dupedType):")

                        for item in items {
                            logger.error("-- Source URL: \(item.sourceURL.relativePath(from: projectConfig.baseDir))")
                        }
                    }

                    logger.error("")
                }

                throw CompilerError("Conflicting generated type names")
        }

        return items
    }

}
