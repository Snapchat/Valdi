//
//  TypeScriptCompilerManager.swift
//  Compiler
//
//  Created by saniul on 13/06/2019.
//

import Foundation

struct TypeScriptItemSrc {
    let compilationPath: String
    let sourceURL: URL
}

private struct IntermediateTypeScriptItem {
    let compilationItem: CompilationItem
    let src: TypeScriptItemSrc
    let tsFile: File
}

private struct OpenedIntermediateTypeScriptItem {
    let intermediateItem: IntermediateTypeScriptItem
    let needCompilation: Bool
}

struct TypeScriptItem {
    let src: TypeScriptItemSrc
    let file: File
    let item: CompilationItem
}

struct TypeScriptCompilationResult {
    let outItems: [CompilationItem]
    let typeScriptItems: [TypeScriptItem]
}

struct TypedScriptItemAndSymbols {
    let typeScriptItem: TypeScriptItem
    let dumpedSymbols: TS.DumpSymbolsWithCommentsResponseBody
    let restoredFromDisk: Bool
}

struct DumpAllSymbolsResult {
    let typeScriptItemsAndSymbols: [TypedScriptItemAndSymbols]
}

private struct PrepareTypeScriptItemResult {
    let item: CompilationItem
    let promise: Promise<Void>?
    let intermediateTypeScriptItem: IntermediateTypeScriptItem?
    let shouldPassThroughItem: Bool
}

final class TypeScriptCompilerManager {
    private let fileManager: ValdiFileManager

    let typeScriptCompiler: TypeScriptCompiler

    private let logger: ILogger
    private let bundleManager: BundleManager

    private let projectName: String
    private let rootURL: URL
    private let onlyCompileTypeScriptForModules: Set<String>
    private let hotReloadingEnabled: Bool

    public let outputRootURL: URL

    var emitDebug: Bool {
        return hotReloadingEnabled
    }

    private let shouldVerifyImports: Bool
    private let shouldRemoveComments: Bool
    private let tsEmitDeclarationFiles: Bool

    private var everInitializedWorkspace = false
    private let writerCache = CompilationCache<Bool>()

    init(logger: ILogger,
         fileManager: ValdiFileManager,
         typeScriptCompiler: TypeScriptCompiler,
         projectName: String,
         rootURL: URL,
         hotReloadingEnabled: Bool,
         bundleManager: BundleManager,
         projectConfig: ValdiProjectConfig,
         compilerConfig: CompilerConfig) {
        self.logger = logger
        self.fileManager = fileManager
        self.shouldVerifyImports = !compilerConfig.tsSkipVerifyingImports
        self.shouldRemoveComments = !compilerConfig.tsKeepComments
        self.tsEmitDeclarationFiles = compilerConfig.tsEmitDeclarationFiles
        self.typeScriptCompiler = typeScriptCompiler
        self.bundleManager = bundleManager

        self.projectName = projectName
        self.rootURL = rootURL
        self.hotReloadingEnabled = hotReloadingEnabled

        self.outputRootURL = projectConfig.buildDirectoryURL.appendingPathComponent("typescript/output", isDirectory: true)
        self.onlyCompileTypeScriptForModules = compilerConfig.onlyCompileTypeScriptForModules

        if FileManager.default.fileExists(atPath: self.outputRootURL.path) {
            try? FileManager.default.removeItem(at: outputRootURL)
        }
    }

    private class func getRelativeProjectPathWithoutTSExtension(compilationItem: CompilationItem) -> String {
        return compilationItem.relativeProjectPath.removing(suffixes: FileExtensions.typescriptFileExtensionsDotted)
    }

    private func filterIntermediateTypeScriptItems(_ intermediateTypeScriptItems: [IntermediateTypeScriptItem], filter: Set<String>) -> [IntermediateTypeScriptItem] {
        guard !filter.isEmpty else {
            return intermediateTypeScriptItems
        }

        return intermediateTypeScriptItems.filter {
            return $0.compilationItem.bundleInfo.isRoot || filter.contains($0.compilationItem.bundleInfo.name)
        }
    }

    static private func isBundleInfoIncluded(_ bundleInfo: CompilationItem.BundleInfo, filter: Set<String>) -> Bool {
        return !bundleInfo.isRoot && (filter.isEmpty || filter.contains(bundleInfo.name))
    }

