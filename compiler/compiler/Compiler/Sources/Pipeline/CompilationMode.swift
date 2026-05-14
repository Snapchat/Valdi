// Copyright © 2024 Snap, Inc. All rights reserved.

import Foundation
import Yams

enum CompilationMode {
    case jsBytecode
    case js
    case native

    static func from(str: String) throws -> CompilationMode {
        switch str.trimmed {
        case "js":
            return .js
        case "js_bytecode":
            return .jsBytecode
        case "native":
            return .native
        default:
            throw CompilerError("Invalid compilation mode '\(str)'. Expecting 'js', 'js_bytecode' or 'native'")
        }
    }
}

/**
 Holds the set of inclusion rules that helps resolving the compilation mode to use of each item within a module.
 */
struct CompilationModeConfig {
    let js: InclusionConfig?
    let jsBytecode: InclusionConfig?
    let native: InclusionConfig?

    /**
     Compilation mode config that will use JS output  for every item in the module
     */
    static func forJs() -> CompilationModeConfig {
        return CompilationModeConfig(js: InclusionConfig.alwaysIncluded, jsBytecode: nil, native: nil)
    }

    /**
     Compilation mode config that will use JS bytecode output  for every item in the module
     */
    static func forJsBytecode() -> CompilationModeConfig {
        return CompilationModeConfig(js: nil, jsBytecode: InclusionConfig.alwaysIncluded, native: nil)
    }

    /**
     Compilation mode config that will use native output  for every item in the module
     */
    static func forNative() -> CompilationModeConfig {
        return CompilationModeConfig(js: nil, jsBytecode: nil, native: InclusionConfig.alwaysIncluded)
    }

    static func parse(from node: Yams.Node) throws -> CompilationModeConfig {
        if let str = node.string {
            switch try CompilationMode.from(str: str) {
            case .js:
                return CompilationModeConfig.forJs()
            case .jsBytecode:
                return CompilationModeConfig.forJsBytecode()
            case .native:
                return CompilationModeConfig.forNative()
            }
        }

        guard let mapping = node.mapping else {
            throw CompilerError("Expected string or mapping for 'compilation_mode' config")
        }

        var jsInclusionConfig: InclusionConfig?
        var jsBytecodeInclusionConfig: InclusionConfig?
        var nativeInclusionConfig: InclusionConfig?

        for (key, value) in mapping {
            guard let compilationModeStr = key.string else {
                throw CompilerError("Each entry in the 'compilation_mode' mapping should have a string key")
            }

            switch try CompilationMode.from(str: compilationModeStr) {
            case .js:
                jsInclusionConfig = try InclusionConfig.parse(from: value)
            case .jsBytecode:
                jsBytecodeInclusionConfig = try InclusionConfig.parse(from: value)
            case .native:
                nativeInclusionConfig = try InclusionConfig.parse(from: value)
            }
        }

        return CompilationModeConfig(js: jsInclusionConfig, jsBytecode: jsBytecodeInclusionConfig, native: nativeInclusionConfig)
    }

    /// Returns the compilation mode string for BUILD.bazel if this is a simple (non-pattern-based) config,
    /// or nil for complex pattern-based configs.
    var asBazelString: String? {
        switch (js, jsBytecode, native) {
        case (.some, nil, nil): return "js"
        case (nil, .some, nil): return "js_bytecode"
        case (nil, nil, .some): return "native"
        default: return nil
        }
    }

    /**
     Resolves the compilation mode to use for the item at the given path.
     Will use native if one of the include patterns matches the item path.
     Will use js if one of the include patterns matches the item path, or if the js bytecode exclude patterns matches
     Will use js bytecode in all other scenarios.
     */
    func resolveCompilationMode(forItemPath itemPath: String) -> CompilationMode {
        if let native {
            if native.resolve(itemPath) == .included {
                return .native
            }
        }

        if let js {
            if js.resolve(itemPath) == .included {
                return .js
            }
        }

        if let jsBytecode {
            switch jsBytecode.resolve(itemPath) {
            case .included:
                return .jsBytecode
            case .excluded:
                // If jsBytecode is specifically excluded, we use js
                return .js
            case .unknown:
                break
            }
        }

        // Use jsBytecode by default

        return .jsBytecode
    }
}
