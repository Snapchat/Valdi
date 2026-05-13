//
//  BundleAssetsProcessor.swift
//  Compiler
//
//  Created by Brandon Francis on 7/25/18.
//  Copyright © 2018 Snap Inc. All rights reserved.
//

import Foundation

// [.finalFile] -> [.finalFile]
class BundleResourcesProcessor: CompilationProcessor {

    enum OutputMode {
        /**
         Package sources and assets and upload them to the
         moduleUploadBaseURL endpoint set in the config
         */
        case uploadedArtifacts
        /**
         Package assets in an assetpackage format, sources
         are left untouched.
         */
        case assetPackage

        /**
         Package sources and assets into .valdimodule
         */
        case bundledAssetPackage
    }

    enum DownloadableArtifactsProcessingMode {
        /**
         Build modules and verify that the signatures found on-disk match.
         Will fail compilations if a matching signature can't be found, since this
         means that the developer has made a change that hasn't been uploaded to
         the Bolt proxy.
         Normally used when the Valdi compiler is driven with Bazel.
         */
        case verifySignatures
        /**
         Build modules, ensure they have already been uploaded to Bolt,
         generate signature files and write them to disk.
         */
        case uploadIfNeeded
    }

    private struct ModuleAndPlatform: Hashable {
        let moduleInfo: CompilationItem.BundleInfo
        let platform: Platform
    }

    private struct DownloadableAssetsArtifact {
        let deviceDensity: Double
        let files: [SelectedItem<FinalFile>]
    }

    private let logger: ILogger
    private let fileManager: ValdiFileManager
    private let projectConfig: ValdiProjectConfig
    private let compilerConfig: CompilerConfig
    private let outputMode: BundleResourcesProcessor.OutputMode
    private let downloadableArtifactsProcessingMode: DownloadableArtifactsProcessingMode
    private let artifactUploader: ArtifactUploader?
    private let diskCache: DiskCache?
    private let downloadableArtifactCache: DiskCache?
    private let rootBundle: CompilationItem.BundleInfo
    private let companionExecutable: CompanionExecutable
    private let writeDownloadableArtifactsManifest: Bool
    private var pendingPreparedArtifact = Synchronized(data: [String: File]())

    init(logger: ILogger, 
         fileManager: ValdiFileManager,
         diskCacheProvider: DiskCacheProvider,
         projectConfig: ValdiProjectConfig,
         compilerConfig: CompilerConfig,
         maxUploadConcurrentRequests: Int,
         outputMode: OutputMode,
         downloadableArtifactsProcessingMode: DownloadableArtifactsProcessingMode,
         writeDownloadableArtifactsManifest: Bool,
         rootBundle: CompilationItem.BundleInfo,
         companionExecutable: CompanionExecutable) {
        self.logger = logger
        self.fileManager = fileManager
        self.projectConfig = projectConfig
        self.compilerConfig = compilerConfig
        self.outputMode = outputMode
        self.downloadableArtifactsProcessingMode = downloadableArtifactsProcessingMode
        self.writeDownloadableArtifactsManifest = writeDownloadableArtifactsManifest
        self.rootBundle = rootBundle
        self.companionExecutable = companionExecutable

        if let moduleUploadBaseURL = projectConfig.moduleUploadBaseURL {
            artifactUploader = ArtifactUploader(moduleBaseUploadURL: moduleUploadBaseURL,
                                                companionExecutable: companionExecutable)
        } else {
            artifactUploader = nil
        }

        self.diskCache = diskCacheProvider.newCache(cacheName: "modules_build",
                                   outputExtension: FileExtensions.valdiModule,
                                   metadata: [:])

        self.downloadableArtifactCache = diskCacheProvider.newCache(cacheName: "downloadable_artifacts", outputExtension: "bin", metadata: [:])
    }

    var description: String {
        return "Bundling resources"
    }