    func checkItems(compileSequence: CompilationSequence, items: [CompilationItem], onlyCompileTypeScriptForModules: Set<String>) throws -> TypeScriptCompilationResult {
        // Intermediates directory prepared with _all_ TypeScript files
        var (allTypeScriptItems, intermediateTypeScriptItems, _, outItems) = try prepareIntermediateItems(for: items, compilationSequence: compileSequence, dotTsOnly: false)

        // We only open and typecheck files for the requested modules, if filter is present
        let filteredIntermediateTypeScriptItems = filterIntermediateTypeScriptItems(intermediateTypeScriptItems, filter: onlyCompileTypeScriptForModules)

        let checkResult = openFiles(items: filteredIntermediateTypeScriptItems, compileSequence: compileSequence).then { (items) -> Promise<[CompilationItem]> in
            return Promise.of(promises: items.map { self.check(item: $0, compileSequence: compileSequence) })
        }

        outItems += try checkResult.waitForData()

        return TypeScriptCompilationResult(outItems: outItems, typeScriptItems: allTypeScriptItems)
    }

    func prepareItemsForSymbolDumping(compileSequence: CompilationSequence, items: [CompilationItem], onlyCompileTypeScriptForModules: Set<String>) throws -> TypeScriptCompilationResult {
        // Similar to checkItems but skips the actual type checking
        // This is used in regenerate mode where generated files may not exist
        var (allTypeScriptItems, intermediateTypeScriptItems, _, outItems) = try prepareIntermediateItems(for: items, compilationSequence: compileSequence, dotTsOnly: false)

        // We only open files for the requested modules, if filter is present
        let filteredIntermediateTypeScriptItems = filterIntermediateTypeScriptItems(intermediateTypeScriptItems, filter: onlyCompileTypeScriptForModules)

        // Just open the files without type checking
        let openResult = openFiles(items: filteredIntermediateTypeScriptItems, compileSequence: compileSequence).then { (items) -> [CompilationItem] in
            return items.map { $0.compilationItem }
        }

        outItems += try openResult.waitForData()

        return TypeScriptCompilationResult(outItems: outItems, typeScriptItems: allTypeScriptItems)
    }

    func compileItems(compileSequence: CompilationSequence, items: [CompilationItem], onlyCompileTypeScriptForModules: Set<String>) throws -> TypeScriptCompilationResult {
        var (allTypeScriptItems, intermediateTypeScriptItems, jsItems, outItems) = try prepareIntermediateItems(for: items, compilationSequence: compileSequence, dotTsOnly: true)

        // Only compile and emit files for the requested modules, if filter is present
        let filteredIntermediateTypeScriptItems = filterIntermediateTypeScriptItems(intermediateTypeScriptItems, filter: onlyCompileTypeScriptForModules)

        let compilationResults = openFiles(items: filteredIntermediateTypeScriptItems, compileSequence: compileSequence).then { (items) -> [Promise<[CompilationItem]>] in
            return items.map { self.compile(item: $0, compileSequence: compileSequence) }
        }

        for promise in try compilationResults.waitForData() {
            outItems += try promise.waitForData()
        }

        var compilationPathsByBundleInfo: [CompilationItem.BundleInfo: [String]] = [:]

        for jsItem in jsItems where TypeScriptCompilerManager.isBundleInfoIncluded(jsItem.item.bundleInfo, filter: onlyCompileTypeScriptForModules) {
            compilationPathsByBundleInfo[jsItem.item.bundleInfo, default: []].append(jsItem.src.compilationPath)
        }

        for tsItem in filteredIntermediateTypeScriptItems {
            compilationPathsByBundleInfo[tsItem.compilationItem.bundleInfo, default: []].append(tsItem.src.compilationPath)
        }

        // We generate native c files even for modules that have no .ts / .js files
        for item in outItems where TypeScriptCompilerManager.isBundleInfoIncluded(item.bundleInfo, filter: onlyCompileTypeScriptForModules) {
            if compilationPathsByBundleInfo[item.bundleInfo] == nil {
                compilationPathsByBundleInfo[item.bundleInfo] = []
            }
        }

        var compileNativePromises: [Promise<[CompilationItem]>] = []
        for (bundleInfo, compilationPaths) in compilationPathsByBundleInfo {
            let compilationPathsWithNativeCompilation = compilationPaths.filter {  bundleInfo.compilationModeConfig.resolveCompilationMode(forItemPath: $0) == .native }
            if !compilationPathsWithNativeCompilation.isEmpty {
                compileNativePromises.append(self.compileNative(bundleInfo: bundleInfo, compilationPaths: compilationPathsWithNativeCompilation, compileSequence: compileSequence))
            } else {
                // Add empty sources as it will be used by the Bazel rule
                compileNativePromises.append(Promise(data: TypeScriptCompilerManager.generateTSNativeSource(bundleInfo: bundleInfo, inputNativeSources: [])))
            }
        }

        for compileNativePromise in compileNativePromises {
            outItems += try compileNativePromise.waitForData()
        }

        return TypeScriptCompilationResult(outItems: outItems, typeScriptItems: allTypeScriptItems)
    }

