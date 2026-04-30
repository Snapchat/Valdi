//
//  ExplicitImageAssetsProcessor.swift
//

import Foundation

private struct ExplicitImageAssetInputKey: Hashable {
    let moduleName: String
    let relativeProjectPath: String
}

private struct ExplicitIdentifiedImage {
    let variant: ImageVariantSpecs
    let imageInfo: ImageInfo
    let item: SelectedItem<URL>

    var densityIndependentSize: ImageSize {
        return imageInfo.size.scaled(1.0 / variant.scale)
    }
}

// [.rawResource] -> [.imageAsset] using a manifest supplied by the build system.
class ExplicitImageAssetsProcessor: CompilationProcessor {

    var description: String {
        return "Identifying explicit Image assets"
    }

    private let logger: ILogger
    private let imageToolbox: ImageToolbox
    private let compilerConfig: CompilerConfig
    private let manifest: ExplicitImageAssetManifest
    private let inputKeys: Set<ExplicitImageAssetInputKey>
    private let cache: DiskCache?

    private static let maxImageSizeDeviation = 2

    init(logger: ILogger, imageToolbox: ImageToolbox, compilerConfig: CompilerConfig, manifest: ExplicitImageAssetManifest, diskCacheProvider: DiskCacheProvider) throws {
        self.logger = logger
        self.imageToolbox = imageToolbox
        self.compilerConfig = compilerConfig
        self.manifest = manifest

        var inputKeys = Set<ExplicitImageAssetInputKey>()
        for asset in manifest.assets {
            for input in asset.inputs {
                inputKeys.insert(ExplicitImageAssetInputKey(moduleName: asset.moduleName, relativeProjectPath: input.relativeProjectPath))
            }
        }
        self.inputKeys = inputKeys

        if diskCacheProvider.isEnabled() {
            let version = try imageToolbox.getVersion()
            self.cache = diskCacheProvider.newCache(cacheName: "identify_images", outputExtension: "json", metadata: ["image_toolbox_version": version])
        } else {
            self.cache = nil
        }
    }

    private func getImageInfo(item: CompilationItem, inputURL: URL) throws -> ToolboxExecutable.ImageInfoOutput {
        guard let cache else {
            return try imageToolbox.getInfo(inputPath: inputURL.path)
        }

        let inputData = try File.url(inputURL).readData()
        let output = cache.getOutput(compilationItem: item, inputData: inputData)

        if let output, let imageInfoOutput = try? ToolboxExecutable.ImageInfoOutput.fromJSON(output) {
            return imageInfoOutput
        }

        let imageInfoOutput = try imageToolbox.getInfo(inputPath: inputURL.path)
        try cache.setOutput(compilationItem: item, inputData: inputData, outputData: try imageInfoOutput.toJSON())
        return imageInfoOutput
    }

    private func shouldProcess(moduleName: String) -> Bool {
        return compilerConfig.onlyProcessResourcesForModules.isEmpty || compilerConfig.onlyProcessResourcesForModules.contains(moduleName)
    }

    private func variantSpecs(for input: ExplicitImageAssetManifestInput) -> ImageVariantSpecs {
        return ImageVariantSpecs(filenamePattern: input.filenamePattern, scale: input.scale, platform: input.platform)
    }

    private func validateImageVariantsSizes(images: [ExplicitIdentifiedImage], densityIndependentSize: ImageSize) throws {
        var failed = false

        for image in images {
            let currentDPSize = image.densityIndependentSize
            let widthDeviation = abs(densityIndependentSize.width - currentDPSize.width)
            let heightDeviation = abs(densityIndependentSize.height - currentDPSize.height)
            if widthDeviation > ExplicitImageAssetsProcessor.maxImageSizeDeviation || heightDeviation > ExplicitImageAssetsProcessor.maxImageSizeDeviation {
                failed = true
                break
            }
        }

        if failed {
            let detailMessage = images.map {
                let dp = $0.densityIndependentSize
                let pixels = $0.imageInfo.size
                return "\($0.item.item.sourceURL.path): \(dp.width)x\(dp.height)dp (\(pixels.width)x\(pixels.height)px)"
            }.joined(separator: "\n")
            throw CompilerError("Incorrect image sizes, the density independent size of those images varies by more than \(ExplicitImageAssetsProcessor.maxImageSizeDeviation)dp:\n\(detailMessage)\n\nPlease find and remove the incorrect images.")
        }
    }