    private func getZipItem(forItem item: CompilationItem, finalFile: FinalFile) -> ZippableItem {
        let itemPath: String
        if item.relativeBundleURL.pathComponents.count > 1 {
            // Use the filename from the output url, appended to the relative source url
            itemPath = item.relativeBundleURL.deletingLastPathComponent().appendingPathComponent(finalFile.outputURL.lastPathComponent).path
        } else {
            itemPath = finalFile.outputURL.lastPathComponent
        }

        return ZippableItem(file: finalFile.file, path: itemPath)
    }

    /**
     * Bundles the files in the .valdimodule format.
     *
     * Parameters:
     * - moduleName: Only used for caching/logging. The name of the module.
     * - zippableItems/finalFiles: The items (file + path) to be bundled.
     * - platform: Only used for caching. Platform (android/ios).
     * - target: Only used for caching. Build target (debug/release).
     *
     * Returns:
     * - The .valdimodule file contents as Data().
     */
    private func buildModule(moduleName: String, finalFiles: [SelectedItem<FinalFile>], platform: Platform, target: OutputTarget) throws -> Data {
        let zippableItems = finalFiles.map { getZipItem(forItem: $0.item, finalFile: $0.data) }
        return try buildModule(moduleName: moduleName, zippableItems: zippableItems, platform: platform, target: target)
    }

    private func buildModule(moduleName: String, zippableItems: [ZippableItem], platform: Platform, target: OutputTarget) throws -> Data {
        do {
            let moduleBuilder = ValdiModuleBuilder(items: zippableItems)
            moduleBuilder.compress = false

            let data = try moduleBuilder.build()

            let cacheKey = getCacheKey(moduleName)

            if let cachedCompressed = diskCache?.getOutput(item: cacheKey, platform: platform, target: target, inputData: data) {
                return cachedCompressed
            }

            let compressedData = try ValdiModuleBuilder.compress(data: data)
            try diskCache?.setOutput(item: cacheKey, platform: platform, target: target, inputData: data, outputData: compressedData)

            return compressedData
        } catch let error {
            let errorMessage = "Failed to create valdi module '\(moduleName)': \(error.legibleLocalizedDescription)"
            logger.error(errorMessage)
            throw CompilerError(errorMessage)
        }
    }

    private func makeItem(forValdiModuleData moduleData: Data, moduleInfo: CompilationItem.BundleInfo, platform: Platform, target: OutputTarget, outputDirectories: OutDirectories) -> CompilationItem {
        let fileName = moduleInfo.name

        let outputDirectoryURL: URL = outputDirectories.assetsURL

        let outputURL = outputDirectoryURL.appendingPathComponent("\(fileName).\(FileExtensions.valdiModule)", isDirectory: false)
        let finalFile = FinalFile(outputURL: outputURL, file: .data(moduleData), platform: platform, kind: .unknown)

        // Source url doesn't matter here, just give it something
        return CompilationItem(sourceURL: moduleInfo.baseDir, relativeProjectPath: nil, kind: .finalFile(finalFile), bundleInfo: moduleInfo, platform: platform, outputTarget: target)
    }

    struct ResolveFilesResult {
        let sources: [SelectedItem<FinalFile>]
        let assets: [SelectedItem<FinalFile>]
        // Items that should not be explicitly handled, they should be just saved
        // on disk.
        let passthroughItems: [SelectedItem<FinalFile>]
    }

    private func resolveFiles(items: [SelectedItem<FinalFile>]) -> ResolveFilesResult {
        var sources = [SelectedItem<FinalFile>]()
        var assets = [SelectedItem<FinalFile>]()
        var passthroughItems = [SelectedItem<FinalFile>]()

        for item in items {
            if item.data.platform == .web {
                passthroughItems.append(item)
            }
            switch item.data.kind {
            case .image:
                assets.append(item)
            case .compiledSource:
                sources.append(item)
            case .unknown, .nativeSource, .assetsPackage, .dependencyInjectionData:
                // Append if it wasn't already added
                if item.data.platform != .web {
                    passthroughItems.append(item)
                }
            }
        }

        return ResolveFilesResult(sources: sources, assets: assets, passthroughItems: passthroughItems)
    }

    private func getCacheKey(_ artifactName: String) -> String {
        return artifactName + ".valdiarchive"
    }

