// Copyright © 2026 Snap, Inc. All rights reserved.

import Foundation

struct ExplicitImageAssetManifestSize: Codable {
    let width: Int
    let height: Int
}

struct ExplicitImageAssetManifestInput: Codable {
    let file: String
    let relativeProjectPath: String
    let filenamePattern: String
    let scale: Double
    let platform: Platform?
    /// Pixel dimensions of the image, populated by `ValdiProcessImages` so that
    /// `ValdiCompile` can skip the toolbox subprocess when reading the manifest.
    let size: ExplicitImageAssetManifestSize?
}

struct ExplicitImageAssetManifestOutput: Codable {
    let filenamePattern: String
    let scale: Double
    let platform: Platform?
    /// Path of the pre-generated output file, relative to the manifest's base URL.
    /// When populated, the compiler reads the file from disk instead of generating
    /// the variant in-process; populated by Bazel when ValdiProcessImages produces
    /// the files in a separate action.
    let file: String?
}

struct ExplicitImageAssetManifestAsset: Codable {
    let moduleName: String
    let assetName: String
    let relativeProjectAssetDirectoryPath: String
    let inputs: [ExplicitImageAssetManifestInput]
    let outputs: [ExplicitImageAssetManifestOutput]
}

struct ExplicitImageAssetManifest: Codable {
    let assets: [ExplicitImageAssetManifestAsset]
}

extension ExplicitImageAssetManifestInput {
    func resolvingVariables(_ variables: [String: String]) throws -> ExplicitImageAssetManifestInput {
        return ExplicitImageAssetManifestInput(file: try file.resolvingVariables(variables),
                                               relativeProjectPath: try relativeProjectPath.resolvingVariables(variables),
                                               filenamePattern: filenamePattern,
                                               scale: scale,
                                               platform: platform,
                                               size: size)
    }
}

extension ExplicitImageAssetManifestOutput {
    func resolvingVariables(_ variables: [String: String]) throws -> ExplicitImageAssetManifestOutput {
        return ExplicitImageAssetManifestOutput(filenamePattern: filenamePattern,
                                                scale: scale,
                                                platform: platform,
                                                file: try file?.resolvingVariables(variables))
    }
}

extension ExplicitImageAssetManifestAsset {
    func resolvingVariables(_ variables: [String: String]) throws -> ExplicitImageAssetManifestAsset {
        return ExplicitImageAssetManifestAsset(moduleName: moduleName,
                                               assetName: assetName,
                                               relativeProjectAssetDirectoryPath: try relativeProjectAssetDirectoryPath.resolvingVariables(variables),
                                               inputs: try inputs.map { try $0.resolvingVariables(variables) },
                                               outputs: try outputs.map { try $0.resolvingVariables(variables) })
    }
}

extension ExplicitImageAssetManifest {
    func resolvingVariables(_ variables: [String: String]) throws -> ExplicitImageAssetManifest {
        return ExplicitImageAssetManifest(assets: try assets.map { try $0.resolvingVariables(variables) })
    }
}
