//
//  ImageResourcesProcessor.swift
//  Compiler
//
//  Created by Simon Corsin on 7/24/18.
//  Copyright © 2018 Snap Inc. All rights reserved.
//

import Foundation

// [.imageAsset] -> [.imageResource]
class ImageResourcesProcessor: CompilationProcessor {

    var description: String {
        return "Processing Resources"
    }

    private let logger: ILogger
    private let fileManager: ValdiFileManager
    private let alwaysUseVariantAgnosticFilenames: Bool
    private let diskCacheProvider: DiskCacheProvider
    private let projectConfig: ValdiProjectConfig
    private let compilerConfig: CompilerConfig
    private let imageConverter: ImageConverter
    private var cacheByExtension: [String: DiskCache] = [:]
    private let lock = DispatchSemaphore.newLock()
    private var imageConverterDependencies: [String: String]?
    private let imageVariantsFilter: ImageVariantsFilter?

    init(logger: ILogger, 
         fileManager: ValdiFileManager,
         diskCacheProvider: DiskCacheProvider,
         projectConfig: ValdiProjectConfig,
         compilerConfig: CompilerConfig,
         imageToolbox: ImageToolbox,
         imageVariantsFilter: ImageVariantsFilter?,
         alwaysUseVariantAgnosticFilenames: Bool) throws {
        self.logger = logger
        self.fileManager = fileManager
        self.diskCacheProvider = diskCacheProvider
        self.projectConfig = projectConfig
        self.compilerConfig = compilerConfig
        self.imageVariantsFilter = imageVariantsFilter
        self.alwaysUseVariantAgnosticFilenames = alwaysUseVariantAgnosticFilenames
        self.imageConverter = ImageConverter(logger: logger, fileManager: fileManager, projectConfig: projectConfig, imageToolbox: imageToolbox)

        // Warm up the disk caches we know we will use
        FileExtensions.exportedImages.forEach { imageExt in
            DispatchQueue.global().async {
                _ = try? self.getDiskCache(forExtension: imageExt)
            }
        }
    }

    private func lockFreeGetOrCreateImageConverterDependenciesVersions() throws -> [String: String] {
        if let imageConverterDependencies {
            return imageConverterDependencies
        }

        let imageConverterDependencies = try imageConverter.dependenciesVersions()
        self.imageConverterDependencies = imageConverterDependencies
        return imageConverterDependencies
    }

    private func getDiskCache(forExtension ext: String) throws -> DiskCache? {
        return try lock.lock {
            if let diskCache = cacheByExtension[ext] {
                return diskCache
            }

            guard diskCacheProvider.isEnabled() else { return nil }

            guard let diskCache = diskCacheProvider.newCache(cacheName: "image_processing/\(ext)", outputExtension: ext, metadata: try lockFreeGetOrCreateImageConverterDependenciesVersions()) else {
                return nil
            }

            self.cacheByExtension[ext] = diskCache

            return diskCache
        }
    }

    private func getGeneratedImageFromCache(cacheKey: String, inputImageData: Data, cache: DiskCache) -> Data? {
        guard let outputData = cache.getOutput(item: cacheKey, inputData: inputImageData) else {
            return nil
        }

        return outputData
    }

    private func generateImage(fromImageAssetVariant: ImageAssetVariant, sourceItemProjectPath: String, inputImageURL: URL, inputImageData: Data, variantSpecs: ImageVariantSpecs) throws -> ImageAssetVariant {
        let diskCache = try getDiskCache(forExtension: variantSpecs.fileExtension)

        let cacheKey = "\(variantSpecs.identifier)/\(sourceItemProjectPath)"

        let conversionInfo = imageConverter.getConversionInfo(sourceImage: fromImageAssetVariant, targetVariantSpecs: variantSpecs)
        let outputImageInfo = ImageInfo(size: conversionInfo.outputSize)

        if let diskCache = diskCache {
            if let cachedImage = getGeneratedImageFromCache(cacheKey: cacheKey, inputImageData: inputImageData, cache: diskCache) {
                logger.verbose("-- Using cached generated image from \(sourceItemProjectPath) with variant \(variantSpecs.identifier)")

                return ImageAssetVariant(imageInfo: outputImageInfo, file: .data(cachedImage), variantSpecs: variantSpecs)
            }
        }

        logger.debug("-- Generating image from \(sourceItemProjectPath) into variant \(variantSpecs.identifier)")

        let outputFileURL = URL.randomFileURL(extension: variantSpecs.fileExtension)
        defer {
            _ = try? FileManager.default.removeItem(at: outputFileURL)
        }

        let resultImageInfo = try imageConverter.convert(imageInfo: fromImageAssetVariant.imageInfo, filePath: inputImageURL.path, outputFileURL: outputFileURL, conversionInfo: conversionInfo)

        let outputData = try File.url(outputFileURL).readData()

        try diskCache?.setOutput(item: cacheKey, inputData: inputImageData, outputData: outputData)

        return ImageAssetVariant(imageInfo: resultImageInfo, file: .data(outputData), variantSpecs: variantSpecs)
    }

