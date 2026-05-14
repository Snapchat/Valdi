// Copyright © 2026 Snap, Inc. All rights reserved.

import Foundation

/// Generates the per-variant output image files declared in an
/// `ExplicitImageAssetManifest`. Used by the compiler's
/// `--image-processing-only` mode to back the Bazel `ValdiProcessImages` action.
///
/// For each asset:
///   - Build the input variant list from the manifest's `inputs[]`.
///   - Pick the best variant (highest scale, prefer SVG).
///   - For each manifest `outputs[]` entry: if a matching input variant exists
///     by identifier, copy it to the declared output path; otherwise resize/encode
///     from the best variant via `ImageConverter`.
///
/// The class does not use `CompilationPipeline` / `CompilationItem` — it operates
/// directly on the manifest, which keeps the image-only invocation independent of
/// non-image inputs (TS, Vue, etc.) so the Bazel action's cache key only depends
/// on the image sources.
final class ExplicitImageAssetGenerator {

    private let logger: ILogger
    private let fileManager: ValdiFileManager
    private let imageToolbox: ImageToolbox
    private let imageConverter: ImageConverter

    init(logger: ILogger, fileManager: ValdiFileManager, imageToolbox: ImageToolbox, imageConverter: ImageConverter) {
        self.logger = logger
        self.fileManager = fileManager
        self.imageToolbox = imageToolbox
        self.imageConverter = imageConverter
    }

    /// Processes all assets in the manifest and returns an updated copy with pixel
    /// dimensions embedded in each input entry so that `ValdiCompile` can skip the
    /// toolbox subprocess entirely when reading image info.
    func process(manifest: ExplicitImageAssetManifest, baseURL: URL) throws -> ExplicitImageAssetManifest {
        let updatedAssets = try manifest.assets.map { try processAsset(asset: $0, baseURL: baseURL) }
        return ExplicitImageAssetManifest(assets: updatedAssets)
    }

    private func processAsset(asset: ExplicitImageAssetManifestAsset, baseURL: URL) throws -> ExplicitImageAssetManifestAsset {
        if asset.assetName.lowercased() != asset.assetName {
            throw CompilerError("Invalid filename '\(asset.assetName)' for module '\(asset.moduleName)', image filenames must be lowercased")
        }

        guard !asset.inputs.isEmpty else {
            throw CompilerError("Image asset '\(asset.assetName)' in module '\(asset.moduleName)' has no inputs")
        }

        let inputs = asset.inputs.map { input -> ResolvedInput in
            let specs = ImageVariantSpecs(filenamePattern: input.filenamePattern, scale: input.scale, platform: input.platform)
            return ResolvedInput(specs: specs, fileURL: baseURL.appendingPathComponent(input.file))
        }

        let bestInput = ExplicitImageAssetGenerator.bestInput(among: inputs)
        let bestInfo = try imageToolbox.getInfo(inputPath: bestInput.fileURL.path)
        let bestImageInfo = ImageInfo(size: ImageSize(width: bestInfo.width, height: bestInfo.height))
        let bestVariant = ImageAssetVariant(imageInfo: bestImageInfo, file: .url(bestInput.fileURL), variantSpecs: bestInput.specs)

        let inputsByIdentifier = Dictionary(inputs.map { ($0.specs.identifier, $0) }, uniquingKeysWith: { first, _ in first })

        for output in asset.outputs {
            guard let outputFile = output.file else {
                throw CompilerError("Manifest output for asset '\(asset.assetName)' in module '\(asset.moduleName)' is missing the `file` path required by --image-processing-only")
            }
            let outputURL = baseURL.appendingPathComponent(outputFile)
            let targetSpecs = ImageVariantSpecs(filenamePattern: output.filenamePattern, scale: output.scale, platform: output.platform)

            if let matchingInput = inputsByIdentifier[targetSpecs.identifier] {
                try copyInputToOutput(inputURL: matchingInput.fileURL, outputURL: outputURL)
            } else {
                let conversionInfo = imageConverter.getConversionInfo(sourceImage: bestVariant, targetVariantSpecs: targetSpecs)
                _ = try imageConverter.convert(imageInfo: bestImageInfo, filePath: bestInput.fileURL.path, outputFileURL: outputURL, conversionInfo: conversionInfo)
            }
        }

        // Embed pixel dimensions for every input so ValdiCompile can skip the toolbox.
        let updatedInputs = try asset.inputs.map { input -> ExplicitImageAssetManifestInput in
            let fileURL = baseURL.appendingPathComponent(input.file)
            let info = fileURL == bestInput.fileURL ? bestInfo : try imageToolbox.getInfo(inputPath: fileURL.path)
            return ExplicitImageAssetManifestInput(
                file: input.file,
                relativeProjectPath: input.relativeProjectPath,
                filenamePattern: input.filenamePattern,
                scale: input.scale,
                platform: input.platform,
                size: ExplicitImageAssetManifestSize(width: info.width, height: info.height)
            )
        }

        return ExplicitImageAssetManifestAsset(
            moduleName: asset.moduleName,
            assetName: asset.assetName,
            relativeProjectAssetDirectoryPath: asset.relativeProjectAssetDirectoryPath,
            inputs: updatedInputs,
            outputs: asset.outputs
        )
    }

    struct ResolvedInput {
        let specs: ImageVariantSpecs
        let fileURL: URL
    }

    /// Pick the highest-quality input. Mirrors `ImageAsset.findHighestVariant`:
    /// prefer SVG; otherwise highest scale, breaking ties by preferring iOS.
    static func bestInput(among inputs: [ResolvedInput]) -> ResolvedInput {
        if let svg = inputs.first(where: { $0.specs.fileExtension == FileExtensions.svg }) {
            return svg
        }
        return inputs.max { left, right in
            if left.specs.scale != right.specs.scale {
                return left.specs.scale < right.specs.scale
            }
            return platformPreferenceScore(left.specs.platform) < platformPreferenceScore(right.specs.platform)
        }!
    }

    // Strict-weak-ordering-compatible tie-break for inputs at the same scale.
    private static func platformPreferenceScore(_ platform: Platform?) -> Int {
        return platform == .ios ? 1 : 0
    }

    private func copyInputToOutput(inputURL: URL, outputURL: URL) throws {
        try fileManager.createDirectory(at: outputURL.deletingLastPathComponent())
        if FileManager.default.fileExists(atPath: outputURL.path) {
            try FileManager.default.removeItem(at: outputURL)
        }
        try FileManager.default.copyItem(at: inputURL, to: outputURL)
    }
}