    private func getDownloadableModuleArtifactFromCache(artifactName: String, moduleData: Data) -> Valdi_DownloadableModuleArtifact? {
        guard let outputData = downloadableArtifactCache?.getOutput(item: getCacheKey(artifactName), inputData: moduleData) else {
            return nil
        }
        return try? Valdi_DownloadableModuleArtifact(serializedData: outputData)
    }

    private func expectedSignatureFileURL(moduleInfo: CompilationItem.BundleInfo, platform: Platform) -> URL {
        let expectedSignatureFileURL = moduleInfo.baseDir.appendingPathComponent(platform.rawValue)
            .appendingPathExtension(FileExtensions.downloadableArtifacts)

        return expectedSignatureFileURL
    }
    
    private func buildAndUpload(moduleInfo: CompilationItem.BundleInfo, artifactUploader: ArtifactUploader, artifactName: String, moduleData: Data, sha256Digest: String, downloadableArtifactSignatures: Valdi_DownloadableModuleArtifactSignatures) -> Promise<Valdi_DownloadableModuleArtifact> {
        do {
            guard let foundSignature = downloadableArtifactSignatures.artifacts.first(where: { $0.sha256Digest == Data(hexString: sha256Digest) }) else {
                throw CompilerError("Downloadable artifact signature for '\(artifactName)' not found.")
            }
            return Promise(data: foundSignature)
        } catch {
            let errorMessage = "Failed to verify that downloadable module \(artifactName) has already been uploaded (sha256: \(sha256Digest)). Underlying error: \(error.legibleLocalizedDescription)"
            switch downloadableArtifactsProcessingMode {
            case .verifySignatures:
                logger.error(errorMessage)
                return Promise(error: CompilerError(errorMessage))
            case .uploadIfNeeded:
                // continue execution to create the artifact and upload it
                break
            }
        }
        let logger = self.logger
        let maxAttempts = 3
        var attempt = 0
        var retryDelay = 500.0
        let startTime = Date()
        let requestId = String(UUID().uuidString.prefix(8))
        while (true) {
            let promise = uploadArtifact(artifactUploader: artifactUploader, artifactName: artifactName, moduleData: moduleData, sha256Digest: sha256Digest)
            let result = promise.waitForResult()
            
            switch result {
            case .success(let artifact):
                return Promise(data: artifact)
            case .failure(let error):
                let elapsedTime = Date().timeIntervalSince(startTime)
                
                logger.warn("[\(requestId)] buildAndUpload attempt \(attempt + 1)/\(maxAttempts) failed for '\(artifactName)'")
                logger.warn("[\(requestId)]   Module: \(moduleInfo.name)")
                logger.warn("[\(requestId)]   Artifact size: \(moduleData.count) bytes")
                logger.warn("[\(requestId)]   SHA256: \(sha256Digest)")
                logger.warn("[\(requestId)]   Error: \(error.legibleLocalizedDescription)")
                logger.warn("[\(requestId)]   Error type: \(type(of: error))")
                logger.warn("[\(requestId)]   Time since start: \(String(format: "%.2f", elapsedTime))s")
                
                if ((attempt + 1) >= maxAttempts) {
                    logger.error("[\(requestId)] buildAndUpload exhausted all \(maxAttempts) attempts for '\(artifactName)' after \(String(format: "%.2f", elapsedTime))s")
                    return Promise(error: error)
                }

                logger.info("[\(requestId)] buildAndUpload waiting \(retryDelay)ms before retry \(attempt + 2)")
                Thread.sleep(forTimeInterval: (retryDelay / 1000.0))
                retryDelay *= 2
                attempt += 1
            }
        }
    }

