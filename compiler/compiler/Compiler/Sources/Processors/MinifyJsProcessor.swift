//
//  MinifyJsProcessor.swift
//  Compiler
//
//  Created by Simon Corsin on 9/19/18.
//  Copyright © 2018 Snap Inc. All rights reserved.
//

import Foundation

enum Minifier: String {
    case uglifyJS = "uglify-js"
    case terser = "terser"
}

let minifierToUse = Minifier.terser

struct BatchMinifyResult: Codable {
    let fileURL: String
    let uglified: String?
    let error: String?
}

// [.javaScript] -> [.javaScript]
class MinifyJsProcessor: CompilationProcessor {
    let companion: CompanionExecutable

    var description: String {
        return "Minify JavaScript"
    }

    private let logger: ILogger
    private var diskCache: DiskCache?
    private let minifyOptions: String?
    private let disableMinifyWeb: Bool

    // These minify options are the same as minifyOptions, but will keep preserve line numbers
    private let beautifyMinifyOptions: String?

    // QuickJS stack traces don't include line numbers, so we need to beautify the output for proper tracing.
    private let androidIsQuickJS: Bool

    // The set of native platforms active for this compilation invocation. Used to tag output
    // items with the correct platform so downstream processors (e.g. SourceMapProcessor) can
    // route them to platform-specific output directories. Falls back to [nil] when no native
    // output is configured (e.g. web-only builds).
    private let enabledNativePlatforms: [Platform?]

    init(logger: ILogger, diskCacheProvider: DiskCacheProvider, projectConfig: ValdiProjectConfig, companion: CompanionExecutable, disableMinifyWeb: Bool) {
        self.logger = logger
        self.companion = companion
        self.disableMinifyWeb = disableMinifyWeb
        self.minifyOptions = projectConfig.minifyConfigURL.flatMap { try? String(contentsOf: $0, encoding: .utf8 )}
        self.androidIsQuickJS = projectConfig.androidJsBytecodeFormat == "quickjs"

        var nativePlatforms: [Platform?] = []
        if projectConfig.iosOutput?.codegenEnabled == true     { nativePlatforms.append(.ios) }
        if projectConfig.androidOutput?.codegenEnabled == true { nativePlatforms.append(.android) }
        self.enabledNativePlatforms = nativePlatforms.isEmpty ? [nil] : nativePlatforms

        do {
            let minifyOptionsData = minifyOptions?.data(using: .utf8) ?? Data()
            var minifyOptionsJson = try JSONSerialization.jsonObject(with: minifyOptionsData, options: []) as? [String: Any] ?? [:]
            minifyOptionsJson["output"] = ["beautify": true, "indent_level": 0]
            let beautifyMinifyOptionsData = try JSONSerialization.data(withJSONObject: minifyOptionsJson, options: [.prettyPrinted])
            self.beautifyMinifyOptions = String(data: beautifyMinifyOptionsData, encoding: .utf8)
        } catch {
            self.beautifyMinifyOptions = nil
        }

        self.diskCache = diskCacheProvider.newCache(cacheName: "uglify_build",
                                                    outputExtension: FileExtensions.javascript,
                                                    metadata: ["minifier": minifierToUse.rawValue,
                                                                "options": self.minifyOptions ?? ""
                                                                ])
    }

    private func doMinifyBatch(items: [SelectedItem<JavaScriptFile>], minifyOptions: String?) throws -> [CompilationItem] {
        items.forEach {
            logger.debug("-- Minifying JavaScript file \($0.data.relativePath)")
        }

        let itemsAndURLHandles = try items.parallelMap { (item: $0, urlHandle: try $0.data.file.getURLHandle()) }

        return try withExtendedLifetime(itemsAndURLHandles) { _ -> [CompilationItem] in
            let urls = itemsAndURLHandles.map { $0.urlHandle.url }

            let uglifyResults = try companion.batchMinifyJS(inputURLs: urls,
                                                            minifier: minifierToUse,
                                                            options: minifyOptions).waitForData()

            let uglifyResultsByFileURL = try uglifyResults.associateUnique { ($0.fileURL, $0) }

            return try itemsAndURLHandles.map {
                let compilationItem = $0.item.item

                guard let uglifyResult = uglifyResultsByFileURL[$0.urlHandle.url.absoluteString] else {
                    return compilationItem.with(error: CompilerError("Couldn't get uglify result"))
                }

                if let errorMessage = uglifyResult.error {
                    return compilationItem.with(error: CompilerError("Minify error: \(errorMessage)"))
                } else {
                    let data = (try uglifyResult.uglified?.utf8Data()) ?? Data()
                    let outputJs = $0.item.data.with(file: .data(data))
                    return compilationItem.with(newKind: .javaScript(outputJs))
                }
            }
        }
    }