    static func generatedUrl(url: URL) -> URL {
        return url.deletingPathExtension().appendingPathExtension("generated.\(url.pathExtension)")
    }

    private func prepareIntermediateItems(for items: [CompilationItem], compilationSequence: CompilationSequence, dotTsOnly: Bool) throws -> (allTypeScriptItems: [TypeScriptItem], intermediateItems: [IntermediateTypeScriptItem], jsItems: [TypeScriptItem], outItems: [CompilationItem]) {
        var outItems = [CompilationItem]()

        var prepareResults = [PrepareTypeScriptItemResult]()
        var jsItems = [TypeScriptItem]()

        for item in items {
            if case .typeScript(let file, _) = item.kind {
                let src = getTypeScriptItemSrc(forItem: item)
                let promise = register(src: src, file: file, compilationSequence: compilationSequence)

                prepareResults.append(PrepareTypeScriptItemResult(item: item, promise: promise, intermediateTypeScriptItem: IntermediateTypeScriptItem(compilationItem: item, src: src, tsFile: file), shouldPassThroughItem: false))
            } else if case .javaScript(let jsFile) = item.kind {
                let src = getTypeScriptItemSrc(forItem: item)

                jsItems.append(TypeScriptItem(src: src, file: jsFile.file, item: item))

                if !dotTsOnly {
                    // We just write the file, no need to pass it through TypeScript, but we still
                    // put it to the intermediate directory so that TypeScript can import it
                    let promise = register(src: src, file: jsFile.file, compilationSequence: compilationSequence)


                    prepareResults.append(PrepareTypeScriptItemResult(item: item, promise: promise, intermediateTypeScriptItem: nil, shouldPassThroughItem: true))
                } else {
                    outItems.append(item)
                }
            } else if case .resourceDocument(let resource) = item.kind, !dotTsOnly && item.sourceURL.lastPathComponent.hasSuffix("tsconfig.json") {
                let src = getTypeScriptItemSrc(forItem: item)
                let promise: Promise<Void>?
                if item.sourceURL.lastPathComponent == "base.tsconfig.json" {
                    logger.debug("-- Write our own base config based on \(item.sourceURL.path)")
                    promise = registerBaseProjectConfigIfNeeded(src: src, file: resource.file, outputDirURL: outputRootURL, compilationSequence: compilationSequence)
                } else {
                    logger.debug("-- Copying TS project file to \(src.compilationPath)")
                    promise = registerTSConfig(src: src, tsConfigFile: resource.file, bundleInfo: item.bundleInfo, compilationSequence: compilationSequence)
                }

                // Pass through tsconfig.json items so they appear in BUILD file generation
                prepareResults.append(PrepareTypeScriptItemResult(item: item, promise: promise, intermediateTypeScriptItem: nil, shouldPassThroughItem: true))
            } else {
                outItems.append(item)
            }
        }

        var allTypeScriptItems = [TypeScriptItem]()
        var intermediateTypeScriptItems = [IntermediateTypeScriptItem]()

        for prepareResult in prepareResults {
            do {
                if let promise = prepareResult.promise {
                    try promise.waitForData()
                }

                if let intermediateTypeScriptItem = prepareResult.intermediateTypeScriptItem {
                    allTypeScriptItems.append(TypeScriptItem(src: intermediateTypeScriptItem.src, file: intermediateTypeScriptItem.tsFile, item: prepareResult.item))
                    intermediateTypeScriptItems.append(intermediateTypeScriptItem)
                }

                if prepareResult.shouldPassThroughItem {
                    outItems.append(prepareResult.item)
                }

                // Output declaration files but only for Web
                // url.pathExtension is just the last chunk after the last . we need the last two chunks for .d.ts
                // Can't use FileExtensions.typescriptDeclaration because it'll also match fileNamed.ts
                let url = prepareResult.item.sourceURL.absoluteString
                if url.hasSuffix(".d.ts") {
                    if case let .typeScript(file, _) = prepareResult.item.kind {
                        outItems.append(prepareResult.item.with(
                            newKind: .declaration(JavaScriptFile(file: file, relativePath: prepareResult.item.relativeProjectPath)),
                            newPlatform: Platform.web))
                    }
                }

            } catch let error {
                outItems.append(prepareResult.item.with(error: error))
            }
        }

        // Sort by natural path ordering, to get a consistent compile order
        intermediateTypeScriptItems.sort { (left, right) -> Bool in
            return left.src.compilationPath < right.src.compilationPath
        }

        if !everInitializedWorkspace {
            do {
                everInitializedWorkspace = true
                logger.debug("Initializing workspace...")
                _ = try typeScriptCompiler.initializeWorkspace().waitForData()
                logger.debug("Workspace initialized!")
            } catch {
                throw CompilerError("Failed to initialize workspace with error: \(error.legibleLocalizedDescription).")
            }
        }

        return (allTypeScriptItems, intermediateTypeScriptItems, jsItems, outItems)
    }