    private func uploadArtifact(artifactUploader: ArtifactUploader, artifactName: String, moduleData: Data, sha256Digest: String) -> Promise<Valdi_DownloadableModuleArtifact> {
        let artifactInfoPromise: Promise<ArtifactInfo>
        if let _ = self.projectConfig.preparedUploadArtifactOutput {
            artifactInfoPromise = artifactUploader.appendToPreparedArtifact(artifactName: artifactName, artifactData: moduleData, sha256: sha256Digest)
        } else {
            artifactInfoPromise = artifactUploader.upload(artifactName: artifactName, artifactData: moduleData, sha256: sha256Digest)
        }
        let promise: Promise<Valdi_DownloadableModuleArtifact> = artifactInfoPromise.then { (artifactInfo) -> Valdi_DownloadableModuleArtifact in
            var artifact = Valdi_DownloadableModuleArtifact()
            artifact.url = artifactInfo.url.absoluteString
            guard let sha256Digest = Data(hexString: artifactInfo.sha256Digest) else {
                throw CompilerError("Couldn't parse SHA256 digest: \(artifactInfo.sha256Digest)")
            }
            artifact.sha256Digest = sha256Digest

            return artifact
        }
        return promise
    }

    private func makeDownloadableAssetsArtifacts(assets: [SelectedItem<FinalFile>]) -> [DownloadableAssetsArtifact] {
        // files without density will be added to every artifact
        var filesWithoutDensity = [SelectedItem<FinalFile>]()
        var filesByDensity = [Double: [SelectedItem<FinalFile>]]()

        for asset in assets {
            switch asset.data.kind {
            case .image(let scale, _):
                if let imageScale = scale, imageScale != 0.0 {
                    filesByDensity[imageScale, default: []].append(asset)
                } else {
                    filesWithoutDensity.append(asset)
                }
            default:
                fatalError("finalFile assets should only be of kind image")
            }
        }

        // We create one artifact with imageScale 0, which will end up containing all the files without density specified only.
        filesByDensity[0] = []

        for key in filesByDensity.keys {
            filesByDensity[key, default: []].append(contentsOf: filesWithoutDensity)
        }

        return filesByDensity
            .map { DownloadableAssetsArtifact(deviceDensity: $0.key, files: $0.value) }
            .filter { !$0.files.isEmpty }
    }

    /**
     Process a local module, which will be stored locally on disk.

     - A .valdimodule is created which contains all sources.
     - All assets are stored locally in the assets or res folder.
     */
    private func processLocalModule(moduleInfo: CompilationItem.BundleInfo,
                                    platform: Platform,
                                    target: OutputTarget,
                                    sources: [ZippableItem],
                                    assets: [SelectedItem<FinalFile>],
                                    outputDirectories: OutDirectories) throws -> [CompilationItem] {
        var out = [CompilationItem]()

        if !sources.isEmpty {
            let valdiModule = try buildModule(moduleName: moduleInfo.name, zippableItems: sources, platform: platform, target: target)
            out.append(makeItem(forValdiModuleData: valdiModule, moduleInfo: moduleInfo, platform: platform, target: target, outputDirectories: outputDirectories))
        }

        for asset in assets {
            if case let .image(_, isRemote) = asset.data.kind, isRemote {
                // Ignore emitted images meant to be remote
                continue
            }

            out.append(asset.item)
        }

        return out
    }

    private func makeModuleName(moduleInfo: CompilationItem.BundleInfo, platform: Platform, isDebug: Bool) -> String {
        return "\(moduleInfo.name)-\(platform)-\(isDebug ? "debug" : "release")"
    }