    private func uglifyBatch(items: [SelectedItem<JavaScriptFile>], minifyOptions: String?, platforms: [Platform?]) -> [CompilationItem] {
        do {
            var itemsToMinify: [(Data, SelectedItem<JavaScriptFile>)] = []
            var outItems: [CompilationItem] = []

            for selectedItem in items {
                let inputData = try selectedItem.data.file.readData() + (minifyOptions?.data(using: .utf8) ?? Data())

                if let cachedData = diskCache?.getOutput(compilationItem: selectedItem.item, inputData: inputData) {
                    let outputJsFile = selectedItem.data.with(file: .data(cachedData))
                    outItems.append(selectedItem.item.with(newKind: .javaScript(outputJsFile), newPlatform: selectedItem.item.platform, newOutputTarget: .debug))
                    if selectedItem.item.outputTarget.contains(.release) {
                        outItems.append(selectedItem.item.with(newKind: .javaScript(outputJsFile), newPlatform: selectedItem.item.platform, newOutputTarget: .release))
                    }
                } else {
                    itemsToMinify.append((inputData, selectedItem))
                }
            }

            if !itemsToMinify.isEmpty {
                let uglifiedItems = try doMinifyBatch(items: itemsToMinify.map { $0.1 }, minifyOptions: minifyOptions)

                for (index, item) in uglifiedItems.enumerated() {
                    switch item.kind {
                    case .javaScript(let jsFile):
                        for platform in platforms {
                            outItems.append(item.with(newKind: item.kind, newPlatform: platform, newOutputTarget: .debug))
                            if item.outputTarget.contains(.release) {
                                outItems.append(item.with(newKind: item.kind, newPlatform: platform, newOutputTarget: .release))
                            }
                        }
                        let inputData = itemsToMinify[index].0
                        try diskCache?.setOutput(compilationItem: item, inputData: inputData, outputData: try jsFile.file.readData())
                    default:
                        outItems.append(item)
                    }
                }
            }

            return outItems
        } catch {
            return items.map { $0.item.with(error: error) }
        }
    }

    private func passThrough(items: [SelectedItem<JavaScriptFile>], platforms: [Platform?]) -> [CompilationItem] {
        do {
            var passThroughItems: [(Data, SelectedItem<JavaScriptFile>)] = []
            var outItems: [CompilationItem] = []

            for selectedItem in items {
                let inputData = try selectedItem.data.file.readData()

                if let cachedData = diskCache?.getOutput(compilationItem: selectedItem.item, inputData: inputData) {
                    let outputJsFile = selectedItem.data.with(file: .data(cachedData))
                    outItems.append(selectedItem.item.with(newKind: .javaScript(outputJsFile), newPlatform: selectedItem.item.platform, newOutputTarget: .debug))
                    if selectedItem.item.outputTarget.contains(.release) {
                        outItems.append(selectedItem.item.with(newKind: .javaScript(outputJsFile), newPlatform: selectedItem.item.platform, newOutputTarget: .release))
                    }
                } else {
                    passThroughItems.append((inputData, selectedItem))
                }
            }

            for (index, rawItem) in passThroughItems.enumerated() {
                let item = rawItem.1.item
                switch item.kind {
                case .javaScript(let jsFile):
                    for platform in platforms {
                        outItems.append(item.with(newKind: item.kind, newPlatform: platform, newOutputTarget: .debug))
                        if item.outputTarget.contains(.release) {
                            outItems.append(item.with(newKind: item.kind, newPlatform: platform, newOutputTarget: .release))
                        }
                    }
                    let inputData = passThroughItems[index].0
                    try diskCache?.setOutput(compilationItem: item, inputData: inputData, outputData: try jsFile.file.readData())
                default:
                    outItems.append(item)
                }
            }
            return outItems
        } catch {
            return items.map { $0.item.with(error: error) }
        }
    }

    func process(items: CompilationItems) throws -> CompilationItems {
        let selectedItems = items.select { item -> JavaScriptFile? in
            switch item.kind {
            case .javaScript(let file):
                return file
            default:
                return nil
            }
        }
        return selectedItems.transformInBatches { items -> [CompilationItem] in
            if androidIsQuickJS && self.beautifyMinifyOptions != nil {
                var batches = uglifyBatch(items: items, minifyOptions: self.minifyOptions, platforms: [.ios]) +
                    uglifyBatch(items: items, minifyOptions: self.beautifyMinifyOptions, platforms: [.android]) 
                if disableMinifyWeb {
                    batches += passThrough(items: items, platforms: [.web])
                } else {
                    batches += uglifyBatch(items: items, minifyOptions: self.minifyOptions, platforms: [.web])
                }
                return batches
            } else {
                // All enabled platforms share the same minify options. Tag items with their
                // platform so downstream processors (e.g. SourceMapProcessor) can route outputs
                // correctly, including when only a subset of platforms is being compiled.
                var batches = uglifyBatch(items: items, minifyOptions: self.minifyOptions, platforms: enabledNativePlatforms)
                if !enabledNativePlatforms.contains(nil) {
                    // Native platforms are active; also produce web-tagged items so downstream
                    // web processors receive output during multi-platform (native + web) builds.
                    if disableMinifyWeb {
                        batches += passThrough(items: items, platforms: [.web])
                    } else {
                        batches += uglifyBatch(items: items, minifyOptions: self.minifyOptions, platforms: [.web])
                    }
                }
                return batches
            }
        }
    }
}