    private func registerBaseProjectConfigIfNeeded(src: TypeScriptItemSrc, file: File, outputDirURL: URL, compilationSequence: CompilationSequence) -> Promise<Void>? {
        guard shouldRegister(filePath: src.compilationPath, compilationSequence: compilationSequence) else { return nil }

        do {
            let tsConfig = try baseProjectConfig(rootURL: rootURL, outputDirURL: outputDirURL, originalConfigFile: file)

            let encoder = Foundation.JSONEncoder()
            encoder.outputFormatting = [.sortedKeys, .prettyPrinted]
            let encoded = try encoder.encode(tsConfig)
            let promise = typeScriptCompiler.register(file: .data(encoded), filePath: src.compilationPath)
            logger.debug("-- Wrote out base config at \(src.compilationPath)")
            return promise
        } catch {
            return Promise(error: CompilerError("Could not write base project config: \(error.legibleLocalizedDescription)"))
        }
    }

    private func baseProjectConfig(rootURL: URL, outputDirURL: URL, originalConfigFile: File) throws -> TS.Config {
        let rootPath = TypeScriptCompiler.workspaceRoot.path

        let originalConfigData = try originalConfigFile.readData()
        var baseProjectConfig = try JSONDecoder().decode(TS.Config.self, from: originalConfigData)

        var compilerOptions = baseProjectConfig.compilerOptions ?? TS.CompilerOptions()
        compilerOptions.baseUrl = rootPath
        compilerOptions.rootDir = rootPath
        compilerOptions.outDir = outputDirURL.path
        compilerOptions.removeComments = self.shouldRemoveComments
        compilerOptions.declaration = self.tsEmitDeclarationFiles
        // Using this option to have a .js output extension, since
        // we are processing the JSX inline with the TSX compilation
        compilerOptions.jsx = "react-native"

        compilerOptions.sourceMap = true
        compilerOptions.inlineSourceMap = true
        // Uncomment to enable show sources in safari debugger
        // compilerOptions.inlineSources = true
        compilerOptions.sourceRoot = "/"

        baseProjectConfig.compilerOptions = compilerOptions
        return baseProjectConfig
    }

