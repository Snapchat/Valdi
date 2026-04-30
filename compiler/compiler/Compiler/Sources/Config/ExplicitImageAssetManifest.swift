// Copyright © 2026 Snap, Inc. All rights reserved.

import Foundation

struct ExplicitImageAssetManifestInput: Codable {
    let file: String
    let relativeProjectPath: String
    let filenamePattern: String
    let scale: Double
    let platform: Platform?
}

struct ExplicitImageAssetManifestAsset: Codable {
    let moduleName: String
    let assetName: String
    let relativeProjectAssetDirectoryPath: String
    let inputs: [ExplicitImageAssetManifestInput]
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
                                               platform: platform)
    }
}

extension ExplicitImageAssetManifestAsset {
    func resolvingVariables(_ variables: [String: String]) throws -> ExplicitImageAssetManifestAsset {
        return ExplicitImageAssetManifestAsset(moduleName: moduleName,
                                               assetName: assetName,
                                               relativeProjectAssetDirectoryPath: try relativeProjectAssetDirectoryPath.resolvingVariables(variables),
                                               inputs: try inputs.map { try $0.resolvingVariables(variables) })
    }
}

extension ExplicitImageAssetManifest {
    func resolvingVariables(_ variables: [String: String]) throws -> ExplicitImageAssetManifest {
        return ExplicitImageAssetManifest(assets: try assets.map { try $0.resolvingVariables(variables) })
    }
}