    /**
     Process a downloadable module, which will be hosted remotely on a server.

     - A .valdimodule is created with all sources and will be hosted remotely
     - .valdiassets files are created with all assets, for each image density and will be hosted remotely.
     - An additional .valdimodule is created and will contain a single file, the downloadable manifest,
     and will be stored locally. The manifest is used by the app to know how to fetch the remote module/assets.
     */
    private func processDownloadableModule(moduleInfo: CompilationItem.BundleInfo, platform: Platform, target: OutputTarget, sources: [SelectedItem<FinalFile>], assets: [SelectedItem<FinalFile>], previousDownloadableArtifactSignatures:  [DownloadableArtifactSignatures]) throws -> ZippableItem {
        guard let artifactUploader = artifactUploader else {
            throw CompilerError("Cannot process downloadable module '\(moduleInfo.name)': The 'module_upload_base_url' setting was not configured in the project yaml file. Please set it to a valid endpoint that can be used to retrieve and upload artifacts, or make this module non downloadable by removing the 'downloadable: true' in its module.yaml")
        }

        let dependencies = moduleInfo.dependencies.filter { !$0.isRoot }.map { $0.name }

        let hasDownloadableSources = !sources.isEmpty && moduleInfo.downloadableSources
        let hasDownloadableAssets = !assets.isEmpty && moduleInfo.downloadableAssets

        let manifest = Synchronized(data: Valdi_DownloadableModuleManifest())
        manifest.data { (manifest) in
            manifest.name = moduleInfo.name
            manifest.dependencies = dependencies
        }

        var pendingPromises = [Promise<Void>]()
        let newArtifacts: Synchronized<[Valdi_DownloadableModuleArtifact]> = Synchronized(data: [])
        let platformDownloadableArtifactSignatures = previousDownloadableArtifactSignatures.first(where: { $0.key == platform.rawValue })
        var downloadableArtifactSignatures = Valdi_DownloadableModuleArtifactSignatures()

        if (hasDownloadableSources || hasDownloadableAssets) {
            switch (downloadableArtifactsProcessingMode) {
            case .uploadIfNeeded:
                break
            case .verifySignatures:
                do {
                    if (platformDownloadableArtifactSignatures == nil) {
                        throw CompilerError("Cannot verify downloadable module '\(moduleInfo.name)': No downloadable artifact signatures were found")
                    }
                    let fileData = try platformDownloadableArtifactSignatures!.file.readData()
                    downloadableArtifactSignatures = try Valdi_DownloadableModuleArtifactSignatures(serializedData: fileData)
                } catch {
                    throw CompilerError("Failed to verify that downloadable module \(moduleInfo.name) has already been uploaded")
                }
            }
        }

        if hasDownloadableSources {
            let artifactName = "\(moduleInfo.name)-\(platform)-sources"
            do {
                let moduleData = try buildModule(moduleName: artifactName, finalFiles: sources, platform: platform, target: target)
                let sha256Digest = try moduleData.generateSHA256Digest()

                let promise = buildAndUpload(moduleInfo: moduleInfo,
                                             artifactUploader: artifactUploader,
                                             artifactName: artifactName,
                                             moduleData: moduleData,
                                             sha256Digest: sha256Digest,
                                             downloadableArtifactSignatures: downloadableArtifactSignatures).then { (artifact) -> Void in
                    manifest.data { (manifest) in
                        manifest.artifact = artifact
                    }
                    newArtifacts.data { newArtifacts in
                        newArtifacts.append(artifact)
                    }
                }

                pendingPromises.append(promise)
            } catch let error {
                let errorMessage = "Failed to build artifact '\(artifactName)': \(error.legibleLocalizedDescription)"
                logger.error(errorMessage)
                throw CompilerError(errorMessage)
            }
        }
        
        if hasDownloadableAssets {
            let assetsArtifacts = makeDownloadableAssetsArtifacts(assets: assets)
            for assetsArtifact in assetsArtifacts {
                let artifactName = "\(moduleInfo.name)-\(platform)-assets-\(assetsArtifact.deviceDensity)dp"
                do {
                    let moduleData = try buildModule(moduleName: artifactName, finalFiles: assetsArtifact.files, platform: platform, target: target)
                    let sha256Digest = try moduleData.generateSHA256Digest()

                    let promise = buildAndUpload(moduleInfo: moduleInfo,
                                                 artifactUploader: artifactUploader,
                                                 artifactName: artifactName,
                                                 moduleData: moduleData,
                                                 sha256Digest: sha256Digest,
                                                 downloadableArtifactSignatures: downloadableArtifactSignatures
                                                 ).then { (artifact) -> Void in
                        var assets = Valdi_DownloadableModuleAssets()
                        assets.artifact = artifact
                        assets.deviceDensity = assetsArtifact.deviceDensity

                        manifest.data { (manifest) in
                            manifest.assets.append(assets)
                        }

                        newArtifacts.data { newArtifacts in
                            newArtifacts.append(artifact)
                        }
                    }

                    pendingPromises.append(promise)
                } catch let error {
                    let errorMessage = "Failed to build artifact '\(artifactName)': \(error.legibleLocalizedDescription)"
                    logger.error(errorMessage)
                    throw CompilerError(errorMessage)
                }
            }
        }

        for promise in pendingPromises {
            try promise.waitForData()
        }

        if (hasDownloadableSources || hasDownloadableAssets) {
            switch downloadableArtifactsProcessingMode {
            case .verifySignatures:
                break
            case .uploadIfNeeded:
                if writeDownloadableArtifactsManifest {
                    var artifactsSignatures = Valdi_DownloadableModuleArtifactSignatures()
                    artifactsSignatures.artifacts = newArtifacts.unsafeData.sorted(by: { (obj1: Valdi_DownloadableModuleArtifact, obj2: Valdi_DownloadableModuleArtifact) -> Bool in
                        return obj1.url < obj2.url
                    })
                    let serializedData = try artifactsSignatures.serializedData()
                    let expectedSignatureFileURL = expectedSignatureFileURL(moduleInfo: moduleInfo, platform: platform)

                    if (!artifactsSignatures.artifacts.isEmpty) {
                        try self.fileManager.save(data: serializedData, to: expectedSignatureFileURL)
                    }
                }
                break
            }
        }

        var completedManifest = manifest.unsafeData
        completedManifest.assets.sort(by: { left, right in left.deviceDensity < right.deviceDensity })

        let data = try completedManifest.orderedSerializedData()
        return ZippableItem(file: .data(data), path: Files.downloadManifest)
    }

