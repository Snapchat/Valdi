// Copyright © 2026 Snap, Inc. All rights reserved.

import Foundation

struct RustNativeSourceGenerator {
    private init() {}

    static func generate(
        bundleInfo: CompilationItem.BundleInfo,
        cppType: CPPType,
        sourceFileName: GeneratedSourceFilename,
        files: RustGeneratedFiles
    ) -> [NativeSource] {
        let sourceComment =
            "// Generated from type '\(sourceFileName.symbolName)' at '\(sourceFileName.filename)' by the Valdi compiler.\n"
        var sources = [
            NativeSource(
                relativePath: cppType.includeDir,
                filename: "\(cppType.declaration.name).rs",
                file: .string(sourceComment + files.rust),
                groupingIdentifier: "\(bundleInfo.name).rs",
                groupingPriority: 0),
            NativeSource(
                relativePath: cppType.includeDir,
                filename: "\(cppType.declaration.name)_ffi.h",
                file: .string(files.cHeader),
                groupingIdentifier: "\(bundleInfo.name)_ffi.h",
                groupingPriority: 0),
        ]
        sources.append(
            NativeSource(
                relativePath: cppType.includeDir,
                filename: "\(cppType.declaration.name)_rust_link_apple.params",
                file: .string(files.registrationSymbols.map { "-Wl,-u,_\($0)\n" }.joined()),
                groupingIdentifier: "\(bundleInfo.name)_rust_link_apple.params",
                groupingPriority: 0))
        sources.append(
            NativeSource(
                relativePath: cppType.includeDir,
                filename: "\(cppType.declaration.name)_rust_link_elf.params",
                file: .string(files.registrationSymbols.map { "-Wl,-u,\($0)\n" }.joined()),
                groupingIdentifier: "\(bundleInfo.name)_rust_link_elf.params",
                groupingPriority: 0))
        return sources
    }
}