    private func openFiles(items: [IntermediateTypeScriptItem], compileSequence: CompilationSequence) -> Promise<[IntermediateTypeScriptItem]> {
        let promises = items.map { (item) -> Promise<OpenedIntermediateTypeScriptItem> in
            return typeScriptCompiler.ensureOpenFile(filePath: item.src.compilationPath, outputDirURL: outputRootURL, compileSequence: compileSequence)
            .then { (result: EnsureOpenResult) -> Void in
                if case let .opened(importPaths: importPaths) = result {
                    try self.verifyImportPaths(item: item, filePath: item.src.compilationPath, rootURL: self.rootURL, importPaths: importPaths)
                }
            }
            .then { _ -> OpenedIntermediateTypeScriptItem in
                let isTsDeclaration = item.src.compilationPath.hasSuffix(".\(FileExtensions.typescriptDeclaration)")
                let isTS = item.src.compilationPath.hasSuffix(".\(FileExtensions.typescript)")
                let isTSX = item.src.compilationPath.hasSuffix(".\(FileExtensions.typescriptXml)")
                let needCompilation = !isTsDeclaration && (isTS || isTSX)
                return OpenedIntermediateTypeScriptItem(intermediateItem: item, needCompilation: needCompilation)
            }
        }

        return Promise.of(promises: promises).then { (items) -> [IntermediateTypeScriptItem] in
            return items.filter { $0.needCompilation }.map { $0.intermediateItem }
        }
    }

    private func verifyImportPaths(item: IntermediateTypeScriptItem, filePath: String, rootURL: URL, importPaths: [ImportPath]) throws {
        let thisModule = item.compilationItem.bundleInfo
        guard shouldVerifyImports, !importPaths.isEmpty, !thisModule.disableDependencyVerification else {
            return
        }

        let resolvedImports = importPaths.map { importPath in
            return ResolvedModuleImport(compilationPath: filePath, importPath: importPath.absolute)
        }

        if !item.compilationItem.isTestItem {
            let testImportPaths = resolvedImports.filter { $0.isTestImport }
            if !testImportPaths.isEmpty {
                // TODO(vfomin): replace log with error once sqlite testdata module is fixed
//                throw CompilerError("Files under 'src/...' should not import files under 'test/...'. File \(item.compilationItem.relativeBundleURL) from module '\(item.compilationItem.bundleInfo.name)' imports test paths: \(testImportPaths)")
                logger.warn("Files under 'src/...' should not import files under 'test/...'. File \(item.compilationItem.relativeBundleURL) from module '\(item.compilationItem.bundleInfo.name)' imports test paths: \(testImportPaths)")
            }
        }

        let importedModuleNames = resolvedImports.compactMap { $0.moduleName }
        let uniquedActualDependencies = Set(importedModuleNames).subtracting([thisModule.name])

        let declaredDependencies = item.compilationItem.bundleInfo.allDependenciesNames
        let undeclaredDependencies = uniquedActualDependencies.subtracting(declaredDependencies)
        if !undeclaredDependencies.isEmpty {
            let sourcePath = item.compilationItem.relativeProjectPath

            let message = "Found imports from undeclared dependencies in \(sourcePath): [\(undeclaredDependencies.joined(separator: ","))]. Please update the '\(thisModule.name)' module.yaml and specify the missing dependencies."
            throw CompilerError(message)
        }
    }

    private func check(item: IntermediateTypeScriptItem, compileSequence: CompilationSequence) -> Promise<CompilationItem> {
        let logger = self.logger
        logger.debug(">> Checking TypeScript file \(item.src.compilationPath)")

        return typeScriptCompiler.checkErrors(filePath: item.src.compilationPath, compileSequence: compileSequence)
            .then { result in
                logger.debug("<< Checked TypeScript file \(item.src.compilationPath) in \(Int(result.timeTakenMs))ms")
            return Promise(data: item.compilationItem)
        }
        .catch { (error) -> Promise<CompilationItem> in
            logger.error("<< TypeScript file \(item.src.compilationPath) failed to check: \(error.legibleLocalizedDescription)")
            return Promise(data: item.compilationItem.with(error: error))
        }
    }

    private static func isTestFile(item: IntermediateTypeScriptItem) -> Bool {
        let filename = item.src.compilationPath.lastPathComponent()
        return filename.contains(".spec.")
    }