    private func doBuildAssetPackage(fileName: String,
                                     platform: Platform,
                                     target: OutputTarget,
                                     assets: [SelectedItem<FinalFile>]) throws -> Data {
        let assetsArtifacts = makeDownloadableAssetsArtifacts(assets: assets)

        var items = [ZippableItem]()
        for assetsArtifact in assetsArtifacts {
            var assetItems = [ZippableItem]()
            for file in assetsArtifact.files {
                assetItems.append(getZipItem(forItem: file.item, finalFile: file.data))
            }

            let assetsBuilder = ValdiModuleBuilder(items: assetItems)
            assetsBuilder.compress = false

            let assetsData = try assetsBuilder.build()

            items.append(ZippableItem(file: .data(assetsData), path: "\(assetsArtifact.deviceDensity)"))
        }

        return try buildModule(moduleName: fileName, zippableItems: items, platform: platform, target: target)
    }

    private func buildAssetPackage(moduleInfo: CompilationItem.BundleInfo,
                                   platform: Platform,
                                   target: OutputTarget,
                                   sources: [SelectedItem<FinalFile>],
                                   assets: [SelectedItem<FinalFile>],
                                   outputDirectories: OutDirectories) -> [CompilationItem] {
        var out = sources.map { $0.item }

        guard !assets.isEmpty else { return out }

        do {
            let fileName = "\(moduleInfo.name).\(FileExtensions.assetPackage)"

            let packageData = try doBuildAssetPackage(fileName: fileName, platform: platform, target: target, assets: assets)

            let outputDirectoryURL: URL = outputDirectories.assetsURL
            let outputURL = outputDirectoryURL.appendingPathComponent(fileName, isDirectory: false)

            let finalFile = FinalFile(outputURL: outputURL, file: .data(packageData), platform: platform, kind: .assetsPackage)

            out += [CompilationItem(sourceURL: moduleInfo.baseDir, relativeProjectPath: nil, kind: .finalFile(finalFile), bundleInfo: moduleInfo, platform: platform, outputTarget: target)]
        } catch let error {
            out += assets.map { $0.item.with(error: error) }
        }

        return out
    }

