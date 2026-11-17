// Copyright © 2026 Snap, Inc. All rights reserved.

final class RustModelGenerator {
    private let model: ValdiModel
    private let cppType: CPPType
    private let bundleInfo: CompilationItem.BundleInfo
    private let sourceFileName: GeneratedSourceFilename

    init(
        model: ValdiModel,
        cppType: CPPType,
        bundleInfo: CompilationItem.BundleInfo,
        classMapping: ResolvedClassMapping,
        sourceFileName: GeneratedSourceFilename
    ) {
        self.model = model
        self.cppType = cppType
        self.bundleInfo = bundleInfo
        self.sourceFileName = sourceFileName
        _ = classMapping
    }

    func write() throws -> [NativeSource] {
        if model.exportAsInterface {
            return try RustProxyGenerator(
                model: model,
                cppType: cppType,
                bundleInfo: bundleInfo,
                sourceFileName: sourceFileName).write()
        }
        return try RustValueModelGenerator(
            model: model,
            cppType: cppType,
            bundleInfo: bundleInfo,
            sourceFileName: sourceFileName).write()
    }
}