    private func compile(item: IntermediateTypeScriptItem, compileSequence: CompilationSequence) -> Promise<[CompilationItem]> {
        let logger = self.logger
        logger.debug(">> Compiling TypeScript file \(item.src.compilationPath)")
        return typeScriptCompiler.compile(filePath: item.src.compilationPath, outputDirURL: outputRootURL, compileSequence: compileSequence)
        .then { (file) -> [CompilationItem] in
            logger.debug("<< Compiled TypeScript file \(item.src.compilationPath)")

            let outputTarget: OutputTarget = TypeScriptCompilerManager.isTestFile(item: item) ? .debug : item.compilationItem.outputTarget
            var output = [item.compilationItem.with(newKind: .javaScript(file.jsFile), newPlatform: nil, newOutputTarget: outputTarget)]

            if let declarationFile = file.declarationFile {
                output.append(item.compilationItem.with(newKind: .finalFile(declarationFile)))

                // Output declaration files to an additional location
                // (but only for Web)
                let fileName = declarationFile.outputURL.lastPathComponent
                let outputURL = URL(fileURLWithPath: file.jsFile.relativePath)
                    .deletingLastPathComponent()
                    .appendingPathComponent(fileName)
                let webDeclaration = JavaScriptFile(file: declarationFile.file, relativePath: outputURL.relativePath)
                output.append(item.compilationItem.with(newKind: .declaration(webDeclaration), newPlatform: .web, newOutputTarget: outputTarget))

            }

            return output
        }.catch { (error) -> Promise<[CompilationItem]> in
            logger.error("<< TypeScript file \(item.src.compilationPath) failed to compile: \(error.legibleLocalizedDescription)")
            return Promise(data: [item.compilationItem.with(error: error)])
        }
    }

    private static func generateTSNativeSource(bundleInfo: CompilationItem.BundleInfo, inputNativeSources: [NativeSource]) -> [CompilationItem] {
        let outputNativeSourceFile: File
        if inputNativeSources.isEmpty {
            outputNativeSourceFile = .string("")
        } else if inputNativeSources.count == 1 {
            outputNativeSourceFile = inputNativeSources[0].file
        } else {
            outputNativeSourceFile = .lazyData(block: {
                var data = Data()

                for inputNativeSource in inputNativeSources {
                    data.append(contentsOf: try inputNativeSource.file.readData())
                }

                return data
            })
        }

        let filename = "\(bundleInfo.name)_native.c"

        let sourceURL = bundleInfo.baseDir
        let item = CompilationItem(sourceURL: sourceURL,
                               relativeProjectPath: bundleInfo.relativeProjectPath(forItemPath: filename),
                               kind: .nativeSource(NativeSource(relativePath: nil,
                                                                filename: filename,
                                                                file: outputNativeSourceFile,
                                                                groupingIdentifier: filename,
                                                                groupingPriority: 0)),
                               bundleInfo: bundleInfo,
                               platform: nil,
                               outputTarget: .all)

        return [item.with(newPlatform: .android), item.with(newPlatform: .ios)]
    }

    private func compileNative(bundleInfo: CompilationItem.BundleInfo, compilationPaths: [String], compileSequence: CompilationSequence) -> Promise<[CompilationItem]> {
        logger.debug(">> Compiling TypeScript files into native for module \(bundleInfo.name)")
        return typeScriptCompiler.compileNative(filePaths: compilationPaths.sorted(), compileSequence: compileSequence).then { nativeSources in
            return TypeScriptCompilerManager.generateTSNativeSource(bundleInfo: bundleInfo, inputNativeSources: nativeSources)
        }
    }

    private func getTypeScriptItemSrc(forItem item: CompilationItem) -> TypeScriptItemSrc {
        var sourceURLToUse = item.sourceURL
        if case let .typeScript(_, sourceURL) = item.kind {
            sourceURLToUse = sourceURL
        }

        let compilationPath = item.relativeProjectPath

        return TypeScriptItemSrc(compilationPath: compilationPath, sourceURL: sourceURLToUse)
    }

    private func shouldRegister(filePath: String, compilationSequence: CompilationSequence) -> Bool {
        var shouldRegister = false

        _ = writerCache.getOrUpdate(key: filePath, compileSequence: compilationSequence, onUpdate: { _, _ in
            shouldRegister = true
            return true
        })

        return shouldRegister
    }