    private func buildModuleAndUpload(moduleInfo: CompilationItem.BundleInfo,
                                      platform: Platform,
                                      target: OutputTarget,
                                      sources: [SelectedItem<FinalFile>],
                                      assets: [SelectedItem<FinalFile>],
                                      appendAssetsToValdiModule: Bool,
                                      outputDirectories: OutDirectories,
                                      downloadableArtifactSignatures: [DownloadableArtifactSignatures]) -> [CompilationItem] {
        do {
            if sources.isEmpty && assets.isEmpty {
                return []
            }

            var moduleItems = [ZippableItem]()

            if moduleInfo.downloadableSources || moduleInfo.downloadableAssets {
                let manifest = try processDownloadableModule(moduleInfo: moduleInfo, platform: platform, target: target, sources: sources, assets: assets, previousDownloadableArtifactSignatures: downloadableArtifactSignatures)
                moduleItems.append(manifest)
            }

            if !moduleInfo.downloadableSources {
                moduleItems += sources.map { getZipItem(forItem: $0.item, finalFile: $0.data) }

                if appendAssetsToValdiModule && !assets.isEmpty && !moduleInfo.downloadableAssets {
                    let fileName = "\(moduleInfo.name).\(FileExtensions.assetPackage)"

                    let assetPackage = try doBuildAssetPackage(fileName: fileName, platform: platform, target: target, assets: assets)
                    moduleItems.append(ZippableItem(file: .data(assetPackage), path: "res.\(FileExtensions.assetPackage)"))
                    return try processLocalModule(moduleInfo: moduleInfo,
                                                  platform: platform,
                                                  target: target,
                                                  sources: moduleItems,
                                                  assets: [],
                                                  outputDirectories: outputDirectories)
                } else {
                    return try processLocalModule(moduleInfo: moduleInfo,
                                                  platform: platform,
                                                  target: target,
                                                  sources: moduleItems,
                                                  assets: assets,
                                                  outputDirectories: outputDirectories)
                }
            } else {
                // Sources are remote, we just store the manifest in the .valdimodule
                let moduleData = try buildModule(moduleName: "\(moduleInfo.name)-manifest", zippableItems: moduleItems, platform: platform, target: target)

                return [makeItem(forValdiModuleData: moduleData, moduleInfo: moduleInfo, platform: platform, target: target, outputDirectories: outputDirectories)]
            }
        } catch let error {
            return (sources + assets).map { $0.item.with(error: error) }
        }
    }
    
    private func processModule(moduleInfo: CompilationItem.BundleInfo, platform: Platform, target: OutputTarget, sources: [SelectedItem<FinalFile>], assets: [SelectedItem<FinalFile>], outputDirectories: OutDirectories, downloadableArtifactSignatures: [DownloadableArtifactSignatures]) -> [CompilationItem] {
        switch outputMode {
        case .uploadedArtifacts:
            return buildModuleAndUpload(moduleInfo: moduleInfo, platform: platform, target: target, sources: sources, assets: assets, appendAssetsToValdiModule: false, outputDirectories: outputDirectories, downloadableArtifactSignatures: downloadableArtifactSignatures)
        case .assetPackage:
            return buildAssetPackage(moduleInfo: moduleInfo, platform: platform, target: target, sources: sources, assets: assets, outputDirectories: outputDirectories)
        case .bundledAssetPackage:
            return buildModuleAndUpload(moduleInfo: moduleInfo, platform: platform, target: target, sources: sources, assets: assets, appendAssetsToValdiModule: true, outputDirectories: outputDirectories, downloadableArtifactSignatures: downloadableArtifactSignatures)
        }
    }
    
