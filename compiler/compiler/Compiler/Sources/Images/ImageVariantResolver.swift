//
//  ImageVariantResolver.swift
//  
//
//  Created by Simon Corsin on 2/12/20.
//

import Foundation

struct ResolvedImageVariant {
    let variant: ImageVariantSpecs
    let assetIdentifier: ImageAssetIdentifier
}

class ImageVariantResolver {

    private static let iosVariantSpecs = [
        ImageVariantSpecs(filenamePattern: "$file@2x.png", scale: 2.0, platform: .ios),
        ImageVariantSpecs(filenamePattern: "$file@3x.png", scale: 3.0, platform: .ios)
    ]

    private static let androidVariantSpecs = [
        ImageVariantSpecs(filenamePattern: "drawable-mdpi/$file.webp", scale: 1.0, platform: .android),
        ImageVariantSpecs(filenamePattern: "drawable-hdpi/$file.webp", scale: 1.5, platform: .android),
        ImageVariantSpecs(filenamePattern: "drawable-xhdpi/$file.webp", scale: 2.0, platform: .android),
        ImageVariantSpecs(filenamePattern: "drawable-xxhdpi/$file.webp", scale: 3.0, platform: .android),
        ImageVariantSpecs(filenamePattern: "drawable-xxxhdpi/$file.webp", scale: 4.0, platform: .android)
    ]

    private static let webVariantSpecs = [
        ImageVariantSpecs(filenamePattern: "$file.png", scale: 3.0, platform: .web)
    ]

    private static let svgVariantSpecs = ImageVariantSpecs(filenamePattern: "$file.svg", scale: 1.0, platform: nil)

    static let allExportedVariantSpecs = iosVariantSpecs + androidVariantSpecs + webVariantSpecs
    static let allSupportedVariantSpecs = allExportedVariantSpecs + [svgVariantSpecs]

    /// Returns only the exported variant specs for the given platform enablement flags.
    /// Falls back to `allExportedVariantSpecs` when all three are enabled (preserves existing behaviour).
    static func exportedVariantSpecs(android: Bool, ios: Bool, web: Bool) -> [ImageVariantSpecs] {
        if android && ios && web {
            return allExportedVariantSpecs
        }
        var specs = [ImageVariantSpecs]()
        if ios { specs += iosVariantSpecs }
        if android { specs += androidVariantSpecs }
        if web { specs += webVariantSpecs }
        return specs
    }

    static func resolveVariant(imageURL: URL, relativeProjectPath: String) throws -> ResolvedImageVariant {
        for variant in allSupportedVariantSpecs {
            if let assetIdentifier = variant.matches(fileURL: imageURL, relativeProjectPath: relativeProjectPath) {
                return ResolvedImageVariant(variant: variant, assetIdentifier: assetIdentifier)
            }
        }
        
        let supportedPatterns = allSupportedVariantSpecs.map { $0.filenamePattern }

        throw CompilerError("Could not resolve image variant at file path: \(imageURL.path).\nIn order to use an image it must match one of the filename patterns:\n\(supportedPatterns.joined(separator: "\n"))")
    }

}