    private func register(src: TypeScriptItemSrc, file: File, compilationSequence: CompilationSequence) -> Promise<Void>? {
        guard shouldRegister(filePath: src.compilationPath, compilationSequence: compilationSequence) else {
            return nil
        }

        return self.typeScriptCompiler.register(file: file, filePath: src.compilationPath)
    }

    private func makeTsConfigFileDataWithSkipLibCheck(tsConfigFileContent: String) throws -> File {
        let tsConfigFileData = tsConfigFileContent.strippingSingleLineComments

        var json = try JSONSerialization.jsonObject(with: try tsConfigFileData.utf8Data()) as? [String: Any]
        if json == nil {
            json = [:]
        }

        var compilerOptions = json!["compilerOptions"] as? [String: Any] ?? [:]
        compilerOptions["skipLibCheck"] = true

        json!["compilerOptions"] = compilerOptions

        let updatedTsConfigFileData = try JSONSerialization.data(withJSONObject: json as Any)

        return .data(updatedTsConfigFileData)
    }

    private func registerTSConfig(src: TypeScriptItemSrc, tsConfigFile: File, bundleInfo: CompilationItem.BundleInfo, compilationSequence: CompilationSequence) -> Promise<Void>? {
        if onlyCompileTypeScriptForModules.isEmpty || onlyCompileTypeScriptForModules.contains(bundleInfo.name) {
            return register(src: src, file: tsConfigFile, compilationSequence: compilationSequence)
        } else {
            guard shouldRegister(filePath: src.compilationPath, compilationSequence: compilationSequence) else {
                return nil
            }

            do {
                // Insert skipLibCheck for tsconfig files of modules we are not compiling
                let tsConfigFileContent = try tsConfigFile.readString()
                let updatedTsConfigFile = try makeTsConfigFileDataWithSkipLibCheck(tsConfigFileContent: tsConfigFileContent)
                return typeScriptCompiler.register(file: updatedTsConfigFile, filePath: src.compilationPath)
            } catch {
                return Promise(error: error)
            }
        }
    }

    private func canonicalizeDumpSymbolsResponse(_ response: TS.DumpSymbolsWithCommentsResponseBody) throws -> TS.DumpSymbolsWithCommentsResponseBody {
        let prefix = "/"
        let updatedReferences: [TS.AST.TypeReference] = response.references.map {
            guard $0.fileName.hasPrefix(prefix) else {
                return $0
            }

            let canonicalizedFileName = String($0.fileName.dropFirst(prefix.count))
            return TS.AST.TypeReference(name: $0.name, fileName: canonicalizedFileName)
        }

        var responseCopy = response
        responseCopy.references = updatedReferences
        return responseCopy
    }

    func dumpAllSymbols(typescriptItems: [TypeScriptItem]) throws -> DumpAllSymbolsResult {
        let promises: [Promise<TypedScriptItemAndSymbols?>] = typescriptItems.map { typeScriptItem in
            if !onlyCompileTypeScriptForModules.isEmpty && !typeScriptItem.item.bundleInfo.isRoot && !onlyCompileTypeScriptForModules.contains(typeScriptItem.item.bundleInfo.name) {
                return Promise(data: nil)
            } else {
                logger.verbose("Dumping symbols for \(typeScriptItem.src.compilationPath)")
                return self.typeScriptCompiler.dumpSymbolsWithComments(filePath: typeScriptItem.src.compilationPath).then { dumpSymbolsWithCommentsResult in
                    return TypedScriptItemAndSymbols(typeScriptItem: typeScriptItem, dumpedSymbols: try self.canonicalizeDumpSymbolsResponse(dumpSymbolsWithCommentsResult), restoredFromDisk: false)
                }
            }
        }

        // Collect successful results, log failures but don't fail the entire operation
        let typeScriptItemsAndSymbols = promises.compactMap { promise -> TypedScriptItemAndSymbols? in
            do {
                return try promise.waitForData()
            } catch {
                // Log the error but continue processing other modules
                logger.warn("Failed to dump symbols: \(error.legibleLocalizedDescription)")
                return nil
            }
        }

        let result = DumpAllSymbolsResult(typeScriptItemsAndSymbols: typeScriptItemsAndSymbols)
        return result
    }
}