    private func generateModules(selectedItems: GroupedItems<ModuleAndPlatform, SelectedItem<FinalFile>>, downloadableArtifactSignatures: [DownloadableArtifactSignatures]) -> [CompilationItem] {
        let moduleInfo = selectedItems.key.moduleInfo
        let platform = selectedItems.key.platform

        logger.verbose("Processing module \(moduleInfo.name) for platform \(platform)")

        let resolvedFiles = resolveFiles(items: selectedItems.items)

        var out = [CompilationItem]()
        out += resolvedFiles.passthroughItems.map { $0.item }

        // Web and CPP doesn't need any packaging
        if platform == .web  || platform == .cpp {
            return out
        }

        let debugSources = resolvedFiles.sources.filter { $0.item.outputTarget.contains(.debug) }
        let releaseSources = resolvedFiles.sources.filter { $0.item.outputTarget.contains(.release) }
        let debugAssets = resolvedFiles.assets.filter { $0.item.outputTarget.contains(.debug) }
        let releaseAssets = resolvedFiles.assets.filter { $0.item.outputTarget.contains(.release) }

        let debugDirectories: OutDirectories? = {
            switch platform {
            case .android:
                return moduleInfo.androidDebugOutputDirectories
            case .ios:
                return moduleInfo.iosDebugOutputDirectories
            case .web:
                return moduleInfo.webDebugOutputDirectories
            default:
                return nil
            }
        }()

        let releaseDirectories: OutDirectories? = {
            switch platform {
            case .android:
                return moduleInfo.androidReleaseOutputDirectories
            case .ios:
                return moduleInfo.iosReleaseOutputDirectories
            case .web:
                return moduleInfo.webReleaseOutputDirectories
            default:
                return nil
            }
        }()

        if let debugDirectories = debugDirectories {
            out += processModule(moduleInfo: moduleInfo, platform: platform, target: .debug, sources: debugSources, assets: debugAssets, outputDirectories: debugDirectories, downloadableArtifactSignatures: downloadableArtifactSignatures)
        }

        if let releaseDirectories = releaseDirectories {
            out += processModule(moduleInfo: moduleInfo, platform: platform, target: .release, sources: releaseSources, assets: releaseAssets, outputDirectories: releaseDirectories, downloadableArtifactSignatures: downloadableArtifactSignatures)
        }

        return out
    }

    private func generatePreparedUploadArtifact(to url: URL, artifactUploader: ArtifactUploader) -> CompilationItem {
        do {
            let preparedUploadArtifact = try artifactUploader.makePreparedUploadArtifact()
            return CompilationItem(relativeProjectPath: url.lastPathComponent,
                                   kind: .finalFile(FinalFile(outputURL: url, file: preparedUploadArtifact, platform: nil, kind: .assetsPackage)),
                                   bundleInfo: self.rootBundle,
                                   platform: nil,
                                   outputTarget: .all)
        } catch let error {
            return CompilationItem(relativeProjectPath: url.lastPathComponent, kind: .error(error, originalItemKind: .unknown(.url(url))), bundleInfo: self.rootBundle, platform: nil, outputTarget: .all)
        }

    }

    func process(items: CompilationItems) throws -> CompilationItems {
        let downloadableArtifactSignatureItems = items.select { item in
            if case let .downloadableArtifactSignatures(sig) = item.kind {
                return sig
            }
            return nil
        }

        let downloadableArtifactSignaturesByModule = downloadableArtifactSignatureItems.selectedItems.groupBy { selectedItem in
            return selectedItem.item.bundleInfo
        }.mapValues { selectedItems in
            return selectedItems.map { $0.data }
        }

        let filteredItems = downloadableArtifactSignatureItems.transformAll { _ in return [] }

        if compilerConfig.regenerateValdiModulesBuildFiles {
            return filteredItems
        }

        var items = filteredItems.select { (item) -> FinalFile? in
            if !compilerConfig.onlyProcessResourcesForModules.isEmpty && !compilerConfig.onlyProcessResourcesForModules.contains(item.bundleInfo.name) /* The following expr breaks module filtering: && !item.shouldOutputToWeb */ {
                return nil
            }

            guard case let .finalFile(finalFile) = item.kind, finalFile.shouldBundle else {
                return nil
            }

            return finalFile
        }.groupBy { (item: SelectedItem<FinalFile>) -> ModuleAndPlatform in
            return ModuleAndPlatform(moduleInfo: item.item.bundleInfo, platform: item.data.platform!)
        }.transformEachConcurrently { groupedItems -> [CompilationItem] in
            return generateModules(selectedItems: groupedItems, downloadableArtifactSignatures: downloadableArtifactSignaturesByModule[groupedItems.key.moduleInfo] ?? [] )
        }

        if let preparedUploadArtifactOutput = self.projectConfig.preparedUploadArtifactOutput, let artifactUploader = artifactUploader {
            items.append(item: generatePreparedUploadArtifact(to: preparedUploadArtifactOutput, artifactUploader: artifactUploader))
        }

        return items
    }

}