    private func identifyImage(asset: ExplicitImageAssetManifestAsset, input: ExplicitImageAssetManifestInput, selectedItem: SelectedItem<URL>) -> Promise<ExplicitIdentifiedImage> {
        do {
            if asset.assetName.lowercased() != asset.assetName {
                throw CompilerError("Invalid filename '\(asset.assetName)', image filenames need to be lowercased")
            }

            let imageInfo: ImageInfo
            if compilerConfig.generateTSResFiles {
                imageInfo = ImageInfo(size: ImageSize(width: 0, height: 0))
            } else {
                let info = try getImageInfo(item: selectedItem.item, inputURL: selectedItem.data)
                imageInfo = ImageInfo(size: ImageSize(width: info.width, height: info.height))
            }

            return Promise(data: ExplicitIdentifiedImage(variant: variantSpecs(for: input), imageInfo: imageInfo, item: selectedItem))
        } catch let error {
            return Promise(error: error)
        }
    }

    private func createImageAssetItem(asset: ExplicitImageAssetManifestAsset, images: [ExplicitIdentifiedImage]) -> CompilationItem {
        let sortedImages = images.sorted { left, right in
            if left.variant.scale == right.variant.scale {
                return left.item.data.absoluteString > right.item.data.absoluteString
            } else {
                return left.variant.scale > right.variant.scale
            }
        }
        let highestImage = sortedImages.first!
        let imageSize = highestImage.densityIndependentSize

        do {
            if !compilerConfig.generateTSResFiles {
                try validateImageVariantsSizes(images: sortedImages, densityIndependentSize: imageSize)
            }

            var variants = sortedImages.map {
                ImageAssetVariant(imageInfo: $0.imageInfo, file: .url($0.item.data), variantSpecs: $0.variant)
            }
            variants.sort { left, right in
                return left.variantSpecs.scale < right.variantSpecs.scale
            }

            let identifier = ImageAssetIdentifier(assetName: asset.assetName,
                                                  relativeProjectAssetDirectoryPath: asset.relativeProjectAssetDirectoryPath)
            let imageAsset = ImageAsset(identifier: identifier, size: imageSize, variants: variants)
            return highestImage.item.item.with(newKind: .imageAsset(imageAsset))
        } catch let error {
            logger.error(error.legibleLocalizedDescription)
            return highestImage.item.item.with(error: error)
        }
    }

    private func identifyAssets(items: [SelectedItem<URL>]) throws -> [CompilationItem] {
        var selectedItemByKey = [ExplicitImageAssetInputKey: SelectedItem<URL>]()
        var out = [CompilationItem]()

        for item in items {
            guard shouldProcess(moduleName: item.item.bundleInfo.name) else {
                continue
            }

            let key = ExplicitImageAssetInputKey(moduleName: item.item.bundleInfo.name, relativeProjectPath: item.item.relativeProjectPath)
            guard inputKeys.contains(key) else {
                out.append(item.item.with(error: CompilerError("Image asset \(item.item.relativeProjectPath) in module \(item.item.bundleInfo.name) was not declared in the explicit image asset manifest")))
                continue
            }

            selectedItemByKey[key] = item
        }

        for asset in manifest.assets where shouldProcess(moduleName: asset.moduleName) {
            let imagePromises = try asset.inputs.map { input -> (SelectedItem<URL>, Promise<ExplicitIdentifiedImage>) in
                let key = ExplicitImageAssetInputKey(moduleName: asset.moduleName, relativeProjectPath: input.relativeProjectPath)
                guard let selectedItem = selectedItemByKey[key] else {
                    throw CompilerError("Image asset manifest references missing input \(input.relativeProjectPath) in module \(asset.moduleName)")
                }
                return (selectedItem, identifyImage(asset: asset, input: input, selectedItem: selectedItem))
            }

            var images = [ExplicitIdentifiedImage]()
            for (selectedItem, imagePromise) in imagePromises {
                do {
                    images.append(try imagePromise.waitForData())
                } catch let error {
                    out.append(selectedItem.item.with(error: error))
                }
            }

            if !images.isEmpty {
                out.append(createImageAssetItem(asset: asset, images: images))
            }
        }

        return out
    }

    func process(items: CompilationItems) throws -> CompilationItems {
        return try items.select { item -> URL? in
            if case .rawResource(let file) = item.kind, case .url(let url) = file, FileExtensions.exportedImages.contains(url.pathExtension) {
                return url
            }
            return nil
        }
        .transformAll(identifyAssets)
    }
}