    private func shouldInclude(variantSpecs: ImageVariantSpecs) -> Bool {
        guard let platform = variantSpecs.platform else {
            return true
        }

        // Gate on which platforms are enabled before consulting the variants filter.
        switch platform {
        case .android where !compilerConfig.outputForAndroid: return false
        case .ios where !compilerConfig.outputForIOS: return false
        case .web where !compilerConfig.outputForWeb: return false
        default: break
        }

        guard let imageVariantsFilter else {
            return true
        }

        return imageVariantsFilter.shouldInclude(platform: platform, scale: variantSpecs.scale)
    }

    private func findMissingVariants(currentVariants: [ImageAssetVariant]) -> [ImageVariantSpecs] {
        let existingVariants = Set(currentVariants.map { $0.variantSpecs.identifier })
        let targetSpecs = ImageVariantResolver.exportedVariantSpecs(
            android: compilerConfig.outputForAndroid,
            ios: compilerConfig.outputForIOS,
            web: compilerConfig.outputForWeb
        )
        return targetSpecs.filter { shouldInclude(variantSpecs: $0) && !existingVariants.contains($0.identifier) }
    }

    private func makeImageResourceItem(fromCompilationItem: CompilationItem, imageAsset: ImageAsset) -> [CompilationItem] {
        let variants = imageAsset.variants.filter { $0.variantSpecs.platform != nil }

        var items = [CompilationItem]()
        if fromCompilationItem.bundleInfo.downloadableAssets || alwaysUseVariantAgnosticFilenames {
            for variant in variants {
                let outputFilename = "\(imageAsset.identifier.assetName).\(variant.variantSpecs.fileExtension)"
                let imageResource = ImageResource(outputFilename: outputFilename, file: variant.file, imageScale: variant.variantSpecs.scale, isRemote: true)

                items.append(fromCompilationItem.with(newKind: .processedResourceImage(imageResource), newPlatform: variant.variantSpecs.platform))
            }
        } else {
            for variant in variants {
                let resolvedFilename: String
                if variant.variantSpecs.platform == .android {
                    // On Android, we prefix by the module_name for uniqueness, since the asset won't be
                    // stored in a unique .bundle
                    resolvedFilename = "\(fromCompilationItem.bundleInfo.name)_\(imageAsset.identifier.assetName)"
                } else {
                    resolvedFilename = imageAsset.identifier.assetName
                }

                let outputFilename = variant.variantSpecs.resolveFilename(assetName: resolvedFilename)
                let imageResource = ImageResource(outputFilename: outputFilename, file: variant.file, imageScale: variant.variantSpecs.scale, isRemote: false)

                items.append(fromCompilationItem.with(newKind: .processedResourceImage(imageResource), newPlatform: variant.variantSpecs.platform))
            }
        }

        return items
    }

    private func processImageAsset(item: SelectedItem<ImageAsset>) -> [CompilationItem] {
        do {
            guard !item.data.variants.isEmpty else {
                throw CompilerError("No variants available!")
            }

            // Step 1: We find the image variants which we don't currently have
            let missingVariants = findMissingVariants(currentVariants: item.data.variants)

            // Step 2: Find the variant we can use for resizing the images
            let bestVariant = item.data.bestVariant!

            let inputImage = bestVariant.file

            return try inputImage.withURL { url in
                // Step 3: Generate all the missing variants

                let generatedImages = try missingVariants.map { try generateImage(fromImageAssetVariant: bestVariant, sourceItemProjectPath: item.item.relativeProjectPath, inputImageURL: url, inputImageData: try inputImage.readData(), variantSpecs: $0) }

                let allVariants = (item.data.variants + generatedImages).filter { variant in
                    return shouldInclude(variantSpecs: variant.variantSpecs)
                }

                let newImageAsset = ImageAsset(identifier: item.data.identifier, size: item.data.size, variants: allVariants)
                return makeImageResourceItem(fromCompilationItem: item.item, imageAsset: newImageAsset)
            }
        } catch let error {
            let errorMessage = "Failed to process image asset from \(item.item.relativeProjectPath): \(error.legibleLocalizedDescription)"
            logger.error(errorMessage)
            return [item.item.with(error: CompilerError(errorMessage))]
        }
    }

    func process(items: CompilationItems) throws -> CompilationItems {
        return items.select { item in
            if !compilerConfig.onlyProcessResourcesForModules.isEmpty && !compilerConfig.onlyProcessResourcesForModules.contains(item.bundleInfo.name) {
                return nil
            }

            if case .imageAsset(let asset) = item.kind {
                return asset
            }
            return nil
        }.transformEachConcurrently(processImageAsset)
    }
}
