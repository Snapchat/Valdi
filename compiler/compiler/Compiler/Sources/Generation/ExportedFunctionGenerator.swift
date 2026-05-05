//
//  ExportedFunctionGenerator.swift
//  BlueSocket
//
//  Created by saniul on 25/03/2019.
//

import Foundation

struct ExportedFunction {
    let containingIosType: IOSType?
    let containingAndroidTypeName: String?
    let containingCppType: CPPType?

    let functionName: String
    let parameters: [ValdiModelProperty]
    let returnType: ValdiModelPropertyType
    let allowSyncCall: Bool
    let comments: String?
}

final class ExportedFunctionGenerator: NativeSourceGenerator {

    private let exportedFunction: ExportedFunction
    private let modulePath: String

    init(exportedFunction: ExportedFunction, modulePath: String) {
        self.exportedFunction = exportedFunction
        self.modulePath = modulePath
    }

    func generateSwiftSources(parameters: NativeSourceParameters, type: IOSType) throws -> [NativeSource] {
        let iosGenerator = SwiftFunctionGenerator(iosType: type,
                                                  exportedFunction: exportedFunction,
                                                  classMapping: parameters.classMapping,
                                                  sourceFileName: parameters.sourceFileName,
                                                  bundleName: parameters.bundleInfo.name,
                                                  modulePath: modulePath,
                                                  bundleInfo: parameters.bundleInfo)

        return try iosGenerator.write()
    }

    func generateObjCSources(parameters: NativeSourceParameters, type: IOSType) throws -> [NativeSource] {
        let iosGenerator = ObjCFunctionGenerator(iosType: type,
                                                 exportedFunction: exportedFunction,
                                                 classMapping: parameters.classMapping,
                                                 sourceFileName: parameters.sourceFileName,
                                                 bundleName: parameters.bundleInfo.name,
                                                 modulePath: modulePath,
                                                 bundleInfo: parameters.bundleInfo)

        return try iosGenerator.write()
    }

    func generateKotlinSources(parameters: NativeSourceParameters, fullTypeName: String) throws -> [NativeSource] {
        let androidGenerator = KotlinFunctionGenerator(bundleInfo: parameters.bundleInfo,
                                                       fullTypeName: fullTypeName,
                                                       exportedFunction: exportedFunction,
                                                       classMapping: parameters.classMapping,
                                                       sourceFileName: parameters.sourceFileName,
                                                       modulePath: modulePath)

        return try androidGenerator.write()
    }

    func generateCppSources(parameters: NativeSourceParameters, cppType: CPPType) throws -> [NativeSource] {
        let cppGenerator = CppFunctionGenerator(cppType: cppType,
                                                exportedFunction: exportedFunction,
                                                classMapping: parameters.classMapping,
                                                sourceFileName: parameters.sourceFileName,
                                                modulePath: modulePath,
                                                bundleInfo: parameters.bundleInfo)
        return try cppGenerator.write()
    }
}
