//
//  ExportedEnumGenerator.swift
//  BlueSocket
//
//  Created by saniul on 25/03/2019.
//

import Foundation

struct ExportedEnum {
    let iosType: IOSType?
    let androidTypeName: String?
    let cppType: CPPType?

    enum Cases {
        case `enum`([EnumCase<Int>])
        case stringEnum([EnumCase<String>])
    }
    let cases: Cases
    let comments: String?
}

final class ExportedEnumGenerator: NativeSourceGenerator {

    private let exportedEnum: ExportedEnum

    init(exportedEnum: ExportedEnum) {
        self.exportedEnum = exportedEnum
    }

    func generateSwiftSources(parameters: NativeSourceParameters, type: IOSType) throws -> [NativeSource] {
        let iosGenerator = SwiftEnumGenerator(iosType: type,
                                             exportedEnum: exportedEnum,
                                              classMapping: parameters.classMapping,
                                              sourceFileName: parameters.sourceFileName,
                                              bundleInfo: parameters.bundleInfo)

        return try iosGenerator.write()
    }

    func generateObjCSources(parameters: NativeSourceParameters, type: IOSType) throws -> [NativeSource] {
        let iosGenerator = ObjCEnumGenerator(iosType: type,
                                             exportedEnum: exportedEnum,
                                             classMapping: parameters.classMapping,
                                             sourceFileName: parameters.sourceFileName,
                                             bundleInfo: parameters.bundleInfo)

        return try iosGenerator.write()
    }

    func generateKotlinSources(parameters: NativeSourceParameters, fullTypeName: String) throws -> [NativeSource] {
        let androidGenerator = KotlinEnumGenerator(bundleInfo: parameters.bundleInfo,
                                                   fullTypeName: fullTypeName,
                                                   exportedEnum: exportedEnum,
                                                   classMapping: parameters.classMapping,
                                                   sourceFileName: parameters.sourceFileName)

        return try androidGenerator.write()
    }

    func generateCppSources(parameters: NativeSourceParameters, cppType: CPPType) throws -> [NativeSource] {
        let cppGenerator = CppEnumGenerator(exportedEnum: exportedEnum,
                                            cppType: cppType,
                                            bundleInfo: parameters.bundleInfo,
                                            sourceFileName: parameters.sourceFileName)
        return try cppGenerator.write()
    }
}
