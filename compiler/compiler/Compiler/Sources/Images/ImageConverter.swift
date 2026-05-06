//
//  ImageConverter.swift
//  Compiler
//
//  Created by David Byttow on 10/23/18.
//  Copyright © 2018 Snap Inc. All rights reserved.
//

import Foundation

final class ImageConverter {

    let logger: ILogger
    let fileManager: ValdiFileManager
    let projectConfig: ValdiProjectConfig
    let imageToolbox: ImageToolbox

    init(logger: ILogger, fileManager: ValdiFileManager, projectConfig: ValdiProjectConfig, imageToolbox: ImageToolbox) {
        self.logger = logger
        self.fileManager = fileManager
        self.projectConfig = projectConfig
        self.imageToolbox = imageToolbox
    }

    struct ImageConversionInfo {
        let renderSize: ImageSize
        let outputSize: ImageSize
        let density: String
    }

    private func computeExtent(desiredExtent: Int, scale: Double) -> Int {
        let divider = Int(scale)
        let remainder = desiredExtent % divider

        return desiredExtent + remainder
    }

    public func getConversionInfo(sourceImage: ImageAssetVariant, targetVariantSpecs: ImageVariantSpecs) -> ImageConversionInfo {
        let resolvedScale = 1.0 / sourceImage.variantSpecs.scale * targetVariantSpecs.scale
        let targetSize = sourceImage.imageInfo.size.scaled(resolvedScale)

        let density = String(Int(resolvedScale * 100)) + "%"

        let extentWidth = computeExtent(desiredExtent: targetSize.width, scale: targetVariantSpecs.scale)
        let extentHeight = computeExtent(desiredExtent: targetSize.height, scale: targetVariantSpecs.scale)

        return ImageConversionInfo(renderSize: targetSize, outputSize: ImageSize(width: extentWidth, height: extentHeight), density: density)
    }

    func dependenciesVersions() throws -> [String: String] {
        return [
            "image_toolbox": try imageToolbox.getVersion()
        ]
    }

    private func optimizePNG(outputFileURL: URL) throws {
        guard outputFileURL.pathExtension == "png" else {
            return
        }

        try imageToolbox.optimizePNG(inputPath: outputFileURL.path)
    }

    public func convert(imageInfo: ImageInfo, filePath: String, outputFileURL: URL, conversionInfo: ImageConversionInfo) throws -> ImageInfo {
        if conversionInfo.outputSize != conversionInfo.renderSize {
            logger.warn("Image conversion outputSize doesn't match renderSize: \(filePath) -> \(outputFileURL.path)\nThis usually means that the original image is not divisible by its display scale (e.g. @3x image with the size of 62x47).")
        }

        try fileManager.createDirectory(at: outputFileURL.deletingLastPathComponent())

        var qualityRatio: Double?
        if outputFileURL.pathExtension == "webp" {
            // matching the default quality value previously used by cwebp
            qualityRatio = 0.75
        }
        try imageToolbox.convert(inputPath: filePath,
                                 outputPath: outputFileURL.path,
                                 outputWidth: conversionInfo.outputSize.width,
                                 outputHeight: conversionInfo.outputSize.height,
                                 qualityRatio: qualityRatio)

        if outputFileURL.pathExtension == "png" {
            try optimizePNG(outputFileURL: outputFileURL)
        }

        return ImageInfo(size: conversionInfo.outputSize)
    }

}
