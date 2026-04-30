// Copyright © 2024 Snap, Inc. All rights reserved.

final class ExportedModuleGenerator: NativeSourceGenerator {

    private let exportedModule: ExportedModule

    init(bundleInfo: CompilationItem.BundleInfo, exportedModule: ExportedModule) {
        self.exportedModule = exportedModule
    }

    func generateSwiftSources(parameters: NativeSourceParameters, type: IOSType) throws -> [NativeSource] {
        return []
    }

    func generateObjCSources(parameters: NativeSourceParameters, type: IOSType) throws -> [NativeSource] {
        let iosGenerator = ObjCModuleGenerator(iosType: type,
                                               exportedModule: exportedModule,
                                               bundleInfo: parameters.bundleInfo,
                                               classMapping: parameters.classMapping,
                                               sourceFileName: parameters.sourceFileName)
        return try iosGenerator.write()
    }

    func generateKotlinSources(parameters: NativeSourceParameters, fullTypeName: String) throws -> [NativeSource] {
        let androidGenerator = KotlinModuleGenerator(bundleInfo: parameters.bundleInfo,
                                                     fullTypeName: fullTypeName,
                                                     exportedModule: exportedModule,
                                                     classMapping: parameters.classMapping,
                                                     sourceFileName: parameters.sourceFileName)

        return try androidGenerator.write()
    }

    func generateCppSources(parameters: NativeSourceParameters, cppType: CPPType) throws -> [NativeSource] {
        let cppGenerator = CppModuleGenerator(bundleInfo: parameters.bundleInfo,
                                              cppType: cppType,
                                              exportedModule: exportedModule,
                                              classMapping: parameters.classMapping,
                                              sourceFileName: parameters.sourceFileName)
        return try cppGenerator.write()
    }
}
