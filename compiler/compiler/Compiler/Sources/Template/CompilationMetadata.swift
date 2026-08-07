// Copyright © 2024 Snap, Inc. All rights reserved.

import Foundation

struct CompilationMetadata: Codable {
    let nativeApiMinVersion: String?

    let classMappings: [String: ValdiClass]
    let nativeTypes: SerializedTypeScriptNativeTypeResolver
    let generatedTypes: [GeneratedTypesSummary]

    init(classMappings: [String: ValdiClass],
         nativeTypes: SerializedTypeScriptNativeTypeResolver,
         nativeApiMinVersion: String?,
         generatedTypes: [GeneratedTypesSummary]) {
        self.nativeApiMinVersion = nativeApiMinVersion
        self.classMappings = classMappings
        self.nativeTypes = nativeTypes
        self.generatedTypes = generatedTypes
    }

    enum CodingKeys: CodingKey {
        case nativeApiMinVersion
        case classMappings
        case nativeTypes
        case generatedTypes
    }

    init(from decoder: Decoder) throws {
        let container = try decoder.container(keyedBy: CodingKeys.self)
        nativeApiMinVersion = try container.decodeIfPresent(String.self, forKey: .nativeApiMinVersion)
        classMappings = try container.decode([String: ValdiClass].self, forKey: .classMappings)
        nativeTypes = try container.decode(SerializedTypeScriptNativeTypeResolver.self, forKey: .nativeTypes)
        // Compilation metadata is decoded only to restore class mappings and native type resolver
        // entries from dependencies. Generated type descriptions are an output-only API snapshot,
        // so decoding intentionally does not round-trip them.
        generatedTypes = []
    }

    func encode(to encoder: Encoder) throws {
        var container = encoder.container(keyedBy: CodingKeys.self)
        try container.encode(nativeApiMinVersion, forKey: .nativeApiMinVersion)
        try container.encode(classMappings, forKey: .classMappings)
        try container.encode(nativeTypes, forKey: .nativeTypes)
        try container.encode(generatedTypes, forKey: .generatedTypes)
    }
}